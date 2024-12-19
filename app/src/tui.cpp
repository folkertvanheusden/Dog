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

void display(const libchess::Position & p, const bool large, const bool colors)
{
	if (!large) {
		p.display();
		return;
	}

	if (colors) {
		printf("\x1b[0m");
		printf("\x1b[43;30m    ");
		for(int x=0; x<8; x++)
			printf("   ");
		printf(" \x1b[0m\n");
	}

	for(int y=7; y>=0; y--) {
		if (colors)
			printf("\x1b[43;30m %c |", y + '1');
		else
			printf(" %c |", y + '1');
		for(int x=0; x<8; x++) {
			const auto piece = p.piece_on(libchess::Square::from(libchess::File(x), libchess::Rank(y)).value());
			if (piece.has_value()) {
				int  c        = piece.value().type().to_char();
				bool is_white = piece.value().color() == libchess::constants::WHITE;
				if (colors) {
					if (is_white)
						printf("\x1b[30;47m");
					else
						printf("\x1b[40;37m");
				}
				printf(" %c ", piece.value().color() == libchess::constants::WHITE ? toupper(c) : c);
				if (colors)
					printf("\x1b[43;30m");
			}
			else {
				printf("   ");
			}
		}
		printf(" \x1b[0m\n");
	}
	if (colors) {
		printf("\x1b[43;30m   +");
		for(int x=0; x<8; x++)
			printf("---");
		printf(" \x1b[0m\n");
		printf("\x1b[43;30m    ");
		for(int x=0; x<8; x++)
			printf(" %c ", 'A' + x);
		printf(" \x1b[0m\n");
		printf("\x1b[0m");
	}
	else {
		printf("   +");
		for(int x=0; x<8; x++)
			printf("---");
		printf("\n");
		printf("    ");
		for(int x=0; x<8; x++)
			printf(" %c ", 'A' + x);
		printf("\n");
	}
}

bool colors        = false;
bool default_trace = false;
int  think_time    = 1000;  // milliseconds
bool do_ponder     = false;

void write_settings()
{
#if defined(ESP32)
	FILE *fh = fopen("/spiffs/settings.dat", "w");
	if (!fh) {
		fprintf(stderr, "Cannot write settings\n");
		return;
	}

	fprintf(fh, "%d\n", colors);
	fprintf(fh, "%d\n", default_trace);
	fprintf(fh, "%d\n", think_time);
	fprintf(fh, "%d\n", do_ponder);

	fclose(fh);
#endif
}

void load_settings()
{
#if defined(ESP32)
	FILE *fh = fopen("/spiffs/settings.dat", "r");
	if (!fh)
		return;

	char buffer[16] { };
	fgets(buffer, sizeof buffer, fh);
	colors        = atoi(buffer);
	fgets(buffer, sizeof buffer, fh);
	default_trace = atoi(buffer);
	fgets(buffer, sizeof buffer, fh);
	think_time    = atoi(buffer);
	fgets(buffer, sizeof buffer, fh);
	do_ponder     = atoi(buffer);

	fclose(fh);
#endif
}

void tui()
{
	load_settings();

	set_thread_name("TUI");

	if (do_ponder)
		set_new_ponder_position(true);

	libchess::Color player = positiont1.side_to_move();

	trace_enabled = default_trace;
	i.set_local_echo(true);

	for(;;) {
		display(positiont1, true, colors);

		bool finished = positiont1.game_state() != libchess::Position::GameState::IN_PROGRESS;
		if (player == positiont1.side_to_move() || finished) {
			if (do_ponder)
				set_new_ponder_position(true);

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
				printf("colors  on/off\n");
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
			else if (parts[0] == "time" && parts.size() == 2) {
				think_time = std::stod(parts[1]) * 1000;
				write_settings();
			}
			else if (parts[0] == "trace") {
				if (parts.size() == 2)
					trace_enabled = parts[1] == "on";
				else
					trace_enabled = !trace_enabled;
				default_trace = trace_enabled;
				write_settings();
				printf("Tracing is now %senabled\n", trace_enabled ? "":"not ");
			}
			else if (parts[0] == "colors") {
				if (parts.size() == 2)
					colors = parts[1] == "on";
				else
					colors = !colors;
				write_settings();
				printf("Colors are now %senabled\n", colors ? "":"not ");
			}
			else if (parts[0] == "ponder") {
				bool prev_ponder = do_ponder;
				if (parts.size() == 2)
					do_ponder = parts[1] == "on";
				else
					do_ponder = !do_ponder;
				write_settings();
				printf("Pondering is now %senabled\n", do_ponder ? "":"not ");
				if (prev_ponder && do_ponder == false)
					pause_ponder();
				else if (!prev_ponder && do_ponder)
					set_new_ponder_position(true);
			}
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
			if (do_ponder)
				set_new_ponder_position(false);  // lazy smp

			printf("Color: %s\n", positiont1.side_to_move() == libchess::constants::WHITE ? "white":"black");
			printf("Thinking... (%.3f seconds)\n", think_time / 1000.);
			libchess::Move best_move  { 0 };
			int            best_score { 0 };
			clear_flag(sp1.stop);
			std::tie(best_move, best_score) = search_it(&positiont1, think_time, true, &sp1, -1, 0, { });
			printf("Selected move: %s (score: %d)\n", best_move.to_str().c_str(), best_score);
			positiont1.make_move(best_move);
			printf("\n");

			if (do_ponder)
				set_new_ponder_position(true);  // regular ponder
#if !defined(linux) && !defined(_WIN32)
			stop_blink(led_green_timer, &led_green);
#endif
		}
	}

	i.set_local_echo(false);
	trace_enabled = true;

	set_new_ponder_position(true);
}

void run_tui()
{
	// because of ESP32 stack
	auto th = new std::thread{tui};
	th->join();
	delete th;
}
