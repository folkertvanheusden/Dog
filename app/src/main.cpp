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
#if defined(linux) || defined(_WIN32) || defined(__APPLE__)
#include "syzygy.h"
bool with_syzygy = false;
#endif
#include "test.h"
#include "tt.h"
#include "tui.h"
#include "tuners.h"


std::vector<search_pars_t *> sp;

std::thread *usb_disp_thread { nullptr };

#if defined(linux) || defined(__APPLE__)
uint64_t wboard { 0 };
uint64_t bboard { 0 };
#endif

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
#if defined(linux) || defined(_WIN32) || defined(__APPLE__)
	if (my_trace_file.empty() == false) {
		FILE *fh = fopen(my_trace_file.c_str(), "a+");
		if (fh) {
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
	stop->flag = true;
	stop->cv.notify_all();
}

void clear_flag(end_t *const stop)
{
	stop->flag = false;
}

auto stop_handler = []()
{
	for(auto & i: sp)
		set_flag(&i->stop);
#if !defined(__ANDROID__)
	my_trace("# stop_handler invoked\n");
#endif
};

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
	bool                    reconfigure_threads { false };
	std::condition_variable search_cv;
	std::string             search_fen;
	int                     search_fen_version { -1 };
	int                     search_think_time  { 0  };
	bool                    search_is_abs_time { false };
	int                     search_max_depth;
	std::optional<uint64_t> search_max_n_nodes;
	std::mutex              search_publish_lock;
	std::condition_variable search_cv_finished;
	libchess::Move          search_best_move   { 0  };
	int                     search_best_score  { 0  };
	bool                    search_output      { false };
	int                     search_count_running { 0 };
} work;

void searcher(const int i)
{
	printf("Thread %d started\n", i);

	int last_fen_version = -1;

	for(;;) {
		std::unique_lock<std::mutex> lck(work.search_fen_lock);
		while(work.search_fen_version == last_fen_version && work.reconfigure_threads == false)
			work.search_cv.wait(lck);

		if (work.reconfigure_threads)
			break;

		last_fen_version = work.search_fen_version;

		int  local_search_think_time  = work.search_think_time;
		bool local_search_is_abs_time = work.search_is_abs_time;
		int  local_search_max_depth   = work.search_max_depth;
		auto local_search_max_n_nodes = work.search_max_n_nodes;
		bool local_search_output      = work.search_output;

		work.search_count_running++;

		clear_flag(&sp.at(i)->stop);
		lck.unlock();

		// search!
		libchess::Move best_move  { 0 };
		int            best_score { 0 };
		std::tie(best_move, best_score) = search_it(sp.at(i)->pos, local_search_think_time, local_search_is_abs_time, sp.at(i), local_search_max_depth, i, local_search_max_n_nodes, i == 0 && local_search_output);

		// notify finished
		lck.lock();

		if (i == 0) {
			work.search_best_move  = best_move;
			work.search_best_score = best_score;
		}

		work.search_count_running--;
		work.search_cv_finished.notify_one();
	}

	printf("Thread %d finished\n", i);
}

void start_ponder()
{
	my_trace("# start ponder\n");
	std::unique_lock<std::mutex> lck(work.search_fen_lock);
	assert(work.search_count_running == 0);

	clear_flag(&sp.at(0)->stop);
	for(size_t i=1; i<sp.size(); i++) {
		sp.at(i)->pos = sp.at(0)->pos;
		clear_flag(&sp.at(i)->stop);
	}

	work.search_think_time  = -1;
	work.search_is_abs_time = false;
	work.search_max_depth   = -1;
	work.search_max_n_nodes.reset();
	work.search_fen_version++;
	work.search_best_move  = libchess::Move(0);
	work.search_best_score = -32768;
	work.search_output     = false;
	work.search_cv.notify_all();
	my_trace("# ponder started\n");
}

void stop_ponder()
{
	my_trace("# stop ponder\n");

	{
		std::unique_lock<std::mutex> lck(work.search_fen_lock);

		for(auto & i: sp)
			set_flag(&i->stop);

		work.search_cv.notify_all();
	}

	{
		std::unique_lock<std::mutex> lck(work.search_publish_lock);

		while(work.search_count_running != 0)
			work.search_cv_finished.wait(lck);
	}

	my_trace("# ponder stopped\n");
}

void delete_threads()
{
	work.reconfigure_threads = true;

	for(auto & i: sp)
		set_flag(&i->stop);

	work.search_cv.notify_all();

	for(auto & i: sp) {
		i->thread_handle->join();
		delete i->thread_handle;

		free(i->history);
		delete i;
	}

	sp.clear();

	work.reconfigure_threads = false;
}

void allocate_threads(const int n)
{
	delete_threads();

	for(int i=0; i<n; i++) {
		sp.push_back(new search_pars_t({ default_parameters, reinterpret_cast<int16_t *>(calloc(1, history_malloc_size)) }));
		sp.at(i)->thread_handle = new std::thread(searcher, i);
	}
#if defined(ESP32)
	if (n > 0) {
		think_timeout_pars.arg = &sp.at(0)->stop;
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
	if (allow_ponder != value) {
		if (value)
			start_ponder();
		else
			stop_ponder();
	}
	allow_ponder = value;
	printf("# Ponder %s\n", value ? "enabled" : "disabled");
};

auto commerial_option_handler = [](const std::string & value) { };

#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__) && !defined(__APPLE__)
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
int check_min_stack_size(const int nr, search_pars_t & sp)
{
	UBaseType_t level = uxTaskGetStackHighWaterMark(nullptr);

	my_trace("# dts: %lld depth %d nodes %u lower_bound: %d, task name: %s\n", esp_timer_get_time() - esp_start_ts, sp.md, sp.cs.data.nodes, level, pcTaskGetName(xTaskGetCurrentTaskHandle()));

	if (level < 768) {
		set_flag(&sp.stop);
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

void main_task()
{
	libchess::UCIService *uci_service = new libchess::UCIService("Dog v3.0", "Folkert van Heusden", std::cout, is);

	std::ios_base::sync_with_stdio(true);
	std::cout.setf(std::ios::unitbuf);

	init_lmr();

	chess_stats global_cs;

	auto eval_handler = [](std::istringstream&) {
		int score = eval(sp.at(0)->pos, sp.at(0)->parameters);
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

		perft(sp.at(0)->pos, std::stoi(temp));
	};

	auto ucinewgame_handler = [&global_cs](std::istringstream&) {
		stop_ponder();
		for(auto & i: sp)
			memset(i->history, 0x00, history_malloc_size);
		global_cs.reset();
		tti.reset();
		printf("# --- New game ---\n");
		if (allow_ponder)
			start_ponder();
	};

	auto position_handler = [](const libchess::UCIPositionParameters & position_parameters) {
		stop_ponder();
		sp.at(0)->pos = libchess::Position { position_parameters.fen() };
		if (!position_parameters.move_list())
			return;

		for (auto & move_str : position_parameters.move_list()->move_list())
			sp.at(0)->pos.make_move(*libchess::Move::from(move_str));
	};

	auto go_handler = [&global_cs](const libchess::UCIGoParameters & go_parameters) {
#if defined(ESP32)
		uint64_t start_ts = esp_timer_get_time();
#endif

		try {
			for(auto & i: sp)
				clear_flag(&i->stop);
			for(auto & i: sp)
				i->md = 1;
			reset_search_statistics();

#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__) && !defined(__APPLE__)
			esp_start_ts = start_ts;
			stop_blink(led_red_timer, &led_red);
			start_blink(led_green_timer);
#endif

			int moves_to_go = 40 - sp.at(0)->pos.fullmoves();

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

				my_trace("# My time: %d ms, inc: %d ms, opponent time: %d ms, inc: %d ms, full: %d, half: %d, phase: %d, moves_to_go: %d, tt: %d\n", ms, time_inc, ms_opponent, time_inc_opp, sp.at(0)->pos.fullmoves(), sp.at(0)->pos.halfmoves(), game_phase(sp.at(0)->pos, default_parameters), moves_to_go, tti.get_per_mille_filled());
			}

			libchess::Move best_move  { 0 };
			int            best_score { 0 };
			bool           has_best   { false };

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
				stop_ponder();

				// put
				{
					std::unique_lock<std::mutex> lck(work.search_fen_lock);

					for(size_t i=1; i<sp.size(); i++)
						sp.at(i)->pos = sp.at(0)->pos;

					work.search_think_time  = depth.has_value() && think_time == 0 ? -1 : think_time;
					work.search_is_abs_time = is_absolute_time;
					work.search_max_depth   = depth.has_value() ? depth.value() : -1;
					work.search_max_n_nodes.reset();
					work.search_fen_version++;
					work.search_best_move  = libchess::Move(0);
					work.search_best_score = -32768;
					work.search_output     = true;
					work.search_cv.notify_all();
				}

				// get
				{
					std::unique_lock<std::mutex> lck(work.search_publish_lock);

					while(work.search_best_move.value() == 0 || work.search_count_running != 0)
						work.search_cv_finished.wait(lck);

					best_move  = work.search_best_move;
					best_score = work.search_best_score;
				}
			}

			// emit result
			libchess::UCIService::bestmove(best_move.to_str());

			// no longer thinking
#if !defined(linux) && !defined(_WIN32) && !defined(__APPLE__)
			stop_blink(led_green_timer, &led_green);
#endif
#if defined(__ANDROID__)
			__android_log_print(ANDROID_LOG_INFO, APPNAME, "Performed move %s for position %s", best_move.to_str().c_str(), sp.at(0)->pos.fen().c_str());
#endif

			if (sp.at(0)->pos.game_state() != libchess::Position::GameState::IN_PROGRESS)
				emit_statistics(global_cs, "global statistics");

			if (allow_ponder)
				start_ponder();

			global_cs.add(calculate_search_statistics());
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

	uci_service->register_position_handler(position_handler);
	uci_service->register_go_handler      (go_handler);
	uci_service->register_stop_handler    (stop_handler);

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
	my_printf("# Version " DOG_VERSION ", compiled on " __DATE__ " " __TIME__ "\n\n");
	my_printf("# Dog is a chess program written by Folkert van Heusden <mail@vanheusden.com>.\n");
#endif
}

void run_bench()
{
#if 0  // TODO
	init_lmr();

	memset(sp.at(0)->history, 0x00, history_malloc_size);

	libchess::Move best_move  { 0 };
	int            best_score { 0 };

	uint64_t start_ts = esp_timer_get_time();
	std::tie(best_move, best_score) = search_it(positiont1, 1<<31, true, sp.at(0), 10, 0, { }, true);
	uint64_t end_ts   = esp_timer_get_time();

	uint64_t node_count = sp.at(0)->cs.data.nodes + sp.at(0)->cs.data.qnodes;
	uint64_t t_diff     = end_ts - start_ts;

	printf("===========================\n");
	printf("Total time (ms) : %" PRIu64 "\n", t_diff / 1000);
	printf("Nodes searched  : %" PRIu64 "\n", node_count);
	printf("Nodes/second    : %" PRIu64 "\n", node_count * 1000000 / t_diff);
#endif
}

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
void help()
{
	print_max();

	printf("-t x  thread count\n");
	printf("-p    allow pondering\n");
	printf("-s x  set path to Syzygy\n");
	printf("-H x  set size of hashtable to x MB\n");
	printf("-u x  USB display device\n");
	printf("-T x  tune using epd file\n");
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
	// for openbench
	if (argc == 2 && strcmp(argv[1], "bench") == 0) {
		run_bench();
		return 0;
	}

	int thread_count =  1;
	int c            = -1;
	while((c = getopt(argc, argv, "t:pT:s:u:UR:rH:Q:h")) != -1) {
		if (c == 'T') {
			tune(optarg);
			return 0;
		}

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
#if !defined(_WIN32) && !defined(__APPLE__)
		else if (c == 'u')
			usb_disp_thread = new std::thread(usb_disp, optarg);
#endif
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

	if (my_trace_file.empty() == false)
		my_trace("# tracing to file enabled\n");

	allocate_threads(thread_count);

	setvbuf(stdout, nullptr, _IONBF, 0);

	static_assert(sizeof(int) >= 4, "INT should be at least 32 bit");

	main_task();

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

	esp_chip_info_t chip_info { };
	esp_chip_info(&chip_info);
	allocate_threads(chip_info.cores);

	main_task();

	start_blink(led_red_timer);
	esp_restart();
}
#endif
