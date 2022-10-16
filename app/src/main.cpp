// This code was written by Folkert van Heusden.
// Released under the MIT license.
// Some parts are not mine (e.g. the inbuf class): see the url
// above it for details.
#include <atomic>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <signal.h>
#include <streambuf>
#include <string>

#if defined(linux) || defined(_WIN32)
#include <chrono>
#include <limits.h>
#include <pthread.h>
#include <thread>
#include <sys/time.h>
#else
#include <driver/uart.h>

#include <driver/gpio.h>
#include <esp32/rom/uart.h>

#include <esp_task_wdt.h>

#include <esp_timer.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif

#include <libchess/Position.h>
#include <libchess/UCIService.h>

#include "eval.h"
#include "eval_par.h"
#include "psq.h"
#include "tt.h"


std::atomic_bool stop1 { false };
std::atomic_bool stop2 { false };
std::atomic_bool ponder_quit    { false };
bool             run_2nd_thread { false };

#if defined(linux) || defined(_WIN32)
std::thread *ponder_thread_handle { nullptr };
#else
TaskHandle_t ponder_thread_handle;
#endif

uint32_t         nodes { 0 };

void think_timeout(void *arg) {
	std::atomic_bool *stop = reinterpret_cast<std::atomic_bool *>(arg);

	*stop = true;
}

#if defined(linux) || defined(_WIN32)
#define LED_INTERNAL 0
#define LED_GREEN    0
#define LED_BLUE     0
#define LED_RED      0
#else
#define LED_INTERNAL (GPIO_NUM_2 )
#define LED_GREEN    (GPIO_NUM_27)
#define LED_BLUE     (GPIO_NUM_25)
#define LED_RED      (GPIO_NUM_22)

const esp_timer_create_args_t think_timeout_pars = {
            .callback = &think_timeout,
            .arg = &stop1,
            .name = "searchto"
};

typedef struct {
	gpio_num_t pin_nr;
	bool       state;
} led_t;

void blink_led(void *arg) {
	led_t *l = reinterpret_cast<led_t *>(arg);

	gpio_set_level(l->pin_nr, l->state);

	l->state = !l->state;
}

void start_blink(esp_timer_handle_t handle)
{
	esp_timer_start_periodic(handle, 100000);  // 10 times per second
}

void stop_blink(esp_timer_handle_t handle, led_t *l)
{
	esp_timer_stop(handle);

	gpio_set_level(l->pin_nr, false);
}

led_t led_green { LED_GREEN, false };

const esp_timer_create_args_t led_green_timer_pars = {
            .callback = &blink_led,
            .arg = &led_green,
            .name = "greenled"
};

led_t led_blue { LED_BLUE, false };

const esp_timer_create_args_t led_blue_timer_pars = {
            .callback = &blink_led,
            .arg = &led_blue,
            .name = "blueled"
};

led_t led_red { LED_RED, false };

const esp_timer_create_args_t led_red_timer_pars = {
            .callback = &blink_led,
            .arg = &led_red,
            .name = "redled"
};

esp_timer_handle_t think_timeout_timer;
esp_timer_handle_t led_green_timer;
esp_timer_handle_t led_blue_timer;
esp_timer_handle_t led_red_timer;
#endif

libchess::Position positiont1 { libchess::constants::STARTPOS_FEN };
libchess::Position positiont2 { libchess::constants::STARTPOS_FEN };

std::mutex  search_fen_lock;
std::string search_fen;

auto position_handler = [](const libchess::UCIPositionParameters & position_parameters) {
	positiont1 = libchess::Position { position_parameters.fen() };

	if (!position_parameters.move_list())
		return;

	for (auto & move_str : position_parameters.move_list()->move_list())
		positiont1.make_move(*libchess::Move::from(move_str));
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
        if (gptr() < egptr())
            return traits_type::to_int_type(*gptr());

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

#if !defined(linux) && !defined(_WIN32)
		vTaskDelay(1);
#endif
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
libchess::UCIService uci_service{"Dog v0.7", "Folkert van Heusden", std::cout, is};

tt tti;

auto stop_handler = []() { stop1 = true; };

int  thread_count = 2;

auto thread_count_handler = [](const int value) { thread_count = value; };

typedef struct
{
	std::atomic_bool *stop_flag;
} search_pars_t;

#if !defined(linux) && !defined(_WIN32)
extern "C" {
void vApplicationMallocFailedHook()
{
	printf("# *** OUT OF MEMORY (heap) ***\n");

	heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

#if !defined(linux) && !defined(_WIN32)
	start_blink(led_red_timer);
#endif
}
}

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
				printf("# %s\t%u%%\t%u\n",
						pxTaskStatusArray[x].pcTaskName,
						ulStatsAsPercentage,
						pxTaskStatusArray[x].usStackHighWaterMark);
			}
			else {
				printf("# %s\t%u\n",
						pxTaskStatusArray[x].pcTaskName,
						pxTaskStatusArray[x].usStackHighWaterMark);
			}
		}
	}

	vPortFree(pxTaskStatusArray);
}

int     md       = 1;
int64_t start_ts = 0;

bool checkMinStackSize(const int nr, search_pars_t *const sp)
{
	UBaseType_t level = uxTaskGetStackHighWaterMark(nullptr);

	if (level < 1024) {
		*sp->stop_flag = true;

#if !defined(linux) && !defined(_WIN32)
		start_blink(led_red_timer);
#endif

		printf("# stack protector %d engaged (%d)\n", nr, level);

		printf("# heap free: %u, max block size: %u\n", esp_get_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
		vTaskGetRunTimeStats();

		return true;
	}

	printf("# dts: %lld depth %d nodes %u lower_bound: %d\n", esp_timer_get_time() - start_ts, md, nodes, level);

	return false;
}
#endif

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
                for(size_t i=0; i<first_moves.size(); i++) {
                        if (move == first_moves.at(i))
                                return INT_MAX - i;
                }

                auto piece_from = p->piece_on(move.from_square());
		if (piece_from.has_value() == false)  // should not happen but is possible due to psuedo_legal_moves
			return -1;

                int score       = 0;

                if (p->is_promotion_move(move))
                        score += eval_piece(*move.promotion_piece_type(), default_parameters) << 18;

                if (p->is_capture_move(move)) {
                        auto piece_to = p->piece_on(move.to_square());
			if (piece_to.has_value() == false)  // cannot happen
				return -1;

                        // victim
                        score += (move.type() == libchess::Move::Type::ENPASSANT ? default_parameters.tune_pawn.value() : eval_piece(piece_to->type(), default_parameters)) << 18;

                        if (piece_from->type() != libchess::constants::KING)
                                score += (950 - eval_piece(piece_from->type(), default_parameters)) << 8;
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

int qs(libchess::Position & pos, int alpha, int beta, int qsdepth, search_pars_t *const sp)
{
	if (*sp->stop_flag)
		return 0;

#if !defined(linux) && !defined(_WIN32)
	if (qsdepth > md) {
		if (checkMinStackSize(1, sp))
			return 0;

		md = qsdepth;
	}
#endif

	nodes++;

	if (pos.halfmoves() >= 100 || pos.is_repeat() || is_insufficient_material_draw(pos))
		return 0;

	int  best_score = -32767;

	bool in_check   = pos.in_check();

	if (!in_check) {
		best_score = eval(pos, default_parameters);

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
			int  eval_target = move.type() == libchess::Move::Type::ENPASSANT ? default_parameters.tune_pawn.value() : eval_piece(piece_to->type(), default_parameters);

			auto piece_from  = pos.piece_on(move.from_square());
			int  eval_killer = eval_piece(piece_from->type(), default_parameters);

			if (eval_killer > eval_target && pos.attackers_to(move.to_square(), piece_to->color()))
				continue;
		}

		pos.make_move(move);

		if (pos.attackers_to(pos.piece_type_bb(libchess::constants::KING, !pos.side_to_move()).forward_bitscan(), pos.side_to_move())) {
			pos.unmake_move();
			continue;
		}

		n_played++;

		int score = -qs(pos, -beta, -alpha, qsdepth + 1, sp);

		pos.unmake_move();

		if (score > best_score) {
			best_score = score;

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
			best_score = eval(pos, default_parameters);
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

int search(libchess::Position & pos, int8_t depth, int16_t alpha, int16_t beta, const int null_move_depth, const int16_t max_depth, libchess::Move *const m, search_pars_t *const sp)
{
	if (*sp->stop_flag)
		return 0;

#if !defined(linux) && !defined(_WIN32)
	int d = max_depth - depth;

	if (d > md) {
		if (checkMinStackSize(1, sp))
			return 0;

		md = d;
	}
#endif

	if (depth == 0)
		return qs(pos, alpha, beta, max_depth, sp);

	nodes++;

	bool is_root_position = max_depth == depth;

	if (!is_root_position && (pos.is_repeat() || is_insufficient_material_draw(pos)))
		return 0;

	bool in_check         = pos.in_check();

	int start_alpha       = alpha;

	// TT //
	std::optional<libchess::Move> tt_move { };
	uint64_t       hash        = pos.hash();
	std::optional<tt_entry> te = tti.lookup(hash);

        if (te.has_value()) {
		if (te.value().data_._data.m)
			tt_move = libchess::Move(te.value().data_._data.m);

		if (tt_move.has_value() && pos.is_legal_move(tt_move.value()) == false) {
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

			if (use) {
				if (tt_move.has_value()) {
					*m = tt_move.value();

					return work_score;
				}

				if (!is_root_position) {
					*m = libchess::Move(0);

					return work_score;
				}
			}
		}
	}
	////////

	if (!is_root_position && depth <= 3 && beta <= 9800) {
		int staticeval = eval(pos, default_parameters);

		// static null pruning (reverse futility pruning)
		if (depth == 1 && staticeval - default_parameters.tune_knight.value() > beta)
			return beta;

		if (depth == 2 && staticeval - default_parameters.tune_rook.value() > beta)
			return beta;

		if (depth == 3 && staticeval - default_parameters.tune_queen.value() > beta)
			depth--;
	}

	///// null move
	int nm_reduce_depth = depth > 6 ? 4 : 3;
	if (depth >= nm_reduce_depth && !in_check && !is_root_position && null_move_depth < 2) {
		pos.make_null_move();

		libchess::Move ignore;
		int nmscore = -search(pos, depth - nm_reduce_depth, -beta, -beta + 1, null_move_depth + 1, max_depth, &ignore, sp);

		pos.unmake_move();

                if (nmscore >= beta) {
			int verification = search(pos, depth - nm_reduce_depth, beta - 1, beta, null_move_depth, max_depth, &ignore, sp);

			if (verification >= beta)
				return beta;
                }
	}
	///////////////
	
	int     extension  = 0;

	// IID //
	libchess::Move iid_move { 0 };

	if (null_move_depth == 0 && tt_move.has_value() == false && depth >= 2) {
		if (abs(search(pos, depth - 2, alpha, beta, null_move_depth, max_depth, &iid_move, sp)) > 9800)
			extension |= 1;
	}
	/////////

	int     best_score = -32767;

	libchess::MoveList move_list = pos.pseudo_legal_move_list();

	sort_movelist_compare smc(&pos);

	if (tt_move.has_value())
		smc.add_first_move(tt_move.value());

	if (m->value())
		smc.add_first_move(*m);

	if (iid_move.value())
		smc.add_first_move(iid_move);

	sort_movelist(pos, move_list, smc);

	int     n_played   = 0;

	int     lmr_start  = !in_check && depth >= 2 ? 4 : 999;

	libchess::Move new_move { 0 };

	for(auto move : move_list) {
		if (pos.is_legal_generated_move(move) == false)
			continue;

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

		score = -search(pos, new_depth + extension, -beta, -alpha, null_move_depth, max_depth, &new_move, sp);

		if (is_lmr && score > alpha) {
		skip_lmr:
			score = -search(pos, depth - 1, -beta, -alpha, null_move_depth, max_depth, &new_move, sp);
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

	if (n_played == 0) {
		if (in_check)
			best_score = -10000 + (max_depth - depth);
		else
			best_score = 0;
	}

	if (*sp->stop_flag == false) {
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

#if defined(linux) || defined(_WIN32)
uint64_t esp_timer_get_time()
{
	struct timeval tv;

	gettimeofday(&tv, nullptr);

	return tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif

std::vector<libchess::Move> get_pv_from_tt(const libchess::Position & pos_in, const libchess::Move & start_move)
{
	auto work = pos_in;

	std::vector<libchess::Move> out = { start_move };

	work.make_move(start_move);

	for(int i=0; i<64; i++) {
		std::optional<tt_entry> te = tti.lookup(work.hash());
		if (!te.has_value())
			break;

		libchess::Move cur_move = libchess::Move(te.value().data_._data.m);

		libchess::MoveList cur_moves = work.legal_move_list();
		if (!is_move_in_movelist(cur_moves, cur_move))
			break;

		out.push_back(cur_move);

		work.make_move(cur_move);

		if (work.is_repeat(3))
			break;
	}

	return out;
}

libchess::Move search_it(libchess::Position *const pos, const int search_time, const bool is_t2, search_pars_t *const sp)
{
	uint64_t t_offset = esp_timer_get_time();

#if defined(linux) || defined(_WIN32)
	std::thread *think_timeout_timer { nullptr };
#endif

	if (is_t2 == false) {
#if defined(linux) || defined(_WIN32)
		think_timeout_timer = new std::thread([search_time, t_offset, sp] {
				while(esp_timer_get_time() < t_offset + search_time * 1000)
					std::this_thread::sleep_for(std::chrono::microseconds(100000)); /* replace by cond.var. */

				*sp->stop_flag = true;
			});
#else
		esp_timer_start_once(think_timeout_timer, search_time * 1000);
#endif
	}

	auto move_list = pos->legal_move_list();

	libchess::Move best_move { *move_list.begin() };

	if (move_list.size() > 1) {
		int16_t alpha     = -32767;
		int16_t beta      =  32767;

		int16_t add_alpha = 75;
		int16_t add_beta  = 75;

		int8_t  max_depth = 1 + is_t2;

		libchess::Move cur_move { 0 };

		for(;;) {
			int score = search(*pos, max_depth, alpha, beta, 0, max_depth, &cur_move, sp);

			if (*sp->stop_flag)
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

				if (!is_t2) {
					if (thought_ms == 0)
						thought_ms = 1;

					std::vector<libchess::Move> pv = get_pv_from_tt(*pos, best_move);

					std::string pv_str;

					for(auto & move : pv)
						pv_str += " " + move.to_str();

					printf("info depth %d score cp %d nodes %u time %llu nps %llu pv%s\n", max_depth, score, nodes, thought_ms, nodes * 1000 / thought_ms, pv_str.c_str());
				}

				if (thought_ms > search_time / 2)
					break;

				add_alpha = 75;
				add_beta  = 75;

				max_depth++;
			}
		}
	}

	if (!is_t2) {
#if defined(linux) || defined(_WIN32)
		stop1 = true;

		think_timeout_timer->join();
		delete think_timeout_timer;
#else
		esp_timer_stop(think_timeout_timer);

		printf("# heap free: %u, max block size: %u\n", esp_get_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

		vTaskGetRunTimeStats();
#endif
	}

	return best_move;
}

#if defined(linux) || defined(_WIN32)
void gpio_set_level(int a, int b)
{
}
#endif

void ponder_thread(void *p)
{
	printf("# pondering started\n");

	search_pars_t sp { &stop2 };

	for(;!ponder_quit;) {
		search_fen_lock.lock();
		stop2      = false;
		positiont2 = libchess::Position(search_fen);
		search_fen_lock.unlock();

		if (positiont2.game_state() == libchess::Position::GameState::IN_PROGRESS && run_2nd_thread) {
			printf("# new ponder position\n");

#if !defined(linux) && !defined(_WIN32)
			start_blink(led_blue_timer);
#endif

			search_it(&positiont2, 2147483647, true, &sp);

#if !defined(linux) && !defined(_WIN32)
			stop_blink(led_blue_timer, &led_blue);
#endif
		}
		else {
			// TODO replace this by condition variables
#if defined(linux) || defined(_WIN32)
			std::this_thread::sleep_for(std::chrono::microseconds(10000));
#else
			vTaskDelay(10);  // TODO divide
#endif
		}
	}

	printf("# pondering stopping\n");

#if !defined(linux) && !defined(_WIN32)
	vTaskDelete(nullptr);
#endif
}

void start_ponder()
{
	stop2                = false;

#if defined(linux) || defined(_WIN32)
	ponder_thread_handle = new std::thread(ponder_thread, nullptr);
#else
	TaskHandle_t temp;

	xTaskCreatePinnedToCore(ponder_thread, "PT", 24576, NULL, 0, &temp, 0);

	ponder_thread_handle = temp;
#endif
}

void stop_ponder()
{
#if defined(linux) || defined(_WIN32)
	if (ponder_thread_handle) {
		ponder_quit = stop2 = true;

		ponder_thread_handle->join();
		delete ponder_thread_handle;
	}
#else
	stop2 = ponder_quit = true;
#endif
}

void main_task()
{
	std::ios_base::sync_with_stdio(true);
	std::cout.setf(std::ios::unitbuf);

	search_pars_t sp { &stop1 };

	auto go_handler = [&sp](const libchess::UCIGoParameters & go_parameters) {
		try {
			stop1 = false;

#if !defined(linux) && !defined(_WIN32)
			start_ts = esp_timer_get_time();

			md    = 1;
#endif
			nodes = 0;

			tti.inc_age();

#if !defined(linux) && !defined(_WIN32)
			stop_blink(led_red_timer, &led_red);
#endif

#if !defined(linux) && !defined(_WIN32)
			start_blink(led_green_timer);
#endif

			int moves_to_go = 40 - positiont1.fullmoves();

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

				int time_inc    = positiont1.side_to_move() == libchess::constants::WHITE ? w_inc : b_inc;

				int ms          = positiont1.side_to_move() == libchess::constants::WHITE ? w_time : b_time;

				think_time = (ms + (cur_n_moves - 1) * time_inc) / double(cur_n_moves + 7);

				int limit_duration_min = ms / 15;
				if (think_time > limit_duration_min)
					think_time = limit_duration_min;
			}

			// let the ponder thread run as a lazy-smp thread
			search_fen_lock.lock();
			run_2nd_thread = thread_count == 2;
			search_fen     = positiont1.fen();
			stop2          = true;  // restart ponder/lazy-smp thread
			search_fen_lock.unlock();

			// main search
			auto best_move = search_it(&positiont1, think_time, false, &sp);

			// no longer thinking
#if !defined(linux) && !defined(_WIN32)
			stop_blink(led_green_timer, &led_green);
#endif

			// emit result
			libchess::UCIService::bestmove(best_move.to_str());

			// set ponder positition
			positiont1.make_move(best_move);

			search_fen_lock.lock();
			run_2nd_thread = true;
			search_fen     = positiont1.fen();
			stop2          = true;
			search_fen_lock.unlock();

			positiont1.unmake_move();
		}
		catch(const std::exception& e) {
			printf("# EXCEPTION in main: %s\n", e.what());
		}
	};

	libchess::UCISpinOption thread_count_option("Threads", 2, 1, 2, thread_count_handler);

	uci_service.register_option(thread_count_option);

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

	stop_ponder();

	printf("TASK TERMINATED\n");
}

#if defined(linux) || defined(_WIN32)
int main(int argc, char *argv[])
{
#if !defined(_WIN32)
	signal(SIGPIPE, SIG_IGN);
#endif

	start_ponder();

	printf("\n\n\n# HELLO, THIS IS DOG\n");

	main_task();

	return 0;
}
#else
extern "C" void app_main()
{
	gpio_config_t io_conf { };
	io_conf.intr_type    = GPIO_INTR_DISABLE;//disable interrupt
	io_conf.mode         = GPIO_MODE_OUTPUT;//set as output mode
	io_conf.pin_bit_mask = (1ULL<<LED_INTERNAL) | (1ULL << LED_GREEN) | (1ULL << LED_BLUE) | (1ULL << LED_RED);//bit mask of the pins that you want to set,e.g.GPIO18
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;//disable pull-down mode
	io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;//disable pull-up mode
	esp_err_t error      = gpio_config(&io_conf);//configure GPIO with the given settings
	if (error != ESP_OK)
		printf("error configuring outputs\n");

	gpio_set_level(LED_INTERNAL, 1);
	gpio_set_level(LED_GREEN,    0);
	gpio_set_level(LED_BLUE,     0);
	gpio_set_level(LED_RED,      0);

	esp_task_wdt_init(30, false);

	esp_timer_create(&led_green_timer_pars, &led_green_timer);
	esp_timer_create(&led_blue_timer_pars,  &led_blue_timer );
	esp_timer_create(&led_red_timer_pars,   &led_red_timer  );

	esp_timer_create(&think_timeout_pars, &think_timeout_timer);

	start_ponder();

	printf("\n\n\n# HELLO, THIS IS DOG\n");

	gpio_set_level(LED_INTERNAL, 0);

	main_task();

#if !defined(linux) && !defined(_WIN32)
	start_blink(led_red_timer);
#endif
}
#endif
