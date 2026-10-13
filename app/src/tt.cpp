#include <bit>
#include <cassert>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#if !defined(NDEBUG)
#if !defined(_WIN32) && !defined(ESP32) && !defined(__ANDROID__) && !defined(__APPLE__)
#include <valgrind/helgrind.h>
#endif
#endif
#if defined(linux)
#include <sys/mman.h>
#endif

#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

#include "libchess/Position.h"
#include "main.h"
#include "tt.h"


static_assert(sizeof(tt_entry) == 8, "tt_entry must be 8 bytes in size");

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

void tt::debug_helper()
{
#if !defined(NDEBUG)
#if !defined(_WIN32) && !defined(ESP32) && !defined(__ANDROID__) && !defined(__APPLE__)
	VALGRIND_HG_DISABLE_CHECKING(entries, n_entries * sizeof(tt_internal_bins));
#endif
#endif
}

void tt::allocate()
{
#if defined(ESP32)
	size_t psram_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
	if (psram_size > ESP32_TT_RAM_SIZE) {
		constexpr const size_t max_sp_size = 4 * 1024l * 1024l;
		psram_size = std::min(psram_size, max_sp_size);
		printf("Using %zu bytes of PSRAM\n", psram_size);
		n_entries = psram_size / sizeof(tt_internal_bins);
		entries = reinterpret_cast<tt_internal_bins *>(heap_caps_malloc(n_entries * sizeof(tt_internal_bins), MALLOC_CAP_SPIRAM));
	}
	else {
		printf("No PSRAM\n");
		for(;;) {
			auto n_bytes = n_entries * sizeof(tt_internal_bins);
			printf("Using %zu bytes of RAM\n", size_t(n_bytes));
			entries = reinterpret_cast<tt_internal_bins *>(malloc(n_bytes));
			if (entries)
				break;
			n_entries = std::max(uint64_t(0), n_entries - 1024 / sizeof(tt_internal_bins));
			if (n_entries == 0)
				break;
		}
	}
#else
	size_t s = n_entries * sizeof(tt_internal_bins);
#if defined(linux)
	if (posix_memalign(reinterpret_cast<void **>(&entries), 1024 * 1024 * 2, s)) {
		printf("# posix_memalign failed: %s\n", strerror(errno));
		entries = reinterpret_cast<tt_internal_bins *>(malloc(s));
	}
	else {
		if (madvise(entries, s, MADV_HUGEPAGE) == -1)
			printf("# madvise failed: %s\n", strerror(errno));
	}
#else
	entries = reinterpret_cast<tt_internal_bins *>(malloc(s));
#endif
#endif
}

void tt::set_size(const uint64_t s)
{
	n_entries = std::max(uint64_t(2), s / sizeof(tt_internal_bins));
	free(entries);
	allocate();
	reset();
	printf("# Newly allocated node count: %" PRIu64 "\n", n_entries);
}

int tt::get_size() const
{
	return n_entries * sizeof(tt_internal_bins);
}

uint64_t tt::get_n() const
{
	return n_entries;
}

void tt::reset()
{
	memset(entries, 0x00, sizeof(tt_internal_bins) * n_entries);
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

std::optional<tt_entry> IRAM_ATTR tt::lookup(const uint64_t hash)
{
	uint64_t                index = fastrange(hash, n_entries);
	tt_internal_bins *const e     = &entries[index];

	for(int i=0; i<N_ITEMS_PER_BIN; i++) {
		tt_entry cur = std::bit_cast<tt_entry>(e->item[i]);
		if (cur.hash == uint16_t(hash)) {
			cur.age = age;
			e->item[i] = std::bit_cast<tt_internal_entry>(cur);
			return cur;
		}
	}

	return { };
}

void tt::store(const uint64_t hash, const tt_entry_flag f, const int d, const int score, const std::optional<libchess::Move> & m)
{
	const uint64_t          index { fastrange(hash, n_entries) };
	tt_internal_bins *const e     { &entries[index]            };
	const uint16_t          compare_hash { hash    };
	int                     use_item     { 0       };
	int                     best_depth   { INT_MAX };

	assert(m.has_value() == false || m.value() < 0x40000);

	for(int i=0; i<N_ITEMS_PER_BIN; i++) {
		tt_entry cur = std::bit_cast<tt_entry>(e->item[i]);

		if (compare_hash == cur.hash) {
                        if (cur.depth > d || (f != EXACT && cur.depth == d)) {
                                cur.age = age;
				e->item[i] = std::bit_cast<tt_internal_entry>(cur);
                                return;
                        }

			use_item = i;
			break;
		}

		if (cur.age != age || cur.depth < best_depth) {
			best_depth = cur.depth;
			use_item   = i;
		}	
	 }

	uint32_t new_M { };
	if (m.has_value())
		new_M = m->value();
	else {
		auto tt_item = std::bit_cast<tt_entry>(e->item[use_item]);
	       	if (tt_item.hash == compare_hash)
			new_M = tt_item.M;
	}

	tt_entry cur = {
		.hash  { compare_hash   },
		.score { int16_t(score) },
		.depth { uint8_t(d)     },
		.M     { new_M          },
		.flags { f              },
		.age   { age            },
	};
	e->item[use_item] = std::bit_cast<tt_internal_entry>(cur);
}

int tt::get_per_mille_filled() const
{
	int count = 0;
	for(int i=0; i<1000; i++) {
		tt_entry cur = std::bit_cast<tt_entry>(entries[i].item[0]);
		count += !!cur.hash;
	}
	return count;
}

int eval_to_tt(const int eval, const int ply)
{
	if (eval > max_non_mate)
		return eval + ply;
	if (eval < -max_non_mate)
		return eval - ply;
	return eval;
}

int eval_from_tt(const int eval, const int ply)
{
	if (eval > max_non_mate)
		return eval - ply;
	if (eval < -max_non_mate)
		return eval + ply;
	return eval;
}
