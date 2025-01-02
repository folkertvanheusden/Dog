#include <assert.h>
#include <cinttypes>
#include <cstdlib>
#include <cstring>

#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

#include "libchess/Position.h"
#include "tt.h"


tt tti;

tt::tt()
{
	allocate();
	reset();
}

tt::~tt()
{
	free(entries);
}

void tt::allocate()
{
#if defined(ESP32)
	size_t psram_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
	if (psram_size > ESP32_TT_RAM_SIZE) {
		printf("Using %zu bytes of PSRAM\n", psram_size);
		n_entries = psram_size / sizeof(tt_entry);
		entries = reinterpret_cast<tt_entry *>(heap_caps_malloc(n_entries * sizeof(tt_entry), MALLOC_CAP_SPIRAM));
	}
	else {
		printf("No PSRAM\n");
		entries = reinterpret_cast<tt_entry *>(malloc(n_entries * sizeof(tt_entry)));
	}
#else
	entries = reinterpret_cast<tt_entry *>(malloc(n_entries * sizeof(tt_entry)));
#endif
}

void tt::set_size(const uint64_t s)
{
	n_entries = s / sizeof(tt_entry);
	free(entries);
	allocate();
	reset();
	printf("# Newly allocated node count: %" PRIu64 "\n", n_entries);
}

void tt::reset()
{
	memset(entries, 0x00, sizeof(tt_entry) * n_entries);
}

// see https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
#if defined(ESP32)
static inline uint32_t fastrange32(uint32_t word, uint32_t p)
{
	return (uint64_t(word) * uint64_t(p)) >> 32;
}
#define fastrange fastrange32
#else
typedef unsigned __int128 uint128_t;
inline uint64_t fastrange64(uint64_t word, uint64_t p)
{
	return (uint128_t(word) * uint128_t(p)) >> 64;
}
#define fastrange fastrange64
#endif

std::optional<tt_entry> tt::lookup(const uint64_t hash)
{
	uint64_t   index = fastrange(hash, n_entries);
	tt_entry & cur   = entries[index];

	if ((cur.hash ^ cur.data_.data) == hash)
		return cur;

	return { };
}

void tt::store(const uint64_t hash, const tt_entry_flag f, const int d, const int score, const libchess::Move & m)
{
	uint64_t        index = fastrange(hash, n_entries);
	tt_entry *const e     = &entries[index];

	tt_entry::u n { };
	n._data.score = int16_t(score);
	n._data.depth = uint8_t(d);
	n._data.flags = f;
	n._data.m     = m.value();

	e->hash       = hash ^ n.data;
	e->data_.data = n.data;
}

int tt::get_per_mille_filled()
{
	int count = 0;
	for(int i=0; i<1000; i++)
		count += entries[i].hash != 0;

	return count;
}

std::vector<libchess::Move> get_pv_from_tt(const libchess::Position & pos_in, const libchess::Move & start_move)
{
	auto work = pos_in;

	std::vector<libchess::Move> out { start_move };
	work.make_move(start_move);

	for(int i=0; i<64; i++) {
		std::optional<tt_entry> te = tti.lookup(work.hash());
		if (!te.has_value())
			break;

		libchess::Move cur_move = libchess::Move(te.value().data_._data.m);
		if (!work.is_legal_move(cur_move))
			break;

		out.push_back(cur_move);
		work.make_move(cur_move);
		if (work.is_repeat(3))
			break;
	}

	return out;
}

int eval_to_tt(const int eval, const int ply)
{
	if (eval > 9800)
		return eval + ply;
	if (eval < -9800)
		return eval - ply;
	return eval;
}

int eval_from_tt(const int eval, const int ply)
{
	if (eval > 9800)
		return eval - ply;
	if (eval < -9800)
		return eval + ply;
	return eval;
}
