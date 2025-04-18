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

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
#include "max.h"
#include <chrono>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <thread>
#include <unistd.h>
#include <sys/time.h>

#if !defined(ESP32)
#include "state_exporter.h"
#endif
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
#include "inbuf.h"
#include "main.h"
#include "max-ascii.h"
#include "nnue.h"
#include "psq.h"
#include "search.h"
#include "str.h"
#if defined(linux) || defined(_WIN32) || defined(__APPLE__)
#include "syzygy.h"
bool with_syzygy = false;
#endif
#include "test.h"
#include "tt.h"
#include "tui.h"


std::vector<search_pars_t *> sp;
#if !defined(ESP32)
state_exporter              *se { nullptr };
#endif

static_assert(sizeof(int) >= 4, "INT should be at least 32 bit");

#if defined(linux) || defined(_WIN32) || defined(__APPLE__)
std::string my_trace_file;
#endif
bool trace_enabled = false;
auto allow_tracing_handler = [](const bool value) {
	trace_enabled = value;
	printf("# Tracing %s\n", value ? "enabled" : "disabled");
};
void my_trace(const char *const fmt, ...)
{
	if (trace_enabled) {
		va_list ap { };
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
#if !defined(ESP32)
	if (my_trace_file.empty() == false) {
		FILE *fh = fopen(my_trace_file.c_str(), "a+");
		if (fh) {
			uint64_t now = esp_timer_get_time();  // is gettimeofday
			time_t t = now / 1000000;
			tm *tm = localtime(&t);
			fprintf(fh, "[%d] %04d-%02d-%02d %02d:%02d:%02d.%06d ", getpid(),
					tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
					tm->tm_hour, tm->tm_min, tm->tm_sec,
					now % 1000000);

			va_list ap { };
			va_start(ap, fmt);
			vfprintf(fh, fmt, ap);
			va_end(ap);

			fclose(fh);
		}
		else {
			fprintf(stderr, "Cannot access %s: %s\n", my_trace_file.c_str(), strerror(errno));
		}
	}
#endif
}

void set_flag(end_t *const stop)
{
	std::unique_lock<std::mutex> lck(stop->cv_lock);
	stop->flag = true;
	stop->cv.notify_all();
}

void clear_flag(end_t *const stop)
{
	std::unique_lock<std::mutex> lck(stop->cv_lock);
	stop->flag = false;
}

auto stop_handler = []()
{
	for(auto & i: sp)
		set_flag(i->stop);
#if !defined(__ANDROID__)
	my_trace("# stop_handler invoked\n");
#endif
};

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
uint64_t esp_timer_get_time()
{
	timeval tv { };
	gettimeofday(&tv, nullptr);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif

void think_timeout(void *arg)
{
	end_t *stop = reinterpret_cast<end_t *>(arg);
	if (stop)
		set_flag(stop);
}

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
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

esp_timer_create_args_t think_timeout_pars = {
            .callback = &think_timeout,
            .arg      = nullptr,
            .name     = "searchto"
};

void blink_led(void *arg)
{
	led_t *l = reinterpret_cast<led_t *>(arg);
	gpio_set_level(l->pin_nr, l->state);
// doesn't work with VT510? --> too much data via serial connection
//	if (l->screen_x != -1 && t == T_VT100)
//		my_printf("\x1b7\x1b[1;%dH\x1b[1m%d\x1b8", l->screen_x, l->state);
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

led_t led_green { LED_GREEN, false, 79};

const esp_timer_create_args_t led_green_timer_pars = {
            .callback = &blink_led,
            .arg = &led_green,
            .name = "greenled"
};

led_t led_blue { LED_BLUE, false, 78 };

const esp_timer_create_args_t led_blue_timer_pars = {
            .callback = &blink_led,
            .arg = &led_blue,
            .name = "blueled"
};

led_t led_red { LED_RED, false, 77 };

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

void set_thread_name(std::string name)
{
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
        if (name.length() > 15)
                name = name.substr(0, 15);

        pthread_setname_np(pthread_self(), name.c_str());
#elif defined(__APPLE__)
        pthread_setname_np(name.c_str());
#endif
}

inbuf i;
std::istream is(&i);

// TODO replace by messages
struct {
	std::mutex              search_fen_lock;
	bool                    reconfigure_threads  { false };
	std::condition_variable search_cv;
	int                     search_version       { -1    };
	int                     search_think_time    { 0     };
	bool                    search_is_abs_time   { false };
	int                     search_max_depth;
	std::optional<uint64_t> search_max_n_nodes;
	std::condition_variable search_cv_finished;
	std::optional<libchess::Move> search_best_move;
	int                     search_best_score    { 0     };
	bool                    search_output        { false };
	int                     search_count_running { 0     };
} work;

void searcher(const int i)
{
	printf("Thread %d started\n", i);

	int last_fen_version = -1;

	for(;;) {
		std::unique_lock<std::mutex> search_lck(work.search_fen_lock);
		while(work.search_version == last_fen_version && work.reconfigure_threads == false)
			work.search_cv.wait(search_lck);

		if (work.reconfigure_threads)
			break;

		last_fen_version = work.search_version;

		int  local_search_think_time  = work.search_think_time;
		bool local_search_is_abs_time = work.search_is_abs_time;
		int  local_search_max_depth   = work.search_max_depth;
		auto local_search_max_n_nodes = work.search_max_n_nodes;
		bool local_search_output      = work.search_output;

		work.search_count_running++;

		clear_flag(sp.at(i)->stop);
		search_lck.unlock();

		// search!
		libchess::Move best_move;
		int            best_score { 0 };
		std::tie(best_move, best_score) = search_it(local_search_think_time, local_search_is_abs_time, sp.at(i), local_search_max_depth, local_search_max_n_nodes, i == 0 && local_search_output);

		// notify finished
		search_lck.lock();

		if (i == 0) {
			work.search_best_move  = best_move;
			work.search_best_score = best_score;

			// stop other threads
			for(auto & thread_pars : sp)
				set_flag(thread_pars->stop);
		}

		work.search_count_running--;
		work.search_cv_finished.notify_one();
	}

	printf("Thread %d finished\n", i);
}

void prepare_threads_state()
{
	clear_flag(sp.at(0)->stop);
#if defined(ESP32)
	sp.at(0)->md = 1;
#endif
	init_move(sp.at(0)->nnue_eval, sp.at(0)->pos);
	for(size_t i=1; i<sp.size(); i++) {
		sp.at(i)->pos = sp.at(0)->pos;
		init_move(sp.at(i)->nnue_eval, sp.at(i)->pos);
#if defined(ESP32)
		sp.at(i)->md  = 1;
#endif
		clear_flag(sp.at(i)->stop);
	}
}

void start_ponder()
{
	my_trace("# start ponder\n");
	std::unique_lock<std::mutex> lck(work.search_fen_lock);

	prepare_threads_state();

	work.search_think_time  = -1;
	work.search_is_abs_time = false;
	work.search_max_depth   = -1;
	work.search_max_n_nodes.reset();
	work.search_version++;
	work.search_best_move.reset();
	work.search_best_score = -32768;
	work.search_output     = false;
	work.search_cv.notify_all();
	my_trace("# ponder started\n");

	if (t != T_ASCII) {
		store_cursor_position();
		my_printf("\x1b[1;80H\x1b[1;5;7mP");
		restore_cursor_position();
	}
}

void stop_ponder()
{
	my_trace("# stop ponder\n");

	{
		std::unique_lock<std::mutex> lck(work.search_fen_lock);

		for(auto & i: sp)
			set_flag(i->stop);

		work.search_cv.notify_all();

		while(work.search_count_running != 0)
			work.search_cv_finished.wait(lck);
	}

	if (t != T_ASCII) {
		store_cursor_position();
		my_printf("\x1b[1;80H\x1b[1;5;7m-");
		restore_cursor_position();
	}

	my_trace("# ponder stopped\n");
}

void delete_threads()
{
#if !defined(ESP32)
	if (se)
		se->clear();
#endif

	{
		std::unique_lock<std::mutex> lck(work.search_fen_lock);
		work.reconfigure_threads = true;

		for(auto & i: sp)
			set_flag(i->stop);

		work.search_cv.notify_all();
	}

	for(auto & i: sp) {
		i->thread_handle->join();
		delete i->thread_handle;
		delete i->nnue_eval;
		delete i->stop;
		free(i->history);
		delete i;
	}

	sp.clear();

	// no locking required here: no threads running!
	work.reconfigure_threads = false;
}

void allocate_threads(const int n)
{
	delete_threads();

	for(int i=0; i<n; i++) {
		sp.push_back(new search_pars_t({ reinterpret_cast<int16_t *>(calloc(1, history_malloc_size)), new end_t, i }));
		sp.at(i)->thread_handle = new std::thread(searcher, i);
		sp.at(i)->nnue_eval = new Eval(sp.at(i)->pos);
	}
#if !defined(ESP32)
	if (se)
		se->set(sp.at(0));
#endif
#if defined(ESP32)
	if (n > 0) {
		think_timeout_pars.arg = sp.at(0)->stop;
		static bool first = true;
		if (first)
			first = false;
		else
			esp_timer_delete(think_timeout_timer);
		esp_timer_create(&think_timeout_pars, &think_timeout_timer);
	}
#endif
}

auto thread_count_handler = [](const int value)  {
	allocate_threads(value);
};

auto hash_size_handler = [](const int value)  {
	tti.set_size(uint64_t(value) * 1024 * 1024);
};

bool allow_ponder         = false;
auto allow_ponder_handler = [](const bool value) {
	allow_ponder = value;
	printf("# Ponder %s\n", value ? "enabled" : "disabled");
};

auto commerial_option_handler = [](const std::string & value) { };

auto opponent_option_handler = [](const std::string & value) {
	my_trace("# Playing against %s\n", value.c_str());
};

#if defined(ESP32)
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
				my_trace("# %s\t%u%%\t%u\n",
						pxTaskStatusArray[x].pcTaskName,
						ulStatsAsPercentage,
						pxTaskStatusArray[x].usStackHighWaterMark);
			}
			else {
				my_trace("# %s\t%u\n",
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
int check_min_stack_size(const int nr, const search_pars_t & sp)
{
	UBaseType_t level = uxTaskGetStackHighWaterMark(nullptr);

	my_trace("# dts: %lld depth %d nodes %u lower_bound: %d, task name: %s\n", esp_timer_get_time() - esp_start_ts, sp.md, sp.cs.data.nodes, level, pcTaskGetName(xTaskGetCurrentTaskHandle()));

	if (level < 768) {
		set_flag(sp.stop);
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

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
void gpio_set_level(int a, int b)
{
}
#endif

void reset_search_statistics()
{
        for(auto & i: sp)
                i->cs.reset();
}

chess_stats calculate_search_statistics()
{
        chess_stats out { };

        for(auto & i: sp)
		out.add(i->cs);

        return out;
}

std::pair<uint64_t, uint64_t> simple_search_statistics()
{
	uint64_t syzygy_query_hits = 0, nodes_visited = 0;

        for(auto & i: sp) {
		syzygy_query_hits += i->cs.data.syzygy_query_hits;
		nodes_visited += i->cs.data.nodes + i->cs.data.qnodes;
	}

	return { nodes_visited, syzygy_query_hits };
}

void main_task()
{
	libchess::UCIService *uci_service = new libchess::UCIService("Dog v" DOG_VERSION, "Folkert van Heusden", std::cout, is);

	std::ios_base::sync_with_stdio(true);
	std::cout.setf(std::ios::unitbuf);

	init_lmr();

	chess_stats global_cs;

	auto eval_handler = [](std::istringstream&) {
		int score = nnue_evaluate(sp.at(0)->nnue_eval, sp.at(0)->pos);
		printf("# eval: %d\n", score);
	};

	auto tui_handler = [](std::istringstream&) {
		printf("Invoking TUI...\n");
		run_tui();
	};

	auto fen_handler = [](std::istringstream&) {
		printf("# fen: %s\n", sp.at(0)->pos.fen().c_str());
	};

	auto dog_handler = [](std::istringstream&) {
		print_max_ascii();
	};

	auto display_handler = [](std::istringstream&) {
		sp.at(0)->pos.display();
	};

	auto status_handler = [](std::istringstream&) {
		printf("# Ponder %s\n",  allow_ponder  ? "enabled" : "disabled");
		printf("# Tracing %s\n", trace_enabled ? "enabled" : "disabled");
#if defined(ESP32)
		show_esp32_info();
#endif
	};

	auto help_handler = [](std::istringstream&) {
		printf("Apart from the standard UCI commands, the following can be used:\n");
		printf("play         play game upto the end. optional parameter is think time or depth if preceeded with \"depth\"\n");
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

		perft(sp.at(0)->pos, std::stoi(temp));
	};

	auto ucinewgame_handler = [&global_cs](std::istringstream&) {
		stop_ponder();
		for(auto & i: sp)
			memset(i->history, 0x00, history_malloc_size);
		global_cs.reset();
		tti.reset();
	};

	auto position_handler = [](const libchess::UCIPositionParameters & position_parameters) {
		stop_ponder();
		sp.at(0)->pos = libchess::Position { position_parameters.fen() };
		init_move(sp.at(0)->nnue_eval, sp.at(0)->pos);
		if (position_parameters.move_list()) {
			for (auto & move_str : position_parameters.move_list()->move_list())
				make_move(sp.at(0)->nnue_eval, sp.at(0)->pos, *str_to_move(sp.at(0)->pos, move_str));
		}
	};

	auto play_handler = [](std::istringstream& line_stream) {
		try {
			std::optional<int> think_time;
			std::optional<int> max_depth;

			try {
				std::string temp;
				line_stream >> temp;

				if (temp == "depth") {
					line_stream >> temp;
					max_depth = std::stoi(temp);
				}
				else {
					think_time = std::stoi(temp);
				}
			}
			catch(...) {
				printf("No or invalid think time (ms) given, using %d instead\n", think_time.value());
			}

			init_move(sp.at(0)->nnue_eval, sp.at(0)->pos);

			chess_stats cs_sum;
			while(sp.at(0)->pos.game_state() == libchess::Position::GameState::IN_PROGRESS) {
				reset_search_statistics();

				prepare_threads_state();

#if defined(ESP32)
				start_blink(led_green_timer);
#endif

				// put
				{
					std::unique_lock<std::mutex> lck(work.search_fen_lock);
					work.search_think_time  = think_time.has_value() ? think_time.value() : -1;
					work.search_is_abs_time = true;
					work.search_max_depth   = max_depth.has_value() ? max_depth.value() : - 1;
					work.search_max_n_nodes.reset();
					work.search_version++;
					work.search_best_move.reset();
					work.search_best_score = -32768;
					work.search_output     = true;
					work.search_cv.notify_all();
				}
				// get
				{
					std::unique_lock<std::mutex> lck(work.search_fen_lock);
					while(work.search_best_move.has_value() == false || work.search_count_running != 0)
						work.search_cv_finished.wait(lck);
				}
#if defined(ESP32)
				stop_blink(led_green_timer, &led_green);
#endif
				cs_sum.add(calculate_search_statistics());

				printf("# %s %s [%d]\n", sp.at(0)->pos.fen().c_str(), work.search_best_move.value().to_str().c_str(), work.search_best_score);

				make_move(sp.at(0)->nnue_eval, sp.at(0)->pos, work.search_best_move.value());
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

	auto go_handler = [&global_cs](const libchess::UCIGoParameters & go_parameters) {
		uint64_t start_ts = esp_timer_get_time();

		try {
			reset_search_statistics();

			stop_ponder();

			my_trace("# position: %s\n", sp.at(0)->pos.fen().c_str());

#if defined(ESP32)
			esp_start_ts = start_ts;
			stop_blink(led_red_timer, &led_red);
			start_blink(led_green_timer);
#endif

			int moves_to_go = 40 - sp.at(0)->pos.fullmoves();

			auto depth     = go_parameters.depth();
			auto nodes     = go_parameters.nodes();

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

			bool is_absolute_time = false;
			bool is_white         = sp.at(0)->pos.side_to_move() == libchess::constants::WHITE;
			if (movetime.has_value()) {
				think_time       = movetime.value();
				is_absolute_time = true;
			}
			else {
				int cur_n_moves  = moves_to_go <= 0 ? 40 : moves_to_go;
				int time_inc     = is_white ? w_inc  : b_inc;
				int time_inc_opp = is_white ? b_inc  : w_inc;
				int ms           = is_white ? w_time : b_time;
				int ms_opponent  = is_white ? b_time : w_time;

				think_time = (ms + (cur_n_moves - 1) * time_inc) / double(cur_n_moves + 7);

				int limit_duration_min = ms / 15;
				if (think_time > limit_duration_min)
					think_time = limit_duration_min;

				my_trace("# My time: %d ms, inc: %d ms, opponent time: %d ms, inc: %d ms, full: %d, half: %d, moves_to_go: %d, tt: %d\n", ms, time_inc, ms_opponent, time_inc_opp, sp.at(0)->pos.fullmoves(), sp.at(0)->pos.halfmoves(), moves_to_go, tti.get_per_mille_filled());
			}

			libchess::Move  best_move;
			int  best_score { 0 };
			bool has_best   { false };

			// probe the Syzygy endgame table base
#if defined(linux) || defined(_WIN32) || defined(__APPLE__)
			if (with_syzygy) {
				sp.at(0)->cs.data.syzygy_queries++;

				auto probe_result = probe_fathom_root(sp.at(0)->pos);
				if (probe_result.has_value()) {
					sp.at(0)->cs.data.syzygy_query_hits++;

					best_move  = probe_result.value().first;
					best_score = probe_result.value().second;
					has_best   = true;

					my_trace("# Syzygy hit %s with score %d\n", best_move.to_str().c_str(), best_score);
				}
			}
#endif

			// main search
			if (!has_best) {
				// put
				{
					std::unique_lock<std::mutex> lck(work.search_fen_lock);

					prepare_threads_state();

					work.search_think_time  = depth.has_value() && think_time == 0 ? -1 : think_time;
					work.search_is_abs_time = is_absolute_time;
					work.search_max_depth   = depth.has_value() ? depth.value() : -1;
					work.search_max_n_nodes = nodes;
					work.search_version++;
					work.search_best_move.reset();
					work.search_best_score = -32768;
					work.search_output     = true;
					work.search_cv.notify_all();
				}

				// get
				{
					std::unique_lock<std::mutex> lck(work.search_fen_lock);

					while(work.search_best_move.has_value() == false || work.search_count_running != 0)
						work.search_cv_finished.wait(lck);

					best_move  = work.search_best_move.value();
					best_score = work.search_best_score;
				}
			}

			// emit result
			libchess::UCIService::bestmove(best_move.to_str());

			my_trace("info string had %d ms, used %.3f ms (including overhead)\n", think_time, (esp_timer_get_time() - start_ts) / 1000.);

			// no longer thinking
#if defined(ESP32)
			stop_blink(led_green_timer, &led_green);
#endif
#if defined(__ANDROID__)
			__android_log_print(ANDROID_LOG_INFO, APPNAME, "Performed move %s for position %s", best_move.to_str().c_str(), sp.at(0)->pos.fen().c_str());
#endif

			global_cs.add(calculate_search_statistics());

			if (allow_ponder)
				start_ponder();
		}
		catch(const std::exception& e) {
#if defined(__ANDROID__)
			__android_log_print(ANDROID_LOG_INFO, APPNAME, "EXCEPTION in main: %s", e.what());
#else
			printf("# EXCEPTION in main: %s\n", e.what());
#endif
		}
	};

#if defined(ESP32)
	libchess::UCISpinOption thread_count_option("Threads", sp.size(), 1, 2, thread_count_handler);
#else
	libchess::UCISpinOption thread_count_option("Threads", sp.size(), 1, 65536, thread_count_handler);
#endif
	uci_service->register_option(thread_count_option);
	libchess::UCISpinOption hash_size_option("Hash", tti.get_size(), 1, 1024, hash_size_handler);
	uci_service->register_option(hash_size_option);
	libchess::UCICheckOption allow_ponder_option("Ponder", allow_ponder, allow_ponder_handler);
	uci_service->register_option(allow_ponder_option);
	libchess::UCICheckOption allow_tracing_option("Trace", trace_enabled, allow_tracing_handler);
	uci_service->register_option(allow_tracing_option);
	libchess::UCIStringOption commerial_option("UCI_EngineAbout", "https://vanheusden.com/chess/Dog/", commerial_option_handler);
	uci_service->register_option(commerial_option);
	libchess::UCIStringOption opponent_option("UCI_Opponent", "", opponent_option_handler);
	uci_service->register_option(opponent_option);

	uci_service->register_position_handler(position_handler);
	uci_service->register_go_handler      (go_handler);
	uci_service->register_stop_handler    (stop_handler);

	uci_service->register_handler("play",       play_handler, true);
	uci_service->register_handler("eval",       eval_handler, true);
	uci_service->register_handler("fen",        fen_handler, true);
	uci_service->register_handler("d",          display_handler, true);
	uci_service->register_handler("display",    display_handler, true);
	uci_service->register_handler("dog",        dog_handler, false);
	uci_service->register_handler("max",        dog_handler, false);
	uci_service->register_handler("perft",      perft_handler, true);
	uci_service->register_handler("ucinewgame", ucinewgame_handler, true);
	uci_service->register_handler("tui",        tui_handler, true);
	uci_service->register_handler("status",     status_handler, false);
	uci_service->register_handler("help",       help_handler, false);

	for(;;) {
		my_printf("# ENTER \"uci\" FOR uci-MODE, OR \"tui\" FOR A TEXT INTERFACE\n");
		my_printf("# \"test\" will run the unit tests, \"quit\" terminate the application\n");

		std::string line;
		std::getline(is, line);

		if (line == "uci") {
			uci_service->run();
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

	delete_threads();

	delete uci_service;

	printf("TASK TERMINATED\n");
}

void hello() {
#if defined(__ANDROID__)
	__android_log_print(ANDROID_LOG_INFO, APPNAME, "HELLO, THIS IS DOG");
#else
	my_printf("\n\n\n# HELLO, THIS IS DOG\n\n");
#if defined(GIT_VERSION)
	my_printf("# Version " DOG_VERSION ", compiled on " __DATE__ " " __TIME__ ", GIT version: " GIT_VERSION "\n\n");
#else
	my_printf("# Version " DOG_VERSION ", compiled on " __DATE__ " " __TIME__ "\n\n");
#endif
	my_printf("# Dog is a chess program written by Folkert van Heusden <mail@vanheusden.com>.\n");
#endif
}

void run_bench()
{
	init_lmr();

	allocate_threads(1);

        // these fens are taken from https://github.com/lynx-chess/Lynx/blob/main/src/Lynx/Bench.cs
        const std::vector<std::string> fens {
                "r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq a6 0 14",
                "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 2 24",
                "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 16 42",
                "6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2 b - - 4 44",
                "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54",
                "7r/2p3k1/1p1p1qp1/1P1Bp3/p1P2r1P/P7/4R3/Q4RK1 w - - 0 36",
                "r1bq1rk1/pp2b1pp/n1pp1n2/3P1p2/2P1p3/2N1P2N/PP2BPPP/R1BQ1RK1 b - - 2 10",
                "3r3k/2r4p/1p1b3q/p4P2/P2Pp3/1B2P3/3BQ1RP/6K1 w - - 3 87",
                "2r4r/1p4k1/1Pnp4/3Qb1pq/8/4BpPp/5P2/2RR1BK1 w - - 0 42",
                "4q1bk/6b1/7p/p1p4p/PNPpP2P/KN4P1/3Q4/4R3 b - - 0 37",
                "2q3r1/1r2pk2/pp3pp1/2pP3p/P1Pb1BbP/1P4Q1/R3NPP1/4R1K1 w - - 2 34",
                "1r2r2k/1b4q1/pp5p/2pPp1p1/P3Pn2/1P1B1Q1P/2R3P1/4BR1K b - - 1 37",
                "r3kbbr/pp1n1p1P/3ppnp1/q5N1/1P1pP3/P1N1B3/2P1QP2/R3KB1R b KQkq b3 0 17",
                "8/6pk/2b1Rp2/3r4/1R1B2PP/P5K1/8/2r5 b - - 16 42",
                "1r4k1/4ppb1/2n1b1qp/pB4p1/1n1BP1P1/7P/2PNQPK1/3RN3 w - - 8 29",
                "8/p2B4/PkP5/4p1pK/4Pb1p/5P2/8/8 w - - 29 68",
                "3r4/ppq1ppkp/4bnp1/2pN4/2P1P3/1P4P1/PQ3PBP/R4K2 b - - 2 20",
                "5rr1/4n2k/4q2P/P1P2n2/3B1p2/4pP2/2N1P3/1RR1K2Q w - - 1 49",
                "1r5k/2pq2p1/3p3p/p1pP4/4QP2/PP1R3P/6PK/8 w - - 1 51",
                "q5k1/5ppp/1r3bn1/1B6/P1N2P2/BQ2P1P1/5K1P/8 b - - 2 34",
                "r1b2k1r/5n2/p4q2/1ppn1Pp1/3pp1p1/NP2P3/P1PPBK2/1RQN2R1 w - - 0 22",
                "r1bqk2r/pppp1ppp/5n2/4b3/4P3/P1N5/1PP2PPP/R1BQKB1R w KQkq - 0 5",
                "r1bqr1k1/pp1p1ppp/2p5/8/3N1Q2/P2BB3/1PP2PPP/R3K2n b Q - 1 12",
                "r1bq2k1/p4r1p/1pp2pp1/3p4/1P1B3Q/P2B1N2/2P3PP/4R1K1 b - - 2 19",
                "r4qk1/6r1/1p4p1/2ppBbN1/1p5Q/P7/2P3PP/5RK1 w - - 2 25",
                "r7/6k1/1p6/2pp1p2/7Q/8/p1P2K1P/8 w - - 0 32",
                "r3k2r/ppp1pp1p/2nqb1pn/3p4/4P3/2PP4/PP1NBPPP/R2QK1NR w KQkq - 1 5",
                "3r1rk1/1pp1pn1p/p1n1q1p1/3p4/Q3P3/2P5/PP1NBPPP/4RRK1 w - - 0 12",
                "5rk1/1pp1pn1p/p3Brp1/8/1n6/5N2/PP3PPP/2R2RK1 w - - 2 20",
                "8/1p2pk1p/p1p1r1p1/3n4/8/5R2/PP3PPP/4R1K1 b - - 3 27",
                "8/4pk2/1p1r2p1/p1p4p/Pn5P/3R4/1P3PP1/4RK2 w - - 1 33",
                "8/5k2/1pnrp1p1/p1p4p/P6P/4R1PK/1P3P2/4R3 b - - 1 38",
                "8/8/1p1kp1p1/p1pr1n1p/P6P/1R4P1/1P3PK1/1R6 b - - 15 45",
                "8/8/1p1k2p1/p1prp2p/P2n3P/6P1/1P1R1PK1/4R3 b - - 5 49",
                "8/8/1p4p1/p1p2k1p/P2npP1P/4K1P1/1P6/3R4 w - - 6 54",
                "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/6R1 b - - 6 59",
                "8/5k2/1p4p1/p1pK3p/P2n1P1P/6P1/1P6/4R3 b - - 14 63",
                "8/1R6/1p1K1kp1/p6p/P1p2P1P/6P1/1Pn5/8 w - - 0 67",
                "1rb1rn1k/p3q1bp/2p3p1/2p1p3/2P1P2N/PP1RQNP1/1B3P2/4R1K1 b - - 4 23",
                "4rrk1/pp1n1pp1/q5p1/P1pP4/2n3P1/7P/1P3PB1/R1BQ1RK1 w - - 3 22",
                "r2qr1k1/pb1nbppp/1pn1p3/2ppP3/3P4/2PB1NN1/PP3PPP/R1BQR1K1 w - - 4 12",
                "2r2k2/8/4P1R1/1p6/8/P4K1N/7b/2B5 b - - 0 55",
                "6k1/5pp1/8/2bKP2P/2P5/p4PNb/B7/8 b - - 1 44",
                "2rqr1k1/1p3p1p/p2p2p1/P1nPb3/2B1P3/5P2/1PQ2NPP/R1R4K w - - 3 25",
                "r1b2rk1/p1q1ppbp/6p1/2Q5/8/4BP2/PPP3PP/2KR1B1R b - - 2 14",
                "6r1/5k2/p1b1r2p/1pB1p1p1/1Pp3PP/2P1R1K1/2P2P2/3R4 w - - 1 36",
                "rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2",
                "2rr2k1/1p4bp/p1q1p1p1/4Pp1n/2PB4/1PN3P1/P3Q2P/2RR2K1 w - f6 0 20",
                "3br1k1/p1pn3p/1p3n2/5pNq/2P1p3/1PN3PP/P2Q1PB1/4R1K1 w - - 0 23",
                "2r2b2/5p2/5k2/p1r1pP2/P2pB3/1P3P2/K1P3R1/7R w - - 23 93",
                "5k2/4q1p1/3P1pQb/1p1B4/pP5p/P1PR4/5PP1/1K6 b - - 0 38",
                "6k1/6p1/8/6KQ/1r6/q2b4/8/8 w - - 0 32",
                "5rk1/1rP3pp/p4n2/3Pp3/1P2Pq2/2Q4P/P5P1/R3R1K1 b - - 0 32",
                "4r1k1/4r1p1/8/p2R1P1K/5P1P/1QP3q1/1P6/3R4 b - - 0 1",
                "R4r2/4q1k1/2p1bb1p/2n2B1Q/1N2pP2/1r2P3/1P5P/2B2KNR w - - 3 31",
                "r6k/pbR5/1p2qn1p/P2pPr2/4n2Q/1P2RN1P/5PBK/8 w - - 2 31",
                "rn2k3/4r1b1/pp1p1n2/1P1q1p1p/3P4/P3P1RP/1BQN1PR1/1K6 w - - 6 28",
                "3q1k2/3P1rb1/p6r/1p2Rp2/1P5p/P1N2pP1/5B1P/3QRK2 w - - 1 42",
                "4r2k/1p3rbp/2p1N1p1/p3n3/P2NB1nq/1P6/4R1P1/B1Q2RK1 b - - 4 32",
                "4r1k1/1q1r3p/2bPNb2/1p1R3Q/pB3p2/n5P1/6B1/4R1K1 w - - 2 36",
                "3qr2k/1p3rbp/2p3p1/p7/P2pBNn1/1P3n2/6P1/B1Q1RR1K b - - 1 30",
                "3qk1b1/1p4r1/1n4r1/2P1b2B/p3N2p/P2Q3P/8/1R3R1K w - - 2 39",

                "6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - - 0 1",                       // SEE test suite - regular promotion
                "1n2kb1r/p1P4p/2qb4/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk - 0 1", // SEE test suite - promotion with capture
                "6Q1/8/1kp4P/2q1p3/2PpP3/2nP2P1/p7/5BK1 b - - 1 35",            // Fischer vs Petrosian - double promotion
                "5R2/2k3PK/8/5N2/7P/5q2/8/q7 w - - 0 69",                       // McShane - Aronian 2012 - knight promotion
                "rnbqk1nr/ppp2ppp/8/4P3/1BP5/8/PP2KpPP/RN1Q1BNR b kq - 1 7",    // Albin Countergambit, Lasker Trap - knight promotion

                "8/nRp5/8/8/8/7k/8/7K w - - 0 1",                               // R vs NP
                "8/bRp5/8/8/8/7k/8/7K w - - 0 1",                               // R vs BP
                "8/8/4k3/3n1n2/5P2/8/3K4/8 b - - 0 12",                         // NN vs P endgame
                "8/bQr5/8/8/8/7k/8/7K w - - 0 1",                               // Q vs RB, leading to Q vs R or Q vs B
                "8/nQr5/8/8/8/7k/8/7K w - - 0 1",                               // Q vs RN, leading to Q vs R or Q vs N
                "4kq2/8/n7/8/8/3Q3b/8/3K4 w - - 0 1",                           // Q vs QBN, leading to Q vs QB or Q vs QN
                "8/5R2/1n2RK2/8/8/7k/4r3/8 b - - 0 1",                          // RR vs RN endgame, where if black takes, they actually loses
                "8/n3p3/8/2B5/2b5/7k/P7/7K w - - 0 1",                          // BP vs BNP endgame, leading to B vs BN
                "8/n3p3/8/2B5/1n6/7k/P7/7K w - - 0 1",                          // BP vs NNP endgame, leading to B vs NN
                "8/bRn5/8/7b/8/7k/8/7K w - - 0 1",                              // R vs BBN, leading to R vs BN or R vs BB
                "1b6/1R1r4/8/1n6/7k/8/8/7K w - - 0 1",                          // R vs RBN, leading to R vs BN or R vs RB or R vs RN
                "8/q5rk/8/8/8/8/Q5RK/7N w - - 0 1",                             // Endgame that can lead to QN vs Q or RN vs R positions
                "1kr5/2bp3q/Q7/1K6/6q1/6B1/8/8 w - - 0 1",                      // Endgame where triple repetition can and needs to be forced by white
                "1kr5/2bp3q/R7/1K6/6q1/6B1/8/8 w - - 96 200",                    // Endgame where 50 moves draw can and needs to be forced by white
                "rnbqk2r/pppp1ppp/5n2/8/Bb2N3/8/PPPPQPPP/RNB1K2R w KQkq - 2 1", // Petroff defense alike position with double check Q and N
                "rnb1kb1r/pppp1ppp/5n2/8/4N3/8/PPPP1PPP/RNB1R1K1 w kq - 2 5",   // Double check R and N
                "rnbqk2r/ppp2ppp/3p4/8/1b2Bn2/8/PPPPQPPP/RNB1K2R w KQkq - 2 5", // Double check Q and B
                "rnbqk2r/ppp2ppp/3p4/8/1b2B3/3n4/PPPP1PPP/RNBQR1K1 w kq - 2 5", // Double check R and B
                "r3k2r/ppp2ppp/n7/1N1p4/Bb6/8/PPPP1PPP/RNBQ1RK1 w kq - 2 1",    // Double check B and N, castling rights
                "r3k2r/ppp2ppp/n7/1N1p4/Bb6/8/PPPP1PPP/RNBQ1RK1 w - - 2 1",     // Double check B and N, no castling rights
        };

	uint64_t start_ts = esp_timer_get_time();

	for(auto & fen: fens) {
		printf("\33[2K\r%s\r", fen.c_str());
		fflush(stdout);
		// put
		{
			std::unique_lock<std::mutex> lck(work.search_fen_lock);
			sp.at(0)->pos = libchess::Position(fen);
			init_move(sp.at(0)->nnue_eval, sp.at(0)->pos);
			memset(sp.at(0)->history, 0x00, history_malloc_size);
			work.search_think_time  = 1 << 31;
			work.search_is_abs_time = true;
			work.search_max_depth   = 10;
			work.search_max_n_nodes.reset();
			work.search_version++;
			work.search_best_move.reset();
			work.search_best_score = -32768;
			work.search_output     = false;
			work.search_cv.notify_all();
		}
		// get
		{
			std::unique_lock<std::mutex> lck(work.search_fen_lock);
			while(work.search_best_move.has_value() == false || work.search_count_running != 0)
				work.search_cv_finished.wait(lck);
		}
		// printf("%s|%s|%d\n", fen.c_str(), work.search_best_move.value().to_str().c_str(), work.search_best_score);
	}
	uint64_t end_ts   = esp_timer_get_time();

	uint64_t node_count = sp.at(0)->cs.data.nodes + sp.at(0)->cs.data.qnodes;
	uint64_t t_diff     = end_ts - start_ts;

	printf("===========================\n");
	printf("Total time (ms) : %" PRIu64 "\n", t_diff / 1000);
	printf("Nodes searched  : %" PRIu64 "\n", node_count);
	printf("Nodes/second    : %" PRIu64 "\n", node_count * 1000000 / t_diff);

	delete_threads();
}

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
void help()
{
	print_max();

	printf("-t x  thread count\n");
	printf("-p    allow pondering\n");
	printf("-s x  set path to Syzygy\n");
	printf("-H x  set size of hashtable to x MB\n");
	printf("-R x  my_trace to file\n");
	printf("-r    enable tracing to screen\n");
	printf("-U    run unit tests\n");
	printf("-Q x:y:z run test type x againt file y with search time z (ms), with x is \"matefinder\"\n");
}

int main(int argc, char *argv[])
{
#if !defined(_WIN32)
	signal(SIGPIPE, SIG_IGN);
#endif

	hello();

#if !defined(__ANDROID__)
	int thread_count =  1;
	int c            = -1;
	while((c = getopt(argc, argv, "t:ps:UR:rH:Q:h")) != -1) {
		if (c == 'U') {
			run_tests();
			return 1;
		}

		if (c == 'Q') {
			auto parts = split(optarg, ":");
			if (parts[0] == "matefinder")
				test_mate_finder(parts[1], std::stoi(parts[2]));
			else {
				printf("Test type %s not known\n", parts[0].c_str());
				return 1;
			}
			return 0;
		}

		if (c == 't')
			thread_count = atoi(optarg);
		else if (c == 'p')
			allow_ponder = true;
		else if (c == 's') {
			fathom_init(optarg);

			with_syzygy = true;
		}
		else if (c == 'R')
			my_trace_file = optarg;
                else if (c == 'r')
                        trace_enabled = true;
		else if (c == 'H')
			tti.set_size(uint64_t(atol(optarg)) * 1024 * 1024);
		else {
			help();

			return c == 'h' ? 0 : 1;
		}
	}
#endif
	// for openbench
	if (optind < argc && strcmp(argv[optind], "bench") == 0) {
		run_bench();
		return 0;
	}

#if !defined(_WIN32) && !defined(ESP32)
	se = new state_exporter(20);
#endif

	if (my_trace_file.empty() == false)
		my_trace("# tracing to file enabled\n");

	allocate_threads(thread_count);

	setvbuf(stdout, nullptr, _IONBF, 0);

	main_task();

	delete se;

	if (with_syzygy)
		fathom_deinit();

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

	// configure UART1 (2nd uart)
	uart_config_t uart_config = {
		.baud_rate = 19200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.rx_flow_ctrl_thresh = 122,
	};
	ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(uart_num, 16, 17, 32, 25));
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
			my_printf("Failed to mount or format filesystem\n");
		else if (ret == ESP_ERR_NOT_FOUND)
			my_printf("Failed to find SPIFFS partition\n");
		else
			my_printf("Failed to initialize SPIFFS (%s)\n", esp_err_to_name(ret));
		my_printf("Did you run \"pio run -t uploadfs\"?\n");
	}

	hello();

	esp_chip_info_t chip_info { };
	esp_chip_info(&chip_info);
	allocate_threads(chip_info.cores);

	allow_ponder = true;

	gpio_set_level(LED_INTERNAL, 0);

	main_task();

	start_blink(led_red_timer);
	esp_restart();
}
#endif
