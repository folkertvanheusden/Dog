#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <ctype.h>
#include <thread>

#include <libchess/Position.h>

#include "eval.h"
#include "main.h"
#include "main.h"
#include "max-ascii.h"
#include "str.h"
#include "tui.h"


uint64_t do_perft(libchess::Position &pos, int depth)
{
	libchess::MoveList move_list = pos.legal_move_list();
	if (depth == 1)
		return move_list.size();

	uint64_t count     = 0;
	for(const libchess::Move & move: move_list) {
		pos.make_move(move);
		count += do_perft(pos, depth - 1);
		pos.unmake_move();
	}

	return count;
}

void perft(libchess::Position &pos, int depth)
{
	printf("Perft for fen: %s\n", pos.fen().c_str());

	for(int d=1; d<=depth; d++) {
		uint64_t t_start = esp_timer_get_time();
		uint64_t count   = do_perft(pos, d);
		uint64_t t_end   = esp_timer_get_time();
		double   t_diff  = std::max(uint64_t(1), t_end - t_start) / 1000000.;
		printf("%d: %" PRIu64 " (%.3f nps, %.2f seconds)\n", d, count, count / t_diff, t_diff);
	}
}

void display(const libchess::Position & p, const bool large)
{
	if (!large) {
		p.display();
		return;
	}

	for(int y=7; y>=0; y--) {
		printf(" %c |", y + '1');
		for(int x=0; x<8; x++) {
			const auto piece = p.piece_on(libchess::Square::from(libchess::File(x), libchess::Rank(y)).value());
			if (piece.has_value()) {
				int    c     = piece.value().type().to_char();
				printf(" %c ", piece.value().color() == libchess::constants::WHITE ? toupper(c) : c);
			}
			else {
				printf("   ");
			}
		}
		printf("\n");
	}
	printf("   +");
	for(int x=0; x<8; x++)
		printf("---");
	printf("\n");
	printf("    ");
	for(int x=0; x<8; x++)
		printf(" %c ", 'A' + x);
	printf("\n");
}

void tui()
{
	int think_time         = 1000;  // in ms
	libchess::Color player = positiont1.side_to_move();

	set_ponder_lazy();

	trace_enabled = false;
	i.set_local_echo(true);

	for(;;) {
		display(positiont1, true);

		bool finished = positiont1.game_state() != libchess::Position::GameState::IN_PROGRESS;
		if (player == positiont1.side_to_move() || finished) {
			if (finished)
				printf("Game is finished\n");
			else
				printf("Move number: %d, color: %s\n", positiont1.fullmoves(), positiont1.side_to_move() == libchess::constants::WHITE ? "white":"black");

			std::string line;
			printf("> ");
			if (!std::getline(is, line))
				break;
			printf("%s\n", line.c_str());
			if (line.empty())
				continue;

			auto parts = split(line, " ");
			if (parts[0] == "help") {
				printf("quit    stop the tui\n");
				printf("new     restart game\n");
				printf("player  select player (\"white\" or \"black\")\n");
				printf("time    set think time, in seconds\n");
				printf("fen     show fen for current position\n");
				printf("eval    show current evaluation score\n");
				printf("undo    take back last move\n");
				printf("trace   on/off\n");
				printf("perft   run \"perft\" for the given depth\n");
			}
			else if (parts[0] == "quit") {
				break;
			}
			else if (parts[0] == "fen")
				printf("FEN: %s\n", positiont1.fen().c_str());
			else if (parts[0] == "perft" && parts.size() == 2)
				perft(positiont1, std::stoi(parts.at(1)));
			else if (parts[0] == "new") {
				memset(sp1.history, 0x00, history_malloc_size);
				tti.reset();
				positiont1 = libchess::Position(libchess::constants::STARTPOS_FEN);
			}
			else if (parts[0] == "player" && parts.size() == 2) {
				if (parts[1] == "white" || parts[1] == "w")
					player = libchess::constants::WHITE;
				else
					player = libchess::constants::BLACK;
			}
			else if (parts[0] == "time" && parts.size() == 2)
				think_time = std::stod(parts[1]) * 1000;
			else if (parts[0] == "trace" && parts.size() == 2)
				trace_enabled = parts[1] == "on";
			else if (parts[0] == "undo") {
				positiont1.unmake_move();
				player = positiont1.side_to_move();
			}
			else if (parts[0] == "eval") {
				int score = eval(positiont1, *sp1.parameters);
				printf("evaluation score: %d\n", score);
			}
			else if (parts[0] == "dog")
				print_max_ascii();
			else {
				auto move = *libchess::Move::from(parts[0]);
				if (positiont1.is_legal_move(move))
					positiont1.make_move(move);
				else
					printf("Not a valid move nor command (enter \"help\" for command list)\n");
			}
		}
		else {
#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__)
			stop_blink(led_red_timer, &led_red);
			start_blink(led_green_timer);
#endif

			printf("color: %s\n", positiont1.side_to_move() == libchess::constants::WHITE ? "white":"black");
			printf("Thinking... (%.3f seconds)\n", think_time / 1000.);
			libchess::Move best_move  { 0 };
			int            best_score { 0 };
			clear_flag(sp1.stop);
			std::tie(best_move, best_score) = search_it(&positiont1, think_time, true, &sp1, -1, 0, { });
			printf("Selected move: %s (score: %d)\n", best_move.to_str().c_str(), best_score);
			positiont1.make_move(best_move);
			printf("\n");
#if !defined(linux) && !defined(_WIN32)
			stop_blink(led_green_timer, &led_green);
#endif
		}
	}

	i.set_local_echo(false);
	trace_enabled = true;

	restart_ponder();
}

void run_tui()
{
	// because of ESP32 stack
	auto th = new std::thread{tui};
	th->join();
	delete th;
}
