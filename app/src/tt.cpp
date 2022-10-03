#include <cstdlib>
#include <cstring>

#include "libchess/Position.h"
#include "tt.h"

tt::tt(size_t size_in_bytes) : entries(nullptr)
{
	resize(size_in_bytes);
}

tt::~tt()
{
	delete [] entries;
}

void tt::resize(size_t size_in_bytes)
{
	if (entries)
		delete [] entries;

	n_entries = size_in_bytes / sizeof(tt_hash_group);

	printf("# TT: %llu buckets\n", n_entries);

	entries   = new tt_hash_group[n_entries];
	memset(entries, 0x00, sizeof(tt_hash_group) * n_entries);

	age = 0;
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
		tt_entry *const cur = &e[i];

		if ((cur -> hash ^ cur -> data_.data) == hash) {
			cur->data_._data.age = age;
			cur->hash = hash ^ cur->data_.data;

			return *cur;
		}
	}

	return { };
}

void tt::store(const uint64_t hash, const tt_entry_flag f, const int d, const int score, const libchess::Move & m)
{
	unsigned long int index = hash % n_entries; // FIXME is biased at the bottom of the list

	tt_entry   *const e     = entries[index].entries;

	int useSubIndex = -1;
	int minDepth    = 999;
	int mdi         = -1;

	for(int i=0; i<N_TE_PER_HASH_GROUP; i++)
	{
		if ((e[i].hash ^ e[i].data_.data) == hash) {
			if (e[i].data_._data.depth > d) {
				e[i].data_._data.age = age;
				e[i].hash = hash ^ e[i].data_.data;
				return;
			}
			if (f!=EXACT && e[i].data_._data.depth==d) {
				e[i].data_._data.age = age;
				e[i].hash = hash ^ e[i].data_.data;
				return;
			}

			useSubIndex = i;

			break;
		}

		if (e[i].data_._data.age != age)
			useSubIndex = i;
		else if (e[i].data_._data.depth < minDepth) {
			minDepth = e[i].data_._data.depth;
			mdi = i;
		}
	}

	if (useSubIndex == -1)
		useSubIndex = mdi;

	tt_entry *const cur = &e[useSubIndex];

	tt_entry::u n;
	n._data.score = int16_t(score);
	n._data.depth = uint8_t(d);
	n._data.flags = f;
	n._data.age   = age;
	n._data.m     = m.value();

	cur -> hash       = hash ^ n.data;
	cur -> data_.data = n.data;
}
