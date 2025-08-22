#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <thread>
#include <libchess/Position.h>

#include "nnue.h"
#include "stats.h"


constexpr const int max_eval = 30000;
constexpr const int max_non_mate = 29500;

typedef struct {
	std::atomic_bool        flag;
	std::condition_variable cv;
	std::mutex              cv_lock;
} end_t;

typedef struct
{
	int16_t   *const history  { nullptr };
	end_t           *stop     { nullptr };
	const int        thread_nr;
	chess_stats      cs;
	uint32_t         cur_move { 0       };
#if defined(ESP32)
	uint16_t         md;
#endif

	libchess::Position pos { libchess::constants::STARTPOS_FEN };
	libchess::Move   best_moves[128];

	std::thread     *thread_handle { nullptr };
	Eval            *nnue_eval     { nullptr };
} search_pars_t;

extern std::vector<search_pars_t *> sp;

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
uint64_t esp_timer_get_time();
#endif

constexpr size_t history_size        = 2 * 6 * 64;
constexpr size_t history_malloc_size = sizeof(int16_t) * history_size;

#include "inbuf.h"
#include "tt.h"

extern bool               trace_enabled;
extern inbuf              i;
extern std::istream       is;
extern tt                 tti;
extern bool               with_syzygy;

#if defined(ESP32)
#include <driver/gpio.h>
#include <esp_timer.h>

typedef struct {
	gpio_num_t pin_nr;
	bool       state;
	int        screen_x;
} led_t;

extern led_t led_green;
extern led_t led_blue;
extern led_t led_red;

extern esp_timer_handle_t led_green_timer;
extern esp_timer_handle_t led_blue_timer;
extern esp_timer_handle_t led_red_timer;

extern esp_timer_handle_t think_timeout_timer;

void start_blink(esp_timer_handle_t handle);
void stop_blink(esp_timer_handle_t handle, led_t *l);

int check_min_stack_size(const int nr, const search_pars_t & sp);
void vTaskGetRunTimeStats();
#endif

void my_trace(const char *const fmt, ...);
void set_flag(end_t *const stop);
void clear_flag(end_t *const stop);
void start_ponder();
void stop_ponder();
void set_thread_name(std::string name);
chess_stats calculate_search_statistics();
std::pair<uint64_t, uint64_t> simple_search_statistics();  // nodes, syzyg hits
void allocate_threads(const int n);
void delete_threads();
void run_bench(const bool long_bench);
