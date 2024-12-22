#include <assert.h>
#include <cstdlib>
#include <cstring>

#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

#include "libchess/Position.h"
#include "tt.h"

tt::tt()
{
#if defined(ESP32)
	size_t psram_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
	if (psram_size > ESP32_TT_RAM_SIZE) {
		printf("Using %zu bytes of PSRAM\n", psram_size);
		n_entries = psram_size / sizeof(tt_hash_group);
		entries = reinterpret_cast<tt_hash_group *>(heap_caps_malloc(n_entries * sizeof(tt_hash_group), MALLOC_CAP_SPIRAM));
	}
	else {
		printf("Not PSRAM\n");
		entries = reinterpret_cast<tt_hash_group *>(malloc(n_entries * sizeof(tt_hash_group)));
	}
#else
	entries = reinterpret_cast<tt_hash_group *>(malloc(n_entries * sizeof(tt_hash_group)));
#endif
	reset();
}

tt::~tt()
{
	free(entries);
}

void tt::reset()
{
	memset(entries, 0x00, sizeof(tt_hash_group) * n_entries);
}

void tt::inc_age()
{
	age++;
}

std::optional<tt_entry> tt::lookup(const uint64_t hash)
{
	uint64_t        index = hash % n_entries;
	tt_entry *const e     = entries[index].entries;
	for(int i=0; i<N_TE_PER_HASH_GROUP; i++) {
		tt_entry & cur = e[i];

		if ((cur.hash ^ cur.data_.data) == hash) {
			cur.data_._data.age = age;
			cur.hash = hash ^ cur.data_.data;
			return cur;
		}
	}

	return { };
}

void tt::store(const uint64_t hash, const tt_entry_flag f, const int d, const int score, const libchess::Move & m)
{
	unsigned long int index = hash % n_entries; // FIXME is biased at the bottom of the list

	int use_sub_index       =  -1;
	int min_depth           = 999;
	int min_depth_index     =  -1;

	tt_entry   *const e     = entries[index].entries;
	for(int i=0; i<N_TE_PER_HASH_GROUP; i++) {
		if ((e[i].hash ^ e[i].data_.data) == hash) {
			if (e[i].data_._data.depth > d) {
				e[i].data_._data.age = age;
				e[i].hash = hash ^ e[i].data_.data;
				return;
			}
			if (f != EXACT && e[i].data_._data.depth == d) {
				e[i].data_._data.age = age;
				e[i].hash = hash ^ e[i].data_.data;
				return;
			}

			use_sub_index = i;

			break;
		}

		if (e[i].data_._data.age != age)
			use_sub_index = i;
		else if (e[i].data_._data.depth < min_depth) {
			min_depth = e[i].data_._data.depth;
			min_depth_index = i;
		}
	}

	if (use_sub_index == -1) {
		use_sub_index = min_depth_index;

		if (use_sub_index == -1) {
			use_sub_index = 0;

			printf("# ERROR: sub_index < 0\n");
		}
	}

	tt_entry *const cur = &e[use_sub_index];

	tt_entry::u n;
	n._data.score     = int16_t(score);
	n._data.depth     = uint8_t(d);
	n._data.flags     = f;
	n._data.age       = age;
	n._data.m         = m.value();

	cur -> hash       = hash ^ n.data;
	cur -> data_.data = n.data;
}
