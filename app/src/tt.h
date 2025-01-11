#pragma once

#include <cstdint>
#include <optional>

#include <libchess/Position.h>


#define __PRAGMA_PACKED__ __attribute__ ((__packed__))

typedef enum { NOTVALID = 0, EXACT = 1, LOWERBOUND = 2, UPPERBOUND = 3 } tt_entry_flag;

typedef struct __PRAGMA_PACKED__
{
        uint64_t hash;

        union u {
                struct {
                        int16_t score;
                        uint8_t flags  : 2;
                        uint8_t filler : 6;
                        uint8_t depth  : 8;
                        uint32_t m;
                } _data;

                uint64_t data;
        } data_;
} tt_entry;

class tt
{
private:
	tt_entry *entries { nullptr };
#if defined(ESP32)
#define ESP32_TT_RAM_SIZE 49152
	uint64_t n_entries { ESP32_TT_RAM_SIZE / sizeof(tt_entry) };
#elif defined(__ANDROID__)
	uint64_t n_entries { 16 * 1024 * 1024  / sizeof(tt_entry) };
#elif defined(linux) || defined(_WIN32) || defined(__APPLE__)
	uint64_t n_entries { 16 * 1024 * 1024  / sizeof(tt_entry) };  // as requested, because of OpenBench testing
#endif
	void allocate();

public:
	tt();
	~tt();

	void reset();
	void set_size(const uint64_t s);
	int  get_size() const;  // in MB
	int  get_per_mille_filled();

	std::optional<tt_entry> lookup(const uint64_t board_hash);
	void store(const uint64_t hash, const tt_entry_flag f, const int d, const int score, const libchess::Move & m);
	void store(const uint64_t hash, const tt_entry_flag f, const int d, const int score);
};

std::vector<libchess::Move> get_pv_from_tt(const libchess::Position & pos_in, const libchess::Move & start_move);
int eval_to_tt  (const int eval, const int ply);
int eval_from_tt(const int eval, const int ply);

extern tt tti;
