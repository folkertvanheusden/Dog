// This code was written by Folkert van Heusden.
// Released under the MIT license.
// Some parts are not mine (e.g. the inbuf class): see the url
// above it for details.
#include <atomic>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <streambuf>

#include <driver/uart.h>

#include <driver/gpio.h>
#include <esp32/rom/uart.h>

#include <esp_task_wdt.h>

#include <esp_timer.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <libchess/Position.h>
#include <libchess/UCIService.h>

#include "psq.h"
#include "tt.h"


#define LED (GPIO_NUM_2)

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

auto position_handler = [](const libchess::UCIPositionParameters & position_parameters) {
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
libchess::UCIService uci_service{"Dog v0.2", "Folkert van Heusden", std::cout, is};

tt tti(65536);

auto stop_handler = []() { stop = true; };

void vTaskGetRunTimeStats()
{
	UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();

	TaskStatus_t *pxTaskStatusArray = (TaskStatus_t *)pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

	uint32_t ulTotalRunTime = 0;
	uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

	ulTotalRunTime /= 100UL;

	if (ulTotalRunTime > 0) {
		for(int x = 0; x < uxArraySize; x++) {
			unsigned ulStatsAsPercentage = pxTaskStatusArray[x].ulRunTimeCounter / ulTotalRunTime;

			if (ulStatsAsPercentage > 0UL) {
				printf("# %s\t%u\t%u%%\t%u\n",
						pxTaskStatusArray[x].pcTaskName,
						pxTaskStatusArray[x].ulRunTimeCounter,
						ulStatsAsPercentage,
						pxTaskStatusArray[x].usStackHighWaterMark);
			}
			else {
				printf("# %s\t%u\t%u\n",
						pxTaskStatusArray[x].pcTaskName,
						pxTaskStatusArray[x].ulRunTimeCounter,
						pxTaskStatusArray[x].usStackHighWaterMark);
			}
		}
	}

	vPortFree(pxTaskStatusArray);
}

int eval_piece(const libchess::PieceType & piece)
{
	constexpr int values[] = { 100, 325, 325, 500, 950, 10000 };

	return values[piece];
}

class sort_movelist_compare
{
private:
        libchess::Position       *const p;
        std::vector<libchess::Move>     first_moves;
        std::optional<libchess::Square> previous_move_target;

public:
        sort_movelist_compare(libchess::Position *const p) : p(p) {
                if (p->previous_move())
                        previous_move_target = p->previous_move()->to_square();
        }

        void add_first_move(const libchess::Move move) {
                if (move.value())
                        first_moves.push_back(move);
        }

        int move_evaluater(const libchess::Move move) const {
                for(int i=0; i<first_moves.size(); i++) {
                        if (move == first_moves.at(i))
                                return INT_MAX - i;
                }

                auto piece_from = p->piece_on(move.from_square());
		if (piece_from.has_value() == false)  // should not happen but is possible due to psuedo_legal_moves
			return -1;

                int score       = 0;

                if (p->is_promotion_move(move))
                        score += eval_piece(*move.promotion_piece_type()) << 18;

                if (p->is_capture_move(move)) {
                        auto piece_to = p->piece_on(move.to_square());
			if (piece_to.has_value() == false)  // cannot happen
				return -1;

                        // victim
                        score += (move.type() == libchess::Move::Type::ENPASSANT ? 100 : eval_piece(piece_to->type())) << 18;

                        if (piece_from->type() != libchess::constants::KING)
                                score += (950 - eval_piece(piece_from->type())) << 8;

//                      if (move.to_square() == previous_move_target)
//                              score += 100 << 8;
                }
                else {
//                        score += meta -> hbt[p->side_to_move()][move.from_square()][move.to_square()] << 8;
                }

                score += -psq(move.from_square(), piece_from->color(), piece_from->type(), 0) + psq(move.to_square(), piece_from->color(), piece_from->type(), 0);

                return score;
        }
};

void sort_movelist(libchess::Position & pos, libchess::MoveList & move_list, sort_movelist_compare & smc)
{
	move_list.sort([&smc](const libchess::Move move) { return smc.move_evaluater(move); });
}

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

int game_phase(const int counts[2][6])
{
        const int num_knights = counts[libchess::constants::WHITE][libchess::constants::KNIGHT] + counts[libchess::constants::BLACK][libchess::constants::KNIGHT];
        const int num_bishops = counts[libchess::constants::WHITE][libchess::constants::BISHOP] + counts[libchess::constants::BLACK][libchess::constants::BISHOP];
        const int num_rooks   = counts[libchess::constants::WHITE][libchess::constants::ROOK]   + counts[libchess::constants::BLACK][libchess::constants::ROOK];
        const int num_queens  = counts[libchess::constants::WHITE][libchess::constants::QUEEN]  + counts[libchess::constants::BLACK][libchess::constants::QUEEN];

        // from https://www.chessprogramming.org/Tapered_Eval
        constexpr int knight_phase = 1;
        constexpr int bishop_phase = 1;
        constexpr int rook_phase   = 2;
        constexpr int queen_phase  = 4;

        constexpr int total_phase = knight_phase * 4 + bishop_phase * 4 + rook_phase * 4 + queen_phase * 2;

        int phase = total_phase;

        phase -= knight_phase * num_knights;
        phase -= bishop_phase * num_bishops;
        phase -= rook_phase   * num_rooks;
        phase -= queen_phase  * num_queens;

        return (phase * 256 + (total_phase / 2)) / total_phase;
}

int eval(libchess::Position & pos)
{
	int counts[2][6] = { 0 };

	for(libchess::Color color : libchess::constants::COLORS) {
		for(libchess::PieceType type : libchess::constants::PIECE_TYPES) {
			libchess::Bitboard piece_bb = pos.piece_type_bb(type, color);

			counts[color][type] += piece_bb.popcount();
		}
	}

	int score = 0;

	int phase = game_phase(counts);

	for(libchess::Color color : libchess::constants::COLORS) {
		for(libchess::PieceType type : libchess::constants::PIECE_TYPES) {
			libchess::Bitboard piece_bb = pos.piece_type_bb(type, color);

			int                mul      = color == libchess::constants::WHITE ? 1 : -1;

			// material
			score += eval_piece(type) * piece_bb.popcount() * mul;

			// psq
			while (piece_bb) {
				libchess::Square sq = piece_bb.forward_bitscan();
				piece_bb.forward_popbit();

				score += psq(sq, color, type, phase) * mul * 402 / 546;
			}
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

libchess::MoveList gen_qs_moves(libchess::Position & pos)
{
	libchess::Color side = pos.side_to_move();

	if (pos.checkers_to(side))
		return pos.pseudo_legal_move_list();

	libchess::MoveList ml;
	pos.generate_promotions(ml, side);
	pos.generate_capture_moves(ml, side);

	return ml;
}

int md = 0;

int qs(libchess::Position & pos, int alpha, int beta, int qsdepth, libchess::Move *m)
{
	auto gs = pos.game_state();

	if (gs != libchess::Position::GameState::IN_PROGRESS) {
		if (gs == libchess::Position::GameState::CHECKMATE)
			return -9999 + qsdepth;

		if (gs == libchess::Position::GameState::STALEMATE)
			return 0;

		if (gs == libchess::Position::GameState::FIFTY_MOVES)
			return 0;

		if (gs == libchess::Position::GameState::THREEFOLD_REPETITION)
			return 0;

		return 0;
	}

	nodes++;

	if (pos.halfmoves() >= 100 || pos.is_repeat() || is_insufficient_material_draw(pos))
		return 0;

	if (qsdepth > md) {
		md = qsdepth;

		printf("# heap free: %u\n", esp_get_free_heap_size());
		vTaskGetRunTimeStats();

		printf("%d\n", qsdepth);
	}

	int  best_score = -32767;

	bool in_check   = pos.in_check();

	if (!in_check) {
		best_score = eval(pos);

		if (best_score > alpha && best_score >= beta)
			return best_score;

		int BIG_DELTA = 975;

		if (pos.is_promotion_move(*pos.previous_move()))
			BIG_DELTA += 775;

		if (best_score < alpha - BIG_DELTA)
			return alpha;

		if (alpha < best_score)
			alpha = best_score;
	}

	int  n_played    = 0;

	auto move_list   = gen_qs_moves(pos);

	sort_movelist_compare smc(&pos);

	sort_movelist(pos, move_list, smc);

	for(auto move : move_list) {
		if (pos.is_legal_generated_move(move) == false)
			continue;

		if (!in_check && pos.is_capture_move(move)) {
			auto piece_to    = pos.piece_on(move.to_square());
			int  eval_target = move.type() == libchess::Move::Type::ENPASSANT ? 100 : eval_piece(piece_to->type());

			auto piece_from  = pos.piece_on(move.from_square());
			int  eval_killer = eval_piece(piece_from->type());

			if (eval_killer > eval_target && pos.attackers_to(move.to_square(), piece_to->color()))
				continue;
		}

		pos.make_move(move);

		if (pos.attackers_to(pos.piece_type_bb(libchess::constants::KING, !pos.side_to_move()).forward_bitscan(), pos.side_to_move())) {
			pos.unmake_move();
			continue;
		}

		n_played++;

		libchess::Move curm { 0 };
		int score = -qs(pos, -beta, -alpha, qsdepth + 1, &curm);

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

	if (n_played == 0) {
		if (in_check)
			best_score = -10000 + qsdepth;
		else if (best_score == -32767)
			best_score = eval(pos);
	}

	return best_score;
}

bool is_move_in_movelist(libchess::MoveList & move_list, libchess::Move & m)
{
	return std::any_of(move_list.begin(), move_list.end(), [m](const libchess::Move & move) {
			return move.from_square() == m.from_square() &&
			move.to_square() == m.to_square() &&
			move.promotion_piece_type() == m.promotion_piece_type();
			});
}

int search(libchess::Position & pos, int depth, int alpha, int beta, const bool is_null_move, const int max_depth, libchess::Move *const m)
{
	if (stop)
		return 0;

	if (depth == 0)
		return qs(pos, alpha, beta, max_depth, m);

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

	bool is_root_position = max_depth == depth;

	bool in_check         = pos.in_check();

	std::optional<libchess::MoveList> move_list { };

	int start_alpha       = alpha;

	// TT //
	std::optional<libchess::Move> tt_move { };
	uint64_t       hash        = pos.hash();
	std::optional<tt_entry> te = tti.lookup(hash);

        if (te.has_value()) {
		if (te.value().data_._data.m)
			tt_move = libchess::Move(te.value().data_._data.m);

		move_list = pos.pseudo_legal_move_list();

		if (tt_move.has_value() && (!is_move_in_movelist(move_list.value(), tt_move.value()) || pos.is_legal_generated_move(tt_move.value()) == false)) {
			tt_move.reset();
		}
		else if (te.value().data_._data.depth >= depth) {
			bool use = false;

			int csd = max_depth - depth;
			int score = te.value().data_._data.score;
			int work_score = abs(score) > 9800 ? (score < 0 ? score + csd : score - csd) : score;

			if (te.value().data_._data.flags == EXACT)
				use = true;
			else if (te.value().data_._data.flags == LOWERBOUND && work_score >= beta)
				use = true;
			else if (te.value().data_._data.flags == UPPERBOUND && work_score <= alpha)
				use = true;

			if (use && (!is_root_position || tt_move.has_value())) {
				*m = tt_move.value();

				return work_score;
			}
		}
	}
	////////

	///// null move
	int nm_reduce_depth = depth > 6 ? 4 : 3;
	if (depth >= nm_reduce_depth && !in_check && !is_root_position && !is_null_move) {
		pos.make_null_move();

		libchess::Move ignore;
		int nmscore = -search(pos, depth - nm_reduce_depth, -beta, -beta + 1, true, max_depth, &ignore);

		pos.unmake_move();

                if (nmscore >= beta) {
			int verification = search(pos, depth - nm_reduce_depth, beta - 1, beta, false, max_depth, &ignore);

			if (verification >= beta)
				return beta;
                }
	}
	///////////////

	*m = libchess::Move(0);

	int     best_score = -32767;

	if (move_list.has_value() == false)
		move_list  = pos.pseudo_legal_move_list();

	sort_movelist_compare smc(&pos);
	if (tt_move.has_value())
		smc.add_first_move(tt_move.value());

	sort_movelist(pos, move_list.value(), smc);

	int     n_played   = 0;

	int     lmr_start  = !in_check && depth >= 2 ? 4 : 999;

	for(auto move : move_list.value()) {
		if (pos.is_legal_generated_move(move) == false)
			continue;

		libchess::Move new_move { 0 };

		bool is_lmr    = false;
		int  new_depth = depth - 1;

		if (n_played >= lmr_start && !pos.is_capture_move(move) && !pos.is_promotion_move(move)) {
			is_lmr = true;

			if (n_played >= lmr_start + 2)
				new_depth = (depth - 1) * 2 / 3;
			else
				new_depth = depth - 2;
		}

		pos.make_move(move);

		int score = -10000;

		bool check_after_move = pos.in_check();

		if (check_after_move)
			goto skip_lmr;

		score = -search(pos, new_depth, -beta, -alpha, is_null_move /* TODO dit hier? */, max_depth, &new_move);

		if (is_lmr && score > alpha) {
		skip_lmr:
			score = -search(pos, depth - 1, -beta, -alpha, is_null_move /* TODO dit hier? */, max_depth, &new_move);
		}

		pos.unmake_move();

		n_played++;

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

	if (stop == false) {
		tt_entry_flag flag = EXACT;

		if (best_score <= start_alpha)
			flag = UPPERBOUND;
		else if (best_score >= beta)
			flag = LOWERBOUND;

		tti.store(hash, flag, depth, best_score, 
				best_score > start_alpha || tt_move.has_value() == false ? *m : tt_move.value());
	}

	return best_score;
}

libchess::Move search_it(libchess::Position & pos, const int search_time)
{
	esp_timer_start_once(think_timeout_timer, search_time * 1000);

	uint64_t t_offset = esp_timer_get_time();

	libchess::Move best_move { *pos.legal_move_list().begin() };

	int alpha     = -32767;
	int beta      = 32767;

	int add_alpha = 75;
	int add_beta  = 75;

	int max_depth = 1;

	for(;;) {
		libchess::Move cur_move;
		int score = search(pos, max_depth, alpha, beta, false, max_depth, &cur_move);

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

	vTaskGetRunTimeStats();

	return best_move;
}

void main_task(void *)
{
	std::ios_base::sync_with_stdio(true);
	std::cout.setf(std::ios::unitbuf);

	auto go_handler = [](const libchess::UCIGoParameters & go_parameters) {
		stop  = false;

		nodes = 0;

		gpio_set_level(LED, 1);

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

		gpio_set_level(LED, 0);

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

	gpio_config_t io_conf { };
	io_conf.intr_type    = GPIO_INTR_DISABLE;//disable interrupt
	io_conf.mode         = GPIO_MODE_OUTPUT;//set as output mode
	io_conf.pin_bit_mask = (1ULL<<LED);//bit mask of the pins that you want to set,e.g.GPIO18
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;//disable pull-down mode
	io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;//disable pull-up mode
	esp_err_t error      = gpio_config(&io_conf);//configure GPIO with the given settings
	if (error!=ESP_OK)
		printf("error configuring outputs\n");

	gpio_set_level(LED, 0);

	esp_timer_create(&think_timeout_pars, &think_timeout_timer);

	TaskHandle_t main_task_handle;
	xTaskCreate(main_task, "chess", 4096, NULL, 10, &main_task_handle);

	printf("\n\n\n# HELLO, THIS IS DOG\n");

	for(;;) {
		vTaskDelay(1000);
	}
}
