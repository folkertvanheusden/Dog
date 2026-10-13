#pragma once

#include <cstdint>
#include <optional>

#include <libchess/Position.h>


#define __PRAGMA_PACKED__ __attribute__ ((__packed__))

typedef enum { NOTVALID = 0, EXACT = 1, LOWERBOUND = 2, UPPERBOUND = 3 } tt_entry_flag;

typedef struct __PRAGMA_PACKED__
{
	uint16_t hash;
	int16_t  score;
	uint8_t  depth  : 8;
	uint32_t M      : 18;
	uint8_t  flags  : 2;
	uint8_t  age    : 4;
} tt_entry;

typedef uint64_t tt_internal_entry;

#define N_ITEMS_PER_BIN 4
typedef struct __PRAGMA_PACKED__
{
	tt_internal_entry item[N_ITEMS_PER_BIN];
} tt_internal_bins;

class tt final
{
private:
	tt_internal_bins *entries { nullptr };
#if defined(ESP32)
#define ESP32_TT_RAM_SIZE 98304
	uint64_t n_entries { ESP32_TT_RAM_SIZE / sizeof(tt_internal_bins) };
#elif defined(__ANDROID__)
	uint64_t n_entries { 16 * 1024 * 1024  / sizeof(tt_internal_bins) };
#elif defined(linux) || defined(_WIN32) || defined(__APPLE__)
	uint64_t n_entries { 256 * 1024 * 1024  / sizeof(tt_internal_bins) };
#endif
	void allocate();

	int  age { };

public:
	tt();
	~tt();

	void     debug_helper();
	void     reset();
	void     set_size(const uint64_t s);
	int      get_size() const;  // in MB
	uint64_t get_n   () const;
	int      get_per_mille_filled() const;

	void     inc_age() { age++; }

	std::optional<tt_entry> lookup(const uint64_t board_hash);
	void store(const uint64_t hash, const tt_entry_flag f, const int d, const int score, const std::optional<libchess::Move> & m);
};

int eval_to_tt  (const int eval, const int ply);
int eval_from_tt(const int eval, const int ply);

extern tt tti;
