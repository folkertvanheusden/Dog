#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>

#include "eval_par.h"


typedef struct {
	std::atomic_bool        flag;
	std::condition_variable cv;
} end_t;

typedef struct {
	uint32_t  nodes;
	uint32_t  qnodes;

	uint32_t  tt_query;
	uint32_t  tt_hit;
	uint32_t  tt_store;
	uint32_t  tt_invalid;

	uint32_t  n_null_move;
	uint32_t  n_null_move_hit;

	uint64_t  n_moves_cutoff;
	uint64_t  nmc_nodes;
	uint64_t  n_qmoves_cutoff;
	uint64_t  nmc_qnodes;

	uint64_t  syzygy_queries;
	uint64_t  syzygy_query_hits;
} chess_stats_t;

typedef struct
{
	const eval_par *parameters;
	bool      is_t2;

	int16_t *const history;

	chess_stats_t *cs;

	uint16_t  md;

	end_t    *stop;
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
	char      move[5];
	int       score;
#endif
} search_pars_t;

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
uint64_t esp_timer_get_time();
#endif

constexpr size_t history_size        = 2 * 6 * 64;
constexpr size_t history_malloc_size = sizeof(int16_t) * history_size;

#include "inbuf.h"
#include "tt.h"

extern bool               trace_enabled;
extern inbuf              i;
extern std::istream       is;
extern search_pars_t      sp1;
extern libchess::Position positiont1;
extern tt                 tti;
extern uint64_t           bboard;
extern uint64_t           wboard;
extern std::vector<search_pars_t> sp2;
extern bool               with_syzygy;

#if defined(ESP32)
#include <driver/gpio.h>
#include <esp_timer.h>

typedef struct {
	gpio_num_t pin_nr;
	bool       state;
} led_t;

extern led_t led_green;
extern led_t led_blue;
extern led_t led_red;

extern esp_timer_handle_t led_green_timer;
extern esp_timer_handle_t led_blue_timer;
extern esp_timer_handle_t led_red_timer;

void start_blink(esp_timer_handle_t handle);
void stop_blink(esp_timer_handle_t handle, led_t *l);
#endif

class sort_movelist_compare
{
private:
        libchess::Position       *const p  { nullptr };
	const eval_par           *const ep { nullptr };
	const search_pars_t      *const sp { nullptr };
        std::vector<libchess::Move>     first_moves;
        std::optional<libchess::Square> previous_move_target;

public:
        sort_movelist_compare(libchess::Position *const p, const search_pars_t *const sp);
        void add_first_move(const libchess::Move move);
        int move_evaluater(const libchess::Move move) const;
};

void trace(const char *const fmt, ...);
void set_flag(end_t *const stop);
void clear_flag(end_t *const stop);
std::pair<libchess::Move, int> search_it(libchess::Position *const pos, const int search_time, const bool is_absolute_time, search_pars_t *const sp, const int ultimate_max_depth, const int thread_nr, std::optional<uint64_t> max_n_nodes);
void set_new_ponder_position(const bool is_ponder);
void start_ponder();
void pause_ponder();
int qs(libchess::Position & pos, int alpha, int beta, int qsdepth, search_pars_t *const sp, const int thread_nr);
void set_thread_name(std::string name);
void sort_movelist(libchess::MoveList & move_list, sort_movelist_compare & smc);
void reset_search_statistics();
chess_stats_t calculate_search_statistics();
void sum_stats(const chess_stats_t *const source, chess_stats_t *const target);
