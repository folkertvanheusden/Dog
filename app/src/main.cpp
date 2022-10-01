#include <atomic>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <streambuf>

#include <driver/uart.h>
#include <esp32/rom/uart.h>

#include <esp_task_wdt.h>

#include <esp_timer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <libchess/Position.h>
#include <libchess/UCIService.h>


std::atomic_bool stop  { false };

uint32_t         nodes { 0 };

void think_timeout(void *arg) {
	stop = true;
}

const esp_timer_create_args_t think_timeout_pars = {
            .callback = &think_timeout,
            .arg = nullptr,
            .name = "searchto"
};

esp_timer_handle_t think_timeout_timer;

libchess::Position position { libchess::constants::STARTPOS_FEN };

auto position_handler = [&position](const libchess::UCIPositionParameters & position_parameters) {
	position = libchess::Position { position_parameters.fen() };

	if (!position_parameters.move_list())
		return;

	for (auto & move_str : position_parameters.move_list()->move_list())
		position.make_move(*libchess::Move::from(move_str));
};


// http://www.josuttis.com/libbook/io/inbuf1.hpp.html
class inbuf : public std::streambuf {
  protected:
    /* data buffer:
     * - at most, four characters in putback area plus
     * - at most, six characters in ordinary read buffer
     */
    static const int bufferSize = 10;    // size of the data buffer
    char buffer[bufferSize];             // data buffer

  public:
    /* constructor
     * - initialize empty data buffer
     * - no putback area
     * => force underflow()
     */
    inbuf() {
        setg (buffer+4,     // beginning of putback area
              buffer+4,     // read position
              buffer+4);    // end position
    }

  protected:
    // insert new characters into the buffer
    virtual int_type underflow () {

        // is read position before end of buffer?
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }

        /* process size of putback area
         * - use number of characters read
         * - but at most four
         */
        int numPutback;
        numPutback = gptr() - eback();
        if (numPutback > 4) {
            numPutback = 4;
        }

        /* copy up to four characters previously read into
         * the putback buffer (area of first four characters)
         */
        std::memmove (buffer+(4-numPutback), gptr()-numPutback,
                      numPutback);

        // read new characters
	int c = 0;

	for(;;) {
		c = fgetc(stdin);

		if (c >= 0)
			break;

		vTaskDelay(1);
	}

	buffer[4] = c;

	int num = 1;

        // reset buffer pointers
        setg (buffer+(4-numPutback),   // beginning of putback area
              buffer+4,                // read position
              buffer+4+num);           // end of buffer

        // return next character
        return traits_type::to_int_type(*gptr());
    }
};

inbuf i;
std::istream is(&i);
libchess::UCIService uci_service{"Dog", "Folkert van Heusden", std::cout, is};

auto stop_handler = [&stop]() { stop = true; };

bool is_check(libchess::Position & pos)
{
	return pos.attackers_to(pos.piece_type_bb(libchess::constants::KING, !pos.side_to_move()).forward_bitscan(), pos.side_to_move());
}

bool is_insufficient_material_draw(const libchess::Position & pos)
{
	if (pos.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE).popcount() || 
		pos.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK).popcount() ||
		pos.piece_type_bb(libchess::constants::ROOK, libchess::constants::WHITE).popcount() ||
		pos.piece_type_bb(libchess::constants::ROOK, libchess::constants::BLACK).popcount() ||
		pos.piece_type_bb(libchess::constants::QUEEN, libchess::constants::WHITE).popcount() ||
		pos.piece_type_bb(libchess::constants::QUEEN, libchess::constants::BLACK).popcount())
		return false;

	if (pos.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::WHITE).popcount() || 
		pos.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::BLACK).popcount() ||
		pos.piece_type_bb(libchess::constants::BISHOP, libchess::constants::WHITE).popcount() ||
		pos.piece_type_bb(libchess::constants::BISHOP, libchess::constants::BLACK).popcount())
		return false;

	return true;
}

int eval_piece(const libchess::PieceType & piece)
{
	constexpr int values[] = { 100, 325, 325, 500, 950, 10000 };

	return values[piece];
}

int eval(libchess::Position & pos)
{
	int score = 0;

	for(libchess::Color color : libchess::constants::COLORS) {
		for(libchess::PieceType type : libchess::constants::PIECE_TYPES) {
			libchess::Bitboard piece_bb = pos.piece_type_bb(type, color);

			int mul = color == libchess::constants::WHITE ? 1 : -1;

			// material
			score += eval_piece(type) * piece_bb.popcount() * mul;
		}
	}

	if (pos.side_to_move() == libchess::constants::WHITE) {
		score += pos.legal_move_list().size();

		pos.make_null_move();
		score -= pos.legal_move_list().size();
		pos.unmake_move();

	}
	else {
		score -= pos.legal_move_list().size();

		pos.make_null_move();
		score += pos.legal_move_list().size();
		pos.unmake_move();
	}

	if (pos.side_to_move() != libchess::constants::WHITE)
		return -score;

	return score;
}
int search(libchess::Position & pos, int depth, int alpha, int beta, libchess::Move *const m)
{
	if (stop)
		return 0;

	auto gs = pos.game_state();

	if (gs != libchess::Position::GameState::IN_PROGRESS) {
		if (gs == libchess::Position::GameState::CHECKMATE)
			return -9999 + depth;

		if (gs == libchess::Position::GameState::STALEMATE)
			return 0;

		if (gs == libchess::Position::GameState::FIFTY_MOVES)
			return 0;

		if (gs == libchess::Position::GameState::THREEFOLD_REPETITION)
			return 0;

		return 0;
	}

	nodes++;

	if (pos.is_repeat() || is_insufficient_material_draw(pos))
		return 0;

	if (depth == 0)
		return eval(pos);

	*m = libchess::Move(0);

	int     best_score = -32767;

	auto    move_list  = pos.pseudo_legal_move_list();

	ssize_t n_moves    = move_list.size();

	for(auto move : move_list) {
		if (pos.is_legal_generated_move(move) == false)
			continue;

		libchess::Move new_move { 0 };

		pos.make_move(move);

		int score = -search(pos, depth - 1, -beta, -alpha, &new_move);

		pos.unmake_move();

		if (score > best_score) {
			best_score = score;

			*m = move;

			if (score > alpha) {
				alpha = score;

				if (score >= beta)
					break;
			}
		}
	}

	return best_score;
}

libchess::Move search_it(libchess::Position & pos, const int search_time)
{
	esp_timer_start_once(think_timeout_timer, search_time * 1000);

	uint64_t t_offset = esp_timer_get_time();

	libchess::Move best_move { 0 };

	int alpha     = -32767;
	int beta      = 32767;

	int add_alpha = 75;
	int add_beta  = 75;

	int max_depth = 1;

	for(;;) {
		libchess::Move cur_move;
		int score = search(pos, max_depth, alpha, beta, &cur_move);

		if (stop)
			break;

		if (score <= alpha) {
			beta = (alpha + beta) / 2;
			alpha = score - add_alpha;
			if (alpha < -10000)
				alpha = -10000;
			add_alpha += add_alpha / 15 + 1;
		}
		else if (score >= beta) {
			alpha = (alpha + beta) / 2;
			beta = score + add_beta;
			if (beta > 10000)
				beta = 10000;
			add_beta += add_beta / 15 + 1;
		}
		else {
			alpha = score - add_alpha;
			if (alpha < -10000)
				alpha = -10000;

			beta = score + add_beta;
			if (beta > 10000)
				beta = 10000;

			best_move = cur_move;

			uint64_t thought_ms = (esp_timer_get_time() - t_offset) / 1000;

			printf("info depth %d score cp %d nodes %u time %llu nps %llu pv %s\n", max_depth, score, nodes, thought_ms, nodes * 1000 / thought_ms, best_move.to_str().c_str());

			if (thought_ms > search_time / 2)
				break;

			add_alpha = 75;
			add_beta = 75;

			max_depth++;
		}
	}

	esp_timer_stop(think_timeout_timer);

	printf("# heap free: %u\n", esp_get_free_heap_size());

	return best_move;
}

void main_task(void *)
{
	std::ios_base::sync_with_stdio(true);
	std::cout.setf(std::ios::unitbuf);

	auto go_handler = [&position, &stop](const libchess::UCIGoParameters & go_parameters) {
		stop  = false;

		nodes = 0;

		int moves_to_go = 40 - position.fullmoves();

		auto movetime = go_parameters.movetime();

		auto a_w_time = go_parameters.wtime();
		auto a_b_time = go_parameters.btime();

		int w_time = a_w_time.has_value() ? a_w_time.value() : 0;
		int b_time = a_b_time.has_value() ? a_b_time.value() : 0;

		auto a_w_inc = go_parameters.winc();
		auto a_b_inc = go_parameters.binc();

		int w_inc = a_w_inc.has_value() ? a_w_inc.value() : 0;
		int b_inc = a_b_inc.has_value() ? a_b_inc.value() : 0;

		int think_time = 0;

		if (movetime.has_value())
			think_time = movetime.value();
		else {
			int cur_n_moves = moves_to_go <= 0 ? 40 : moves_to_go;

			int time_inc    = position.side_to_move() == libchess::constants::WHITE ? w_inc : b_inc;

			int ms          = position.side_to_move() == libchess::constants::WHITE ? w_time : b_time;

			think_time = (ms + (cur_n_moves - 1) * time_inc) / double(cur_n_moves + 7);

			int limit_duration_min = ms / 15;
			if (think_time > limit_duration_min)
				think_time = limit_duration_min;
		}

		auto best_move = search_it(position, think_time);

		libchess::UCIService::bestmove(best_move.to_str());
	};

	uci_service.register_position_handler(position_handler);
	uci_service.register_go_handler(go_handler);
	uci_service.register_stop_handler(stop_handler);

	while (true) {
		std::string line;
		std::getline(is, line);

		if (line == "uci") {
			uci_service.run();
			break;
		}
	}

	printf("TASK TERMINATED\n");
}

extern "C" void app_main()
{
	esp_task_wdt_init(30, false);

	esp_timer_create(&think_timeout_pars, &think_timeout_timer);

	TaskHandle_t main_task_handle;
	xTaskCreate(main_task, "chess", 65536, NULL, 10, &main_task_handle);

	printf("\n\n\n# HELLO, THIS IS DOG\n");

	for(;;) {
		vTaskDelay(1000);
	}
}
