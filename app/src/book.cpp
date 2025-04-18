#include <cassert>
#include <cstring>
#include <errno.h>

#include "book.h"
#include "san.h"
#include "tui.h"


#define my_HTONLL(x) ((1==my_HTONL(1)) ? (x) : (((uint64_t)my_HTONL((x) & 0xFFFFFFFFUL)) << 32) | my_HTONL((uint32_t)((x) >> 32)))
#define my_NTOHLL(x) ((1==my_NTOHL(1)) ? (x) : (((uint64_t)my_NTOHL((x) & 0xFFFFFFFFUL)) << 32) | my_NTOHL((uint32_t)((x) >> 32)))

uint16_t my_HTONS(const uint16_t x)
{
        constexpr const uint16_t e = 1;
        if (*reinterpret_cast<const uint8_t *>(&e))  // system is little endian
                return (x << 8) | (x >> 8);

        return x;
}

uint32_t my_HTONL(const uint32_t x)
{
        constexpr const uint16_t e = 1;
        if (*reinterpret_cast<const uint8_t *>(&e))  // system is little endian
                return (my_HTONS(x) << 16) | my_HTONS(x >> 16);

        return x;
}

uint16_t my_NTOHS(const uint16_t x)
{
        return my_HTONS(x);
}

uint32_t my_NTOHL(const uint32_t x)
{
        return my_HTONL(x);
}

struct polyglot_entry {
	uint64_t hash;
	uint16_t move;
	uint16_t weight;
	uint32_t learn;

	bool operator<(const polyglot_entry & b) const
	{
		return hash < b.hash;
	}
} __attribute__ ((__packed__));

static_assert(sizeof(polyglot_entry) == 16, "Polyglot entry must be 16 bytes in size");

polyglot_book::polyglot_book(const std::string & filename)
{
	fh = fopen(filename.c_str(), "rb");
	if (!fh)
		printf("Failed to open book %s: %s\n", filename.c_str(), strerror(errno));
}

polyglot_book::~polyglot_book()
{
	if (fh)
		fclose(fh);
}

libchess::Move convert_polyglot_move(const uint16_t & move, const libchess::Position & p)
{
	int  to_y    = (move >> 3) & 7;
	auto sq_to   = libchess::Square::from(libchess::File(move & 7       ), libchess::Rank(to_y)           ).value();
	auto sq_from = libchess::Square::from(libchess::File((move >> 6) & 7), libchess::Rank((move >> 9) & 7)).value();

	assert(p.piece_on(sq_from).has_value());
	bool is_capture = p.piece_on(sq_to).has_value();

	int                 promotion_type = (move >> 12) & 3;
	libchess::PieceType promote_to { p.piece_type_on(sq_from).value() };
	if (promotion_type == 0)
		assert(p.piece_type_on(sq_from).value() != libchess::constants::PAWN || (to_y != 0 && to_y != 7));
	else if (p.piece_type_on(sq_from).value() == libchess::constants::PAWN) {
		libchess::PieceType promotions[] { libchess::constants::PAWN /* invalid */, libchess::constants::KNIGHT,
			libchess::constants::BISHOP, libchess::constants::ROOK, libchess::constants::QUEEN };
		promote_to = promotions[promotion_type];
	}

	libchess::Move::Type type { libchess::Move::Type::NONE };

	if (promotion_type) {
		type = is_capture ? libchess::Move::Type::CAPTURE_PROMOTION : libchess::Move::Type::PROMOTION;
		return libchess::Move(sq_from, sq_to, promote_to, type);
	}

	if (is_capture)
		type = libchess::Move::Type::CAPTURE;
	else if ((sq_from == libchess::constants::E1 && (sq_to == libchess::constants::A1 || sq_to == libchess::constants::H1)) ||
		 (sq_from == libchess::constants::E8 && (sq_to == libchess::constants::A8 || sq_to == libchess::constants::H8))) {
		type = libchess::Move::Type::CASTLING;
	}
	else if (p.piece_type_on(sq_from).value() == libchess::constants::PAWN && (sq_to.rank() == 3 || sq_to.rank() == 4)) {
		type = libchess::Move::Type::DOUBLE_PUSH;
	}
	else {
		type = libchess::Move::Type::NORMAL;
	}

	return libchess::Move(sq_from, sq_to, type);
}

void polyglot_book::scan(const libchess::Position & p, const long start_index, const int direction, const long end, std::vector<std::pair<libchess::Move, int> > & moves_out)
{
	const uint64_t hash  { p.hash() };
	polyglot_entry entry {          };

	long index = start_index;
	for(;;) {
		index += direction;
		if (index == end)
			break;
		if (fseek(fh, index * sizeof(polyglot_entry), SEEK_SET) == -1) {
			printf("Seek in book failed: %s\n", strerror(errno));
			break;
		}
		if (fread(&entry, sizeof(polyglot_entry), 1, fh) != 1) {
			printf("Problem reading from book: %s\n", strerror(errno));
			break;
		}
		if (my_NTOHLL(entry.hash) != hash)
			break;
		auto move = convert_polyglot_move(my_NTOHS(entry.move), p);
		if (p.is_legal_move(move))
			moves_out.push_back({ convert_polyglot_move(my_NTOHS(entry.move), p), my_NTOHS(entry.weight) });
		else
			printf("Book: hash collision! (%s)\n", move.to_str().c_str());
	}
}

std::optional<libchess::Move> polyglot_book::query(const libchess::Position & p)
{
	if (!fh)
		return { };

	const uint64_t hash = p.hash();

	fseek(fh, 0, SEEK_END);
	const long size = ftell(fh);
	const long n    = size / sizeof(polyglot_entry);
	assert((size % sizeof(polyglot_entry)) == 0);

	long low  = 0;
	long high = n;

	while(low < high) {
		size_t mid = (low + high) / 2;

		if (fseek(fh, mid * sizeof(polyglot_entry), SEEK_SET) == -1) {
			printf("Seek in book failed: %s\n", strerror(errno));
			break;
		}
		polyglot_entry entry { };
		if (fread(&entry, sizeof(polyglot_entry), 1, fh) != 1) {
			printf("Problem reading from book: %s\n", strerror(errno));
			break;
		}

		if (my_NTOHLL(entry.hash) < hash)
			low = mid + 1;
		else if (my_NTOHLL(entry.hash) > hash)
			high = mid;
		else {
			// entries are surround the current
			size_t index = mid;

			std::vector<std::pair<libchess::Move, int> > moves;
			moves.push_back({ convert_polyglot_move(my_NTOHS(entry.move), p), my_NTOHS(entry.weight) });

			scan(p, index, -1, -1, moves);  // backward search
			scan(p, index,  1,  n, moves);  // forward serach

			printf("Selecting from %zu move(s) (", moves.size());
			bool first = true;
			for(auto & m: moves) {
				if (first)
					first = false;
				else
					printf(" ");
				printf("%s", m.first.to_str().c_str());
			}
			printf(")...\n");

			int use_weight = 0;
			if (moves.size() > 4) {
				int sum = 0;
				for(auto & m : moves)
					sum += m.second;
				use_weight = sum / moves.size();
			}

			for(;;) {
				std::uniform_int_distribution<std::mt19937::result_type> dist(0, moves.size() - 1);
				size_t nr = dist(rng);
				auto & m  = moves.at(nr);
				if (m.second >= use_weight) {
					my_printf("Minimum move weight: %d, chosen move weight: %d\n", use_weight, m.second);
					return m.first;
				}
			}
		}
	}

	return { };
}
