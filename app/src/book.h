#pragma once
#include <cstdint>
#include <cstdio>
#if defined(ESP32)
#include <esp_random.h>
#endif
#include <optional>
#include <random>
#include <libchess/Position.h>


class polyglot_book
{
private:
	FILE              *fh  { nullptr };
#if defined(ESP32)
	std::mt19937::result_type seed { esp_random() };
	std::mt19937       rng { seed    };
#else
	std::random_device dev;
	std::mt19937       rng { dev()   };
#endif
	size_t             n   { 0       };

	void scan(const libchess::Position & p, const long start_index, const int direction, const long end, std::vector<std::pair<libchess::Move, int> > & moves_out);

public:
	polyglot_book();
	~polyglot_book();

	bool   begin(const std::string & filename);

	size_t size() const;

	std::optional<libchess::Move> query(const libchess::Position & p, const bool verbose);
};
