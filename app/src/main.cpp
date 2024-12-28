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
#include "search.h"
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
search_pars_t sp1   { nullptr, false, reinterpret_cast<int16_t *>(malloc(history_malloc_size)), nullptr, 0, &stop1 };

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
std::vector<search_pars_t> sp2;
std::vector<end_t *>       stop2;

std::thread *usb_disp_thread = nullptr;
#else
end_t         stop2 { false };
search_pars_t sp2   { nullptr, true,  reinterpret_cast<int16_t *>(malloc(history_malloc_size)), nullptr, 0, &stop2 };
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
		sp2.push_back({ nullptr, true, reinterpret_cast<int16_t *>(malloc(history_malloc_size)), nullptr, 0, stop2.at(i) });
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

void reset_stats(chess_stats_t *const cs)
{
	memset(cs, 0x00, sizeof(chess_stats_t));
}

void sum_stats(const chess_stats_t *const source, chess_stats_t *const target)
{
	target->nodes    += source->nodes;
	target->qnodes   += source->qnodes;
	target->n_draws  += source->n_draws;

        target->tt_query += source->tt_query;
        target->tt_hit   += source->tt_hit;
        target->tt_store += source->tt_store;

	target->n_null_move     += source->n_null_move;
	target->n_null_move_hit += source->n_null_move_hit;

	target->n_lmr     += source->n_lmr;
	target->n_lmr_hit += source->n_lmr_hit;

	target->n_static_eval     += source->n_static_eval;
	target->n_static_eval_hit += source->n_static_eval_hit;

	target->n_moves_cutoff  += source->n_moves_cutoff;
	target->nmc_nodes       += source->nmc_nodes;
	target->n_qmoves_cutoff += source->n_qmoves_cutoff;
	target->nmc_qnodes      += source->nmc_qnodes;
}

void reset_search_statistics()
{
	reset_stats(sp1.cs);
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
        for(auto & sp: sp2)
                reset_stats(sp.cs);
#else
        reset_stats(sp2.cs);
#endif
}

chess_stats_t calculate_search_statistics()
{
        chess_stats_t out { };

        sum_stats(sp1.cs, &out);
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
        for(auto & sp: sp2)
                sum_stats(sp.cs, &out);
#else
        sum_stats(sp2.cs, &out);
#endif

        return out;
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

	sp1.cs = new chess_stats_t();
	for(auto & sp: sp2)
		sp.cs = new chess_stats_t();

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
				reset_search_statistics();

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
#endif
			reset_search_statistics();
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
				sp1.cs->syzygy_queries++;

				auto probe_result = probe_fathom_root(positiont1);

				if (probe_result.has_value()) {
					sp1.cs->syzygy_query_hits++;

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
		sp2.push_back({ nullptr, true, reinterpret_cast<int16_t *>(malloc(history_malloc_size)), nullptr, 0, stop2.at(i) });
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
