#pragma once

#include <cstdint>
#include <optional>

#include <libchess/Position.h>


#define __PRAGMA_PACKED__ __attribute__ ((__packed__))

typedef enum { NOTVALID = 0, EXACT = 1, LOWERBOUND = 2, UPPERBOUND = 3 } tt_entry_flag;

#define TT_ENTRY_N_ENTRIES 4

typedef struct {
	struct __PRAGMA_PACKED__ tt_entry {
		int16_t  score;
		uint32_t hash   : 20;
		uint8_t  depth  : 8;
		uint8_t  flags  : 2;
		uint32_t M      : 18;
	} entries[TT_ENTRY_N_ENTRIES];
} tt_entries;

class tt
{
private:
	tt_entries *entries { nullptr };
#if defined(ESP32)
#define ESP32_TT_RAM_SIZE 98304
	uint64_t n_entries { ESP32_TT_RAM_SIZE / sizeof(tt_entries) };
#elif defined(__ANDROID__)
	uint64_t n_entries { 16 * 1024 * 1024  / sizeof(tt_entries) };
#elif defined(linux) || defined(_WIN32) || defined(__APPLE__)
	uint64_t n_entries { 256 * 1024 * 1024  / sizeof(tt_entries) };
#endif
	void allocate();

public:
	tt();
	~tt();

	void     debug_helper();
	void     reset();
	void     set_size(const uint64_t s);
	int      get_size() const;  // in MB
	uint64_t get_n   () const;
	int      get_per_mille_filled() const;

	std::optional<tt_entries::tt_entry> lookup(const uint64_t board_hash);
	void store(const uint64_t hash, const tt_entry_flag f, const int d, const int score, const libchess::Move & m);
	void store(const uint64_t hash, const tt_entry_flag f, const int d, const int score);
};

int eval_to_tt  (const int eval, const int ply);
int eval_from_tt(const int eval, const int ply);
uint32_t       libchessmove_to_uint(const libchess::Move & m);
libchess::Move uint_to_libchessmove(const uint32_t v);

extern tt tti;
