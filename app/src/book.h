#include <cstdint>
#include <cstdio>
#include <optional>
#include <random>
#include <libchess/Position.h>


class polyglot_book
{
private:
	FILE              *fh  { nullptr };
	std::random_device dev;
	std::mt19937       rng { dev()   };
	size_t             n   { 0       };

	void scan(const libchess::Position & p, const long start_index, const int direction, const long end, std::vector<std::pair<libchess::Move, int> > & moves_out);

public:
	polyglot_book(const std::string & filename);
	~polyglot_book();

	size_t size() const;

	std::optional<libchess::Move> query(const libchess::Position & p);
};
