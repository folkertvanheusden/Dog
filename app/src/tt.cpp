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
	VALGRIND_HG_DISABLE_CHECKING(entries, n_entries * sizeof(tt_entry));
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
		n_entries = psram_size / sizeof(tt_entry);
		entries = reinterpret_cast<tt_entry *>(heap_caps_malloc(n_entries * sizeof(tt_entry), MALLOC_CAP_SPIRAM));
	}
	else {
		printf("No PSRAM\n");
		for(;;) {
			auto n_bytes = n_entries * sizeof(tt_entry);
			printf("Using %zu bytes of RAM\n", size_t(n_bytes));
			entries = reinterpret_cast<tt_entry *>(malloc(n_bytes));
			if (entries)
				break;
			n_entries = std::max(uint64_t(0), n_entries - 1024 / sizeof(tt_entry));
			if (n_entries == 0)
				break;
		}
	}
#else
	size_t s = n_entries * sizeof(tt_entry);
#if defined(linux)
	if (posix_memalign(reinterpret_cast<void **>(&entries), 1024 * 1024 * 2, s)) {
		printf("# posix_memalign failed: %s\n", strerror(errno));
		entries = reinterpret_cast<tt_entry *>(malloc(s));
	}
	else {
		if (madvise(entries, s, MADV_HUGEPAGE) == -1)
			printf("# madvise failed: %s\n", strerror(errno));
	}
#else
	entries = reinterpret_cast<tt_entry *>(malloc(s));
#endif
#endif
}

void tt::set_size(const uint64_t s)
{
	n_entries = std::max(uint64_t(2), s / sizeof(tt_entry));
	free(entries);
	allocate();
	reset();
	printf("# Newly allocated node count: %" PRIu64 "\n", n_entries);
}

int tt::get_size() const
{
	return n_entries * sizeof(tt_entry);
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

std::optional<tt_entry> IRAM_ATTR tt::lookup(const uint64_t hash)
{
	uint64_t   index = fastrange(hash, n_entries);
	tt_entry & cur   = entries[index];

	if (cur.hash == uint16_t(hash))
		return cur;

	return { };
}

uint32_t libchessmove_to_uint(const libchess::Move & m)
{
	uint32_t v = m.from_square().file() | (m.from_square().rank() << 3) |
		(m.to_square().file() << 6) | (m.to_square().rank() << 9) |
		(int(m.type()) << 15);

	if (m.promotion_piece_type().has_value())
		v |= m.promotion_piece_type().value() << 12;

	return v;
}

libchess::Move uint_to_libchessmove(const uint32_t v)
{
	auto promo = libchess::PieceType((v >> 12) & 7);
	auto from  = libchess::Square::from(libchess::File(v & 7), libchess::Rank((v >> 3) & 7)).value();
	auto to    = libchess::Square::from(libchess::File((v >> 6) & 7), libchess::Rank((v >> 9) & 7)).value();
	auto type  = libchess::Move::Type(v >> 15);

	if (promo)
		return libchess::Move{ from, to, promo, type };

	return libchess::Move{ from, to, type };
}

void tt::store(const uint64_t hash, const tt_entry_flag f, const int d, const int score, const libchess::Move & m)
{
	tt_entry n { };
	n.score = int16_t(score);
	n.depth = uint8_t(d);
	n.flags = f;
	n.M     = libchessmove_to_uint(m);
	n.hash  = uint16_t(hash);

	uint64_t index = fastrange(hash, n_entries);
	entries[index] = n;
}

void tt::store(const uint64_t hash, const tt_entry_flag f, const int d, const int score)
{
	uint64_t        index = fastrange(hash, n_entries);
	tt_entry *const e     = &entries[index];

	tt_entry n { };

	if (e->hash == uint16_t(hash)) {
		tt_entry & cur = entries[index];
		n.M = cur.M;
	}

	n.score = int16_t(score);
	n.depth = uint8_t(d);
	n.flags = f;
	n.hash  = uint16_t(hash);

	entries[index] = n;
}

int tt::get_per_mille_filled()
{
	int count = 0;
	for(int i=0; i<1000; i++)
		count += entries[i].hash != 0;
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
