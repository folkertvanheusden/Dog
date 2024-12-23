// This code was written by Folkert van Heusden.
// Released under the MIT license.
// Some parts are not mine (e.g. the inbuf class): see the url
// above it for details.
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <signal.h>
#include <streambuf>
#include <string>

#if defined(__ANDROID__)
#include <android/log.h>
#define APPNAME "Dog"
#endif

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
#include "max.h"
#include <chrono>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <thread>
#include <unistd.h>
#include <sys/time.h>

#include "usb-device.h"
#else
#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp32/rom/uart.h>
#include <esp_err.h>
#include <esp_spiffs.h>
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
#include "inbuf.h"
#include "main.h"
#include "max-ascii.h"
#include "psq.h"
#include "str.h"
#if defined(linux) || defined(_WIN32)
#include "syzygy.h"
bool with_syzygy = false;
#endif
#include "test.h"
#include "tt.h"
#include "tui.h"
#include "tuners.h"


std::atomic_bool ponder_quit    { false };
int              thread_count   { 1     };
std::atomic_bool ponder_running { false };

void start_ponder_thread();
void stop_ponder_thread();

end_t         stop1 { false };
search_pars_t sp1   { nullptr, false, reinterpret_cast<int16_t *>(malloc(history_malloc_size)), 0, 0, 0, &stop1 };

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
std::vector<search_pars_t> sp2;
std::vector<end_t *>       stop2;

std::thread *usb_disp_thread = nullptr;
#else
end_t         stop2 { false };
search_pars_t sp2   { nullptr, true,  reinterpret_cast<int16_t *>(malloc(history_malloc_size)), 0, 0, 0, &stop2 };
#endif

#if defined(linux)
uint64_t wboard { 0 };
uint64_t bboard { 0 };
#endif

bool trace_enabled = false;
auto allow_tracing_handler = [](const bool value) {
	trace_enabled = value;
	printf("# Tracing %s\n", value ? "enabled" : "disabled");
};
void trace(const char *const fmt, ...)
{
	if (trace_enabled) {
		va_list ap { };
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}

void set_flag(end_t *const stop)
{
	stop->flag = true;
	stop->cv.notify_all();
}

void clear_flag(end_t *const stop)
{
	stop->flag = false;
}

auto stop_handler = []()
{
	set_flag(sp1.stop);
#if !defined(__ANDROID__)
	trace("# stop_handler invoked\n");
#endif
};

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
std::thread *ponder_thread_handle { nullptr };
#else
TaskHandle_t ponder_thread_handle;
#endif

void think_timeout(void *arg)
{
	end_t *stop = reinterpret_cast<end_t *>(arg);
	set_flag(stop);
}

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
#define LED_INTERNAL 0
#define LED_GREEN    0
#define LED_BLUE     0
#define LED_RED      0
#else
#define LED_INTERNAL (GPIO_NUM_2 )
#define LED_GREEN    (GPIO_NUM_27)
#define LED_BLUE     (GPIO_NUM_25)
#define LED_RED      (GPIO_NUM_22)

QueueHandle_t uart_queue;

const esp_timer_create_args_t think_timeout_pars = {
            .callback = &think_timeout,
            .arg      = sp1.stop,
            .name     = "searchto"
};

void blink_led(void *arg)
{
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

// for pondering
std::mutex  search_fen_lock;
std::string search_fen;
uint16_t    search_fen_version { 0     };
bool        is_ponder          { false };

auto position_handler = [](const libchess::UCIPositionParameters & position_parameters) {
	positiont1 = libchess::Position { position_parameters.fen() };
	if (!position_parameters.move_list())
		return;

	for (auto & move_str : position_parameters.move_list()->move_list())
		positiont1.make_move(*libchess::Move::from(move_str));
};

void set_thread_name(std::string name)
{
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
        if (name.length() > 15)
                name = name.substr(0, 15);

        pthread_setname_np(pthread_self(), name.c_str());
#endif
}

inbuf i;
std::istream is(&i);
libchess::UCIService uci_service{"Dog v2.5", "Folkert van Heusden", std::cout, is};

auto thread_count_handler = [](const int value)  {
	thread_count = value;

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
	stop_ponder_thread();

	for(auto & stop: stop2)
		delete stop;

	stop2.clear();

	for(auto & sp: sp2)
		free(sp.history);
	sp2.clear();

	for(int i=0; i<thread_count; i++) {
		stop2.push_back(new end_t());
		sp2.push_back({ nullptr, true, reinterpret_cast<int16_t *>(malloc(history_malloc_size)), 0, 0, 0, stop2.at(i) });
	}

	start_ponder_thread();
#endif
};

bool allow_ponder         = true;
auto allow_ponder_handler = [](const bool value) {
	allow_ponder = value;
	printf("# Ponder %s\n", value ? "enabled" : "disabled");
};

auto commerial_option_handler = [](const std::string & value) { };

#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__)
extern "C" {
void vApplicationMallocFailedHook()
{
	printf("# *** OUT OF MEMORY (heap) ***\n");
	heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
	start_blink(led_red_timer);
}
}

void vTaskGetRunTimeStats()
{
#if !defined(NO_uxTaskGetSystemState)
	UBaseType_t   uxArraySize       = uxTaskGetNumberOfTasks();
	TaskStatus_t *pxTaskStatusArray = reinterpret_cast<TaskStatus_t *>(pvPortMalloc(uxArraySize * sizeof(TaskStatus_t)));

	uint32_t      ulTotalRunTime    = 0;
	uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

	ulTotalRunTime /= 100UL;
	if (ulTotalRunTime > 0) {
		for(int x = 0; x < uxArraySize; x++) {
			unsigned ulStatsAsPercentage = pxTaskStatusArray[x].ulRunTimeCounter / ulTotalRunTime;

			if (ulStatsAsPercentage > 0UL) {
				trace("# %s\t%u%%\t%u\n",
						pxTaskStatusArray[x].pcTaskName,
						ulStatsAsPercentage,
						pxTaskStatusArray[x].usStackHighWaterMark);
			}
			else {
				trace("# %s\t%u\n",
						pxTaskStatusArray[x].pcTaskName,
						pxTaskStatusArray[x].usStackHighWaterMark);
			}
		}
	}

	vPortFree(pxTaskStatusArray);
#endif
}

void show_esp32_info()
{
	printf("# heap free: %u, max block size: %u\n", esp_get_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
	printf("# task name: %s\n", pcTaskGetName(xTaskGetCurrentTaskHandle()));

	vTaskGetRunTimeStats();
}

int64_t esp_start_ts = 0;
int check_min_stack_size(const int nr, search_pars_t *const sp)
{
	UBaseType_t level = uxTaskGetStackHighWaterMark(nullptr);

	trace("# dts: %lld depth %d nodes %u lower_bound: %d, task name: %s\n", esp_timer_get_time() - esp_start_ts, sp->md, sp->nodes, level, pcTaskGetName(xTaskGetCurrentTaskHandle()));

	if (level < 768) {
		set_flag(sp->stop);
		start_blink(led_red_timer);

		printf("# stack protector %d engaged (%d), full stop\n", nr, level);
		show_esp32_info();

		return 2;
	}

	if (level < 1280) {
		printf("# stack protector %d engaged (%d), stop QS\n", nr, level);
		printf("# task name: %s\n", pcTaskGetName(xTaskGetCurrentTaskHandle()));

		return 1;
	}

	return 0;
}
#endif

inline int history_index(const libchess::Color & side, const libchess::PieceType & from_type, const libchess::Square & sq)
{
	return side * 6 * 64 + from_type * 64 + sq;
}

sort_movelist_compare::sort_movelist_compare(libchess::Position *const p, const search_pars_t *const sp) :
	p(p),
	ep(sp->parameters),
	sp(sp)
{
	if (p->previous_move())
		previous_move_target = p->previous_move()->to_square();
}

void sort_movelist_compare::add_first_move(const libchess::Move move)
{
	assert(move.value());
	first_moves.push_back(move);
}

int sort_movelist_compare::move_evaluater(const libchess::Move move) const
{
	for(size_t i=0; i<first_moves.size(); i++) {
		if (move == first_moves.at(i))
			return INT_MAX - i;
	}

	int  score      = 0;
	auto piece_from = p->piece_on(move.from_square());
	auto from_type  = piece_from->type();
	auto to_type    = from_type;

	if (p->is_promotion_move(move)) {
		to_type = *move.promotion_piece_type();

		score  += eval_piece(to_type, *ep) << 18;
	}

	if (p->is_capture_move(move)) {
		if (move.type() == libchess::Move::Type::ENPASSANT)
			score += ep->pawn << 18;
		else {
			auto piece_to = p->piece_on(move.to_square());

			// victim
			score += eval_piece(piece_to->type(), *ep) << 18;
		}

		if (from_type != libchess::constants::KING)
			score += (eval_piece(libchess::constants::QUEEN, *ep) - eval_piece(from_type, *ep)) << 8;
	}
	else {
		int index = history_index(p->side_to_move(), from_type, move.to_square());
		score += sp->history[index] << 8;
	}

	score += -psq(move.from_square(), piece_from->color(), from_type, 0) + psq(move.to_square(), piece_from->color(), to_type, 0);

	return score;
}

void sort_movelist(libchess::MoveList & move_list, sort_movelist_compare & smc)
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

	if ((pos.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::WHITE).popcount() &&
		pos.piece_type_bb(libchess::constants::BISHOP, libchess::constants::WHITE).popcount()) ||
		(pos.piece_type_bb(libchess::constants::KNIGHT, libchess::constants::BLACK).popcount() &&
		pos.piece_type_bb(libchess::constants::BISHOP, libchess::constants::BLACK).popcount()))
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

int qs(libchess::Position & pos, int alpha, int beta, int qsdepth, search_pars_t *const sp, const int thread_nr)
{
	if (sp->stop->flag)
		return 0;
#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__)
	if (qsdepth > sp->md) {
		if (check_min_stack_size(1, sp))
			return 0;

		sp->md = qsdepth;
	}
#endif
	if (qsdepth >= 127)
		return eval(pos, *sp->parameters);

	sp->qnodes++;

	if (pos.halfmoves() >= 100 || pos.is_repeat() || is_insufficient_material_draw(pos))
		return 0;

	int  best_score = -32767;

	bool in_check   = pos.in_check();
	if (!in_check) {
		// standing pat
		best_score = eval(pos, *sp->parameters);
		if (best_score > alpha && best_score >= beta)
			return best_score;

		int BIG_DELTA = sp->parameters->big_delta;
		if (pos.previous_move().has_value() && pos.is_promotion_move(pos.previous_move().value()))
			BIG_DELTA += sp->parameters->big_delta_promotion;
		if (best_score < alpha - BIG_DELTA)
			return alpha;
		if (alpha < best_score)
			alpha = best_score;
	}

	int  n_played    = 0;
	auto move_list   = gen_qs_moves(pos);

	sort_movelist_compare smc(&pos, sp);
	sort_movelist(move_list, smc);

	for(auto move : move_list) {
		if (pos.is_legal_generated_move(move) == false)
			continue;

		if (!in_check && pos.is_capture_move(move)) {
			auto piece_to    = pos.piece_on(move.to_square());
			int  eval_target = move.type() == libchess::Move::Type::ENPASSANT ? sp->parameters->pawn : eval_piece(piece_to->type(), *sp->parameters);
			auto piece_from  = pos.piece_on(move.from_square());
			int  eval_killer = eval_piece(piece_from->type(), *sp->parameters);
			if (eval_killer > eval_target && pos.attackers_to(move.to_square(), !pos.side_to_move()))
				continue;
		}

		n_played++;

		pos.make_move(move);
		int score = -qs(pos, -beta, -alpha, qsdepth + 1, sp, thread_nr);
		pos.unmake_move();

		if (score > best_score) {
			best_score = score;

			if (score > alpha) {
				if (score >= beta)
					break;

				alpha = score;

			}
		}
	}

	if (n_played == 0) {
		if (in_check)
			best_score = -10000 + qsdepth;
		else if (best_score == -32767)
			best_score = eval(pos, *sp->parameters);
	}

	return best_score;
}

void update_history(search_pars_t *const sp, const int index, const int bonus)
{
	constexpr int max_history = 32760;
	constexpr int min_history = -max_history;
	int  clamped_bonus   = std::clamp(bonus, min_history, max_history);
	int  final_value     = clamped_bonus - sp->history[index] * abs(clamped_bonus) / max_history;

	sp->history[index]  += final_value;
}

int search(libchess::Position & pos, int8_t depth, int16_t alpha, int16_t beta, const int null_move_depth, const int16_t max_depth, libchess::Move *const m, search_pars_t *const sp, const int thread_nr)
{
	if (sp->stop->flag)
		return 0;

	if (depth == 0)
		return qs(pos, alpha, beta, max_depth, sp, thread_nr);

	int d = max_depth - depth;

	if (d > sp->md) {
#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__)
		if (check_min_stack_size(0, sp))
			return 0;
#endif

		sp->md = d;
	}

	sp->nodes++;

	bool is_root_position = max_depth == depth;
	if (!is_root_position && (pos.is_repeat() || is_insufficient_material_draw(pos)))
		return 0;

	int start_alpha       = alpha;

	// TT //
	std::optional<libchess::Move> tt_move { };
	uint64_t       hash        = pos.hash();
	std::optional<tt_entry> te = tti.lookup(hash);

        if (te.has_value()) {  // TT hit?
		if (te.value().data_._data.m)  // move stored in TT?
			tt_move = libchess::Move(te.value().data_._data.m);

		if (tt_move.has_value() && pos.is_legal_move(tt_move.value()) == false) {
			tt_move.reset();  // move stored in TT is not valid - TT-collision
		}
		else if (te.value().data_._data.depth >= depth) {
			int csd        = max_depth - depth;
			int score      = te.value().data_._data.score;
			int work_score = abs(score) > 9800 ? (score < 0 ? score + csd : score - csd) : score;
			auto flag      = te.value().data_._data.flags;
                        bool use       = flag == EXACT ||
                                        (flag == LOWERBOUND && work_score >= beta) ||
                                        (flag == UPPERBOUND && work_score <= alpha);

			if (use) {
				if (tt_move.has_value()) {
					*m = tt_move.value();  // move in TT is valid
					return work_score;
				}

				if (!is_root_position) {  // no move, but score is valid
					*m = libchess::Move(0);
					return work_score;
				}
			}
		}
	}
	else if (depth >= 4) {  // IIR, Internal Iterative Reductions
		depth--;
	}
	////////

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
	if (with_syzygy && sp->is_t2) {
		// check piece count
		unsigned counts = pos.occupancy_bb().popcount();

		// syzygy count?
		if (counts <= TB_LARGEST) {
			sp->syzygy_queries++;
			std::optional<int> syzygy_score = probe_fathom_nonroot(pos);

			if (syzygy_score.has_value()) {
				sp->syzygy_query_hits++;

				int score = syzygy_score.value();
				tti.store(hash, EXACT, depth, score, libchess::Move(0));

				return score;
			}
		}
	}
#endif
	bool in_check = pos.in_check();

	if (!is_root_position && !in_check && depth <= 7 && beta <= 9800) {
		int staticeval = eval(pos, *sp->parameters);

		// static null pruning (reverse futility pruning)
		if (staticeval - depth * 121 > beta)
			return (beta + staticeval) / 2;
	}

#if defined(linux)
	if (sp->is_t2 == false) {
		wboard = pos.color_bb(libchess::constants::WHITE);
		bboard = pos.color_bb(libchess::constants::BLACK);
	}
#endif

	///// null move
	int nm_reduce_depth = depth > 6 ? 4 : 3;
	if (depth >= nm_reduce_depth && !in_check && !is_root_position && null_move_depth < 2) {
		pos.make_null_move();
		libchess::Move ignore;
		int nmscore = -search(pos, depth - nm_reduce_depth, -beta, -beta + 1, null_move_depth + 1, max_depth, &ignore, sp, thread_nr);
		pos.unmake_move();

                if (nmscore >= beta) {
			libchess::Move ignore2;
			int verification = search(pos, depth - nm_reduce_depth, beta - 1, beta, null_move_depth, max_depth, &ignore2, sp, thread_nr);
			if (verification >= beta)
				return beta;
                }
	}
	///////////////
	
	int     extension  = 0;

	// IID //
	libchess::Move iid_move { 0 };
	if (null_move_depth == 0 && tt_move.has_value() == false && depth >= 2) {
		if (abs(search(pos, depth - 2, alpha, beta, null_move_depth, max_depth, &iid_move, sp, thread_nr)) > 9800)
			extension |= 1;
	}
	/////////

	int                best_score = -32767;
	libchess::MoveList move_list  = pos.pseudo_legal_move_list();

	sort_movelist_compare smc(&pos, sp);

	if (tt_move.has_value())
		smc.add_first_move(tt_move.value());
	else if (iid_move.value())
		smc.add_first_move(iid_move);

	if (m->value() && pos.is_capture_move(*m))
		smc.add_first_move(*m);

	sort_movelist(move_list, smc);

	int     n_played   = 0;
	int     lmr_start  = !in_check && depth >= 2 ? 4 : 999;

	std::optional<libchess::Move> beta_cutoff_move;
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
		int  score            = -10000;
		bool check_after_move = pos.in_check();
		if (check_after_move)
			goto skip_lmr;

		score = -search(pos, new_depth + extension, -beta, -alpha, null_move_depth, max_depth, &new_move, sp, thread_nr);

		if (is_lmr && score > alpha) {
		skip_lmr:
			score = -search(pos, depth - 1, -beta, -alpha, null_move_depth, max_depth, &new_move, sp, thread_nr);
		}
		pos.unmake_move();

		n_played++;

		if (score > best_score) {
			best_score = score;

			*m = move;

			if (score > alpha) {
				if (score >= beta) {
					if (!pos.is_capture_move(move))
						beta_cutoff_move = move;
					break;
				}

				alpha = score;
			}
		}
	}

	// https://www.chessprogramming.org/History_Heuristic#History_Bonuses
	if (beta_cutoff_move.has_value()) {
		int bonus = depth * depth;
		for(auto move : move_list) {
			if (pos.is_capture_move(move))
				continue;
			auto piece_type_from = pos.piece_type_on(move.from_square());
			int  index           = history_index(pos.side_to_move(), piece_type_from.value(), move.to_square());
			if (move == beta_cutoff_move.value()) {
				update_history(sp, index, bonus);
				break;
			}
			update_history(sp, index, -bonus);
		}
	}

	if (n_played == 0) {
		if (in_check)
			best_score = -10000 + (max_depth - depth);
		else
			best_score = 0;
	}

	if (sp->stop->flag == false) {
		tt_entry_flag flag = EXACT;

		if (best_score <= start_alpha)
			flag = UPPERBOUND;
		else if (best_score >= beta)
			flag = LOWERBOUND;

		tti.store(hash, flag, depth, best_score, 
				(best_score > start_alpha && m->value()) || tt_move.has_value() == false ? *m : tt_move.value());
	}

	return best_score;
}

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
uint64_t esp_timer_get_time()
{
	timeval tv;
	gettimeofday(&tv, nullptr);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif

void timer(const int think_time, end_t *const ei)
{
	if (think_time > 0) {
		auto end_time = std::chrono::high_resolution_clock::now() += std::chrono::milliseconds{think_time};

		std::mutex m;  // not used

		std::unique_lock<std::mutex> lk(m);

		for(;!ei->flag;) {
			if (ei->cv.wait_until(lk, end_time) == std::cv_status::timeout)
				break;
		}
	}

	set_flag(ei);

#if !defined(__ANDROID__)
	trace("# time is up; set stop flag\n");
#endif
}

typedef struct {
	uint64_t nodes;
	uint64_t qnodes;
	uint32_t syzygy_queries;
	uint32_t syzygy_query_hits;
} node_sum_t;

node_sum_t calculate_search_statistics()
{
	node_sum_t out { };

	out.nodes             = sp1.nodes;
	out.qnodes            = sp1.qnodes;
	out.syzygy_queries    = 0;
	out.syzygy_query_hits = 0;

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
	for(auto & sp: sp2) {
		out.nodes  += sp.nodes;
		out.qnodes += sp.qnodes;
	}

	for(auto & sp: sp2) {
		out.syzygy_queries    += sp.syzygy_queries;
		out.syzygy_query_hits += sp.syzygy_query_hits;
	}
#else
	out.nodes  += sp2.nodes;
	out.qnodes += sp2.qnodes;
#endif

	return out;
}

double calculate_EBF(const std::vector<uint64_t> & node_counts)
{
        size_t n = node_counts.size();
        return n >= 3 ? sqrt(double(node_counts.at(n - 1)) / double(node_counts.at(n - 3))) : -1;
}

std::pair<libchess::Move, int> search_it(libchess::Position *const pos, const int search_time, const bool is_absolute_time, search_pars_t *const sp, const int ultimate_max_depth, const int thread_nr, std::optional<uint64_t> max_n_nodes)
{
	uint64_t t_offset = esp_timer_get_time();

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
	std::thread *think_timeout_timer { nullptr };
#endif

	if (sp->is_t2 == false) {
		if (search_time > 0) {
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
			think_timeout_timer = new std::thread([search_time, sp] {
					set_thread_name("searchtotimer");
					timer(search_time, sp->stop);
				});
#else
			esp_timer_start_once(think_timeout_timer, search_time * 1000ll);
#endif
		}
	}

	int16_t best_score = 0;

	auto move_list = pos->legal_move_list();
	libchess::Move best_move { *move_list.begin() };

	if (move_list.size() > 1) {
		int16_t alpha     = -32767;
		int16_t beta      =  32767;

		int16_t add_alpha = 75;
		int16_t add_beta  = 75;

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
		int8_t  max_depth = 1 + (sp->is_t2 ? thread_nr + 1: 0);
#else
		int8_t  max_depth = 1 + sp->is_t2;
#endif

		libchess::Move cur_move { 0 };

		int alpha_repeat = 0;
		int beta_repeat  = 0;

		std::vector<uint64_t> node_counts;
		uint64_t previous_node_count = 0;

		while(ultimate_max_depth == -1 || max_depth <= ultimate_max_depth) {
#if defined(linux)
			sp->md = 0;
#endif
			int score = search(*pos, max_depth, alpha, beta, 0, max_depth, &cur_move, sp, thread_nr);

			if (sp->stop->flag) {
#if !defined(__ANDROID__)
				if (sp->is_t2 == false)
					printf("# stop flag set\n");
#endif
				printf("info depth %d score cp %d\n", max_depth, score);
				break;
			}

			node_sum_t counts    = calculate_search_statistics();
			uint64_t cur_n_nodes = counts.nodes + counts.qnodes;
			node_counts.push_back(cur_n_nodes - previous_node_count);
			previous_node_count  = cur_n_nodes;

			if (score <= alpha) {
				if (alpha_repeat >= 3)
					alpha = -10000;
				else {
					beta = (alpha + beta) / 2;
					alpha = score - add_alpha;
					if (alpha < -10000)
						alpha = -10000;
					add_alpha += add_alpha / 15 + 1;

					alpha_repeat++;
				}
			}
			else if (score >= beta) {
				if (beta_repeat >= 3)
					beta = 10000;
				else {
					alpha = (alpha + beta) / 2;
					beta = score + add_beta;
					if (beta > 10000)
						beta = 10000;
					add_beta += add_beta / 15 + 1;

					beta_repeat++;
				}
			}
			else {
				alpha_repeat = beta_repeat = 0;

				alpha = score - add_alpha;
				if (alpha < -10000)
					alpha = -10000;

				beta = score + add_beta;
				if (beta > 10000)
					beta = 10000;

				best_move  = cur_move;
				best_score = score;

#if defined(linux)
				strncpy(sp->move, best_move.to_str().c_str(), 4);

				sp->score = score;
#endif

				uint64_t   thought_ms = (esp_timer_get_time() - t_offset) / 1000;

				if (!sp->is_t2) {
					std::vector<libchess::Move> pv = get_pv_from_tt(*pos, best_move);
					std::string pv_str;
					for(auto & move : pv)
						pv_str += " " + move.to_str();

					double      ebf     = calculate_EBF(node_counts);
					std::string ebf_str = ebf >= 0 ? std::to_string(ebf) : "";
					if (ebf_str.empty() == false)
						ebf_str = "ebf " + ebf_str + " ";

					uint64_t use_thought_ms = std::max(uint64_t(1), thought_ms);  // prevent div. by 0
					if (abs(score) > 9800) {
						int mate_moves = (10000 - abs(score) + 1) / 2 * (score < 0 ? -1 : 1);
						printf("info depth %d score mate %d nodes %zu %stime %" PRIu64 " nps %" PRIu64 " tbhits %u pv%s\n",
								max_depth, mate_moves,
								size_t(cur_n_nodes), ebf_str.c_str(), thought_ms, uint64_t(cur_n_nodes * 1000 / use_thought_ms),
								counts.syzygy_query_hits, pv_str.c_str());
					}
					else {
						printf("info depth %d score cp %d nodes %zu %stime %" PRIu64 " nps %" PRIu64 " tbhits %u pv%s\n",
								max_depth, score,
								size_t(cur_n_nodes), ebf_str.c_str(), thought_ms, uint64_t(cur_n_nodes * 1000 / use_thought_ms),
								counts.syzygy_query_hits, pv_str.c_str());
					}
				}

				if ((thought_ms > uint64_t(search_time / 2) && search_time > 0 && is_absolute_time == false) ||
				    (thought_ms >= search_time && is_absolute_time == true)) {
#if !defined(__ANDROID__)
					printf("# time %u is up %" PRIu64 "\n", search_time, thought_ms);
#endif
					break;
				}

				add_alpha = 75;
				add_beta  = 75;

				if (max_depth == 127)
					break;

				if (max_n_nodes.has_value() && cur_n_nodes >= max_n_nodes.value()) {
					printf("# node limit reached with %zu nodes\n", size_t(cur_n_nodes));
					break;
				}

				max_depth++;
			}
		}

#if !defined(__ANDROID__)
		node_sum_t counts = calculate_search_statistics();
		printf("# %" PRIu64 " search %" PRIu64 " qs: qs/s=%.3f\n", counts.nodes, counts.qnodes, double(counts.qnodes)/counts.nodes);
#endif
	}
	else {
#if !defined(__ANDROID__)
		printf("# only 1 move possible (%s for %s)\n", best_move.to_str().c_str(), pos->fen().c_str());
#endif
	}

	if (!sp->is_t2) {
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
		set_flag(sp->stop);

		if (think_timeout_timer) {
			think_timeout_timer->join();

			delete think_timeout_timer;
		}
#else
		esp_timer_stop(think_timeout_timer);

		trace("# heap free: %u, max block size: %u\n", esp_get_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

		vTaskGetRunTimeStats();
#endif
	}

	return { best_move, best_score };
}

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
void gpio_set_level(int a, int b)
{
}
#endif

void ponder_thread(void *p)
{
	set_thread_name("PT");
#if !defined(__ANDROID__)
	trace("# pondering started\n");
#endif

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
	for(auto & sp: sp2) {
		sp.parameters = &default_parameters;
		sp.is_t2 = true;
	}
#else
	sp2.parameters = &default_parameters;
	sp2.is_t2 = true;
#endif

	uint16_t prev_search_fen_version = uint16_t(-1);

	for(;!ponder_quit;) {
		bool valid = false;

		search_fen_lock.lock();
		// if other fen, then trigger search
		if (search_fen.empty() == false && search_fen_version != prev_search_fen_version) {
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
			for(auto & sp: sp2)
				clear_flag(sp.stop);
#else
			clear_flag(sp2.stop);
#endif

			positiont2 = libchess::Position(search_fen);
			valid      = positiont2.game_state() == libchess::Position::GameState::IN_PROGRESS;

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
			for(auto & sp: sp2)
				memset(sp.history, 0x00, history_malloc_size);
#else
			memset(sp2.history, 0x00, history_malloc_size);
#endif

#if !defined(__ANDROID__)
			trace("# new ponder position (val: %d/ena: %d): %s\n", valid, allow_ponder, search_fen.c_str());
#endif

			prev_search_fen_version = search_fen_version;
		}
		search_fen_lock.unlock();

		if (valid && allow_ponder) {
			ponder_running = true;
#if !defined(__ANDROID__)
			trace("# ponder search start\n");
#endif

#if defined(linux) || defined(_WIN32) || defined(__ANDROID)
			int n_threads = is_ponder ? thread_count : (thread_count - 1);
			trace("# starting %d threads\n", n_threads);

			std::vector<std::thread *>        ths;
			std::optional<uint64_t>           node_limit;

			for(int i=0; i<n_threads; i++) {
				ths.push_back(new std::thread([i, node_limit] {
					auto *p = new libchess::Position(positiont2.fen());
                                        set_thread_name("PT-" + std::to_string(i));
					search_it(p, 2147483647, true, &sp2.at(i), -1, i, node_limit);
					delete p;
                                }));
			}

			for(auto & th : ths) {
				th->join();
				delete th;
			}
#else
			start_blink(led_blue_timer);
			search_it(&positiont2, 2147483647, true, &sp2, -1, 0, { });
			stop_blink(led_blue_timer, &led_blue);
#endif
			trace("# Pondering finished\n");
			ponder_running = false;
		}
		else {
			// TODO replace this by condition variables
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
			std::this_thread::sleep_for(std::chrono::microseconds(10000));
#else
			vTaskDelay(10);  // TODO divide
#endif
		}
	}

#if !defined(__ANDROID__)
	trace("# pondering thread stopping\n");
#endif

#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__)
	vTaskDelete(nullptr);
#endif
}

void start_ponder_thread()
{
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
	ponder_quit = false;

	for(auto & sp: sp2)
		clear_flag(sp.stop);

	ponder_thread_handle = new std::thread(ponder_thread, nullptr);
#else
	clear_flag(sp2.stop);

	TaskHandle_t temp;
	xTaskCreatePinnedToCore(ponder_thread, "PT", 24576, NULL, 0, &temp, 0);
	ponder_thread_handle = temp;
#endif
}

void stop_ponder_thread()
{
	trace(" *** STOP PONDER ***\n");
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
	if (ponder_thread_handle) {
		ponder_quit = true;

		for(auto & sp: sp2)
			set_flag(sp.stop);

		ponder_thread_handle->join();
		delete ponder_thread_handle;
	}
#else
	ponder_quit = true;
	set_flag(&stop2);
#endif
}

void pause_ponder()
{
	trace("# *** PAUSE PONDER ***\n");
	search_fen_lock.lock();
	search_fen.clear();
	search_fen_version++;
	search_fen_lock.unlock();
}

void set_new_ponder_position(const bool this_is_ponder)
{
	if (this_is_ponder)
		trace("# *** RESTART PONDER ***\n");
	else
		trace("# *** RESTART LAZY SMP ***\n");
	search_fen_lock.lock();
	search_fen = positiont1.fen();
	is_ponder  = this_is_ponder;
	search_fen_version++;
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
	for(auto & sp: sp2)
		set_flag(sp.stop);
#else
	set_flag(&stop2);
#endif
	search_fen_lock.unlock();
}

void main_task()
{
	std::ios_base::sync_with_stdio(true);
	std::cout.setf(std::ios::unitbuf);

	if (!sp1.history)
		printf("Malloc of sp1-history failed\n");

	sp1.parameters = &default_parameters;
	sp1.is_t2      = false;
	memset(sp1.history, 0x00, history_malloc_size);
#if defined(ESP32)
	if (!sp2.history)
		printf("Malloc of sp2-history failed\n");
#endif

	auto eval_handler = [](std::istringstream&) {
		int score = eval(positiont1, *sp1.parameters);
		printf("# eval: %d\n", score);
	};

	auto tui_handler = [](std::istringstream&) {
		printf("Invoking TUI...\n");
		run_tui();
	};

	auto fen_handler = [](std::istringstream&) {
		printf("# fen: %s\n", positiont1.fen().c_str());
	};

	auto dog_handler = [](std::istringstream&) {
		print_max_ascii();
	};

	auto display_handler = [](std::istringstream&) {
		positiont1.display();
	};

	auto status_handler = [](std::istringstream&) {
		printf("# Ponder %s\n", allow_ponder ? "enabled" : "disabled");
		printf("# Ponder %srunning\n", ponder_running ? "" : "NOT ");
		printf("# Tracing %s\n", trace_enabled ? "enabled" : "disabled");
#if defined(ESP32)
		show_esp32_info();
#endif
	};

	auto help_handler = [](std::istringstream&) {
		printf("Apart from the standard UCI commands, the following can be used:\n");
		printf("play         play game upto the end. optional parameter is think time\n");
		printf("eval         show evaluation score\n");
		printf("fen          show fen of current position\n");
		printf("d / display  show current board layout\n");
		printf("perft        perft, parameter is depth\n");
		printf("tui          switch to text interface\n");
		printf("quit         exit to main menu\n");
	};

	auto perft_handler = [](std::istringstream& line_stream) {
		std::string temp;
		line_stream >> temp;

		perft(positiont1, std::stoi(temp));
	};

	auto ucinewgame_handler = [](std::istringstream&) {
		memset(sp1.history, 0x00, history_malloc_size);
		tti.reset();
		printf("# --- New game ---\n");
	};

	auto play_handler = [](std::istringstream& line_stream) {
		try {
			int think_time = 1000;

			try {
				std::string temp;
				line_stream >> temp;
				think_time = std::stoi(temp);
			}
			catch(...) {
				printf("No or invalid think time (ms) given, using %d instead\n", think_time);
			}

			while(positiont1.game_state() == libchess::Position::GameState::IN_PROGRESS) {
				clear_flag(sp1.stop);

				tti.inc_age();

				sp1.nodes  = 0;
				sp1.qnodes  = 0;
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
				for(auto & sp: sp2)
					sp.qnodes = sp.nodes = 0;
#else
				sp1.qnodes = sp2.nodes  = 0;
#endif

#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__)
				sp1.md     = 1;
				sp2.md     = 1;
				start_blink(led_green_timer);
#endif

				// false = lazy smp
				set_new_ponder_position(false);

				auto best_move = search_it(&positiont1, think_time, true, &sp1, -1, 0, { });
#if !defined(linux) && !defined(_WIN32)
				stop_blink(led_green_timer, &led_green);
#endif

				printf("# %s %s [%d]\n", positiont1.fen().c_str(), best_move.first.to_str().c_str(), best_move.second);

				positiont1.make_move(best_move.first);
			}

			printf("\nFinished.\n");
		}
		catch(const std::exception& e) {
#if defined(__ANDROID__)
			__android_log_print(ANDROID_LOG_INFO, APPNAME, "EXCEPTION in main: %s", e.what());
#else
			printf("# EXCEPTION in main: %s\n", e.what());
#endif
		}
	};

	auto go_handler = [](const libchess::UCIGoParameters & go_parameters) {
		uint64_t start_ts = esp_timer_get_time();

		try {
			clear_flag(sp1.stop);

#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__)
			esp_start_ts = start_ts;
			sp1.md       = 1;
			sp2.md       = 1;
			sp2.nodes    = 0;
			sp2.qnodes   = 0;
#else
			for(auto & sp: sp2)
				sp.qnodes = sp.nodes = 0;
#endif
			sp1.qnodes = sp1.nodes = 0;

			tti.inc_age();

#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__)
			stop_blink(led_red_timer, &led_red);
			start_blink(led_green_timer);
#endif

			int moves_to_go = 40 - positiont1.fullmoves();

			auto depth     = go_parameters.depth();

			auto movetime = go_parameters.movetime();

			auto a_w_time = go_parameters.wtime();
			auto a_b_time = go_parameters.btime();

			int w_time = a_w_time.has_value() ? a_w_time.value() : 0;
			int b_time = a_b_time.has_value() ? a_b_time.value() : 0;

			auto a_w_inc = go_parameters.winc();
			auto a_b_inc = go_parameters.binc();

			int w_inc = a_w_inc.has_value() ? a_w_inc.value() : 0;
			int b_inc = a_b_inc.has_value() ? a_b_inc.value() : 0;

			int think_time     = 0;
			int think_time_opp = 0;

			bool is_absolute_time  = false;
			bool time_limit_hit    = false;
			bool is_white          = positiont1.side_to_move() == libchess::constants::WHITE;
			if (movetime.has_value()) {
				think_time       = movetime.value();
				is_absolute_time = true;
			}
			else {
				int  cur_n_moves  = moves_to_go <= 0 ? 40 : moves_to_go;
				int  time_inc     = is_white ? w_inc  : b_inc;
				int  time_inc_opp = is_white ? b_inc  : w_inc;
				int  ms           = is_white ? w_time : b_time;
				int  ms_opponent  = is_white ? b_time : w_time;

				think_time = (ms + (cur_n_moves - 1) * time_inc) / double(cur_n_moves + 7);
				think_time_opp = (ms_opponent + (cur_n_moves - 1) * time_inc_opp) / double(cur_n_moves + 7);

				if (think_time_opp < think_time)
					think_time += (think_time - think_time_opp) / 2;

				int limit_duration_min = ms / 15;
				if (think_time > limit_duration_min) {
					think_time = limit_duration_min;
					time_limit_hit = true;
				}

				trace("# My time: %d ms, inc: %d ms, opponent time: %d ms, inc: %d ms, full: %d, half: %d\n", ms, time_inc, ms_opponent, time_inc_opp, positiont1.fullmoves(), positiont1.halfmoves());
			}

			// let the ponder thread run as a lazy-smp thread
			set_new_ponder_position(false);

			libchess::Move best_move  { 0 };
			int            best_score { 0 };
			bool           has_best   { false };

			// probe the Syzygy endgame table base
#if defined(linux) || defined(_WIN32)
			if (with_syzygy) {
				sp1.syzygy_queries++;

				auto probe_result = probe_fathom_root(positiont1);

				if (probe_result.has_value()) {
					sp1.syzygy_query_hits++;

					best_move  = probe_result.value().first;
					best_score = probe_result.value().second;
					has_best   = true;

					trace("# Syzygy hit %s with score %d\n", best_move.to_str().c_str(), best_score);
				}
			}
#endif

			// main search
			if (!has_best)
				std::tie(best_move, best_score) = search_it(&positiont1, think_time, is_absolute_time, &sp1, depth.has_value() ? depth.value() : -1, 0, go_parameters.nodes());

			// emit result
			libchess::UCIService::bestmove(best_move.to_str());

			// no longer thinking
#if !defined(linux) && !defined(_WIN32)
			stop_blink(led_green_timer, &led_green);
#endif
#if defined(__ANDROID__)
			__android_log_print(ANDROID_LOG_INFO, APPNAME, "Performed move %s for position %s", best_move.to_str().c_str(), positiont1.fen().c_str());
#endif

			// set ponder positition
			positiont1.make_move(best_move);
			uint64_t end_ts = esp_timer_get_time();
			trace("# Think time: %d ms, used %.3f ms (%s, %d halfmoves, %d fullmoves, TL: %d)\n", think_time, (end_ts - start_ts) / 1000., is_white ? "white" : "black", positiont1.halfmoves(), positiont1.fullmoves(), time_limit_hit);
			set_new_ponder_position(true);
			positiont1.unmake_move();
		}
		catch(const std::exception& e) {
#if defined(__ANDROID__)
			__android_log_print(ANDROID_LOG_INFO, APPNAME, "EXCEPTION in main: %s", e.what());
#else
			printf("# EXCEPTION in main: %s\n", e.what());
#endif
		}
	};

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
	libchess::UCISpinOption thread_count_option("Threads", thread_count, 1, 65536, thread_count_handler);
	uci_service.register_option(thread_count_option);
#endif

	libchess::UCICheckOption allow_ponder_option("Ponder", true, allow_ponder_handler);
	uci_service.register_option(allow_ponder_option);
	libchess::UCICheckOption allow_tracing_option("Trace", true, allow_tracing_handler);
	uci_service.register_option(allow_tracing_option);
	libchess::UCIStringOption commerial_option("UCI_EngineAbout", "https://vanheusden.com/chess/Dog/", commerial_option_handler);
	uci_service.register_option(commerial_option);

	uci_service.register_position_handler(position_handler);
	uci_service.register_go_handler      (go_handler);
	uci_service.register_stop_handler    (stop_handler);

	uci_service.register_handler("play",       play_handler, true);
	uci_service.register_handler("eval",       eval_handler, true);
	uci_service.register_handler("fen",        fen_handler, true);
	uci_service.register_handler("d",          display_handler, true);
	uci_service.register_handler("display",    display_handler, true);
	uci_service.register_handler("dog",        dog_handler, false);
	uci_service.register_handler("max",        dog_handler, false);
	uci_service.register_handler("perft",      perft_handler, true);
	uci_service.register_handler("ucinewgame", ucinewgame_handler, true);
	uci_service.register_handler("tui",        tui_handler, true);
	uci_service.register_handler("status",     status_handler, false);
	uci_service.register_handler("help",       help_handler, false);

	for(;;) {
		my_printf("# ENTER \"uci\" FOR uci-MODE, OR \"tui\" FOR A TEXT INTERFACE\n");
		my_printf("# \"test\" will run the unit tests, \"quit\" terminate the application\n");

		std::string line;
		std::getline(is, line);

		if (line == "uci") {
			uci_service.run();
			break;  // else lichess-bot will break
		}
		else if (line == "tui") {
			printf("Invoking TUI...\n");
			run_tui();
			printf("Waiting for \"tui\" or \"uci\"...\n");
		}
		else if (line == "test")
			run_tests();
		else if (line == "quit") {
			break;
		}
	}

	stop_ponder_thread();

	printf("TASK TERMINATED\n");
}

void hello() {
#if defined(__ANDROID__)
	__android_log_print(ANDROID_LOG_INFO, APPNAME, "HELLO, THIS IS DOG");
#else
	my_printf("\n\n\n# HELLO, THIS IS DOG\n\n");
	my_printf("# compiled on " __DATE__ " " __TIME__ "\n\n");
	my_printf("# Dog is a chess program written by Folkert van Heusden <mail@vanheusden.com>.\n");
#endif
}

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
void help()
{
	print_max();

	printf("-t x   thread count\n");
	printf("-U     run unit tests\n");
	printf("-s x   set path to Syzygy\n");
	printf("-u x   USB display device\n");
}

int main(int argc, char *argv[])
{
#if !defined(_WIN32)
	signal(SIGPIPE, SIG_IGN);
#endif

	hello();

#if !defined(__ANDROID__)
	int c = -1;
	while((c = getopt(argc, argv, "t:T:s:u:Uh")) != -1) {
		if (c == 'T') {
			tune(optarg);
			return 0;
		}

		if (c == 'U') {
			run_tests();
			return 1;
		}

		if (c == 't')
			thread_count = atoi(optarg);
		else if (c == 's') {
			fathom_init(optarg);

			with_syzygy = true;
		}
#if !defined(_WIN32)
		else if (c == 'u')
			usb_disp_thread = new std::thread(usb_disp, optarg);
#endif
		else {
			help();

			return c == 'h' ? 0 : 1;
		}
	}
#endif

	for(int i=0; i<thread_count; i++) {
		stop2.push_back(new end_t());
		sp2.push_back({ nullptr, true, reinterpret_cast<int16_t *>(malloc(history_malloc_size)), 0, 0, 0, stop2.at(i) });
	}

	start_ponder_thread();

	setvbuf(stdout, nullptr, _IONBF, 0);

	main_task();

	if (with_syzygy)
		fathom_deinit();

	while(!stop2.empty()) {
		delete *stop2.begin();

		stop2.erase(stop2.begin());
	}

	for(size_t i=0; i<sp2.size(); i++)
		free(sp2.at(i).history);

	free(sp1.history);

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

	// esp_task_wdt_config_t wdtcfg { .timeout_ms = 30000, .idle_core_mask = uint32_t(~0), .trigger_panic = false };
	// esp_task_wdt_init(&wdtcfg);
	esp_task_wdt_init(30, false);

	esp_timer_create(&led_green_timer_pars, &led_green_timer);
	esp_timer_create(&led_blue_timer_pars,  &led_blue_timer );
	esp_timer_create(&led_red_timer_pars,   &led_red_timer  );

	esp_timer_create(&think_timeout_pars, &think_timeout_timer);

	// configure UART1 (2nd uart)
	uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.rx_flow_ctrl_thresh = 122,
	};
	ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(uart_num, 16, 17, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	constexpr int uart_buffer_size = 1024 * 2;
	if (uart_is_driver_installed(uart_num))
		printf("UART ALREADY INSTALLED\n");
	ESP_ERROR_CHECK(uart_driver_install(uart_num, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));

	// flash filesystem
	esp_vfs_spiffs_conf_t conf = {
		.base_path       = "/spiffs",
		.partition_label = nullptr,
		.max_files       = 5,
		.format_if_mount_failed = true
	};
	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK) {
		if (ret == ESP_FAIL)
			printf("Failed to mount or format filesystem\n");
		else if (ret == ESP_ERR_NOT_FOUND)
			printf("Failed to find SPIFFS partition\n");
		else
			printf("Failed to initialize SPIFFS (%s)\n", esp_err_to_name(ret));
		printf("Did you run \"pio run -t uploadfs\"?\n");
	}

	hello();

	gpio_set_level(LED_INTERNAL, 0);

	start_ponder_thread();

	main_task();

	start_blink(led_red_timer);
	esp_restart();
}
#endif
