#include <cstring>
#include <map>

#include "libchess/Position.h"


int game_phase(const libchess::Position & pos)
{
	int counts[2][6] { };

	for(libchess::Color color : libchess::constants::COLORS) {
		for(libchess::PieceType type : libchess::constants::PIECE_TYPES)
			counts[color][type] = pos.piece_type_bb(type, color).popcount();
	}

        const int num_knights = counts[libchess::constants::WHITE][libchess::constants::KNIGHT] + counts[libchess::constants::BLACK][libchess::constants::KNIGHT];
        const int num_bishops = counts[libchess::constants::WHITE][libchess::constants::BISHOP] + counts[libchess::constants::BLACK][libchess::constants::BISHOP];
        const int num_rooks   = counts[libchess::constants::WHITE][libchess::constants::ROOK]   + counts[libchess::constants::BLACK][libchess::constants::ROOK];
        const int num_queens  = counts[libchess::constants::WHITE][libchess::constants::QUEEN]  + counts[libchess::constants::BLACK][libchess::constants::QUEEN];

        // from https://www.chessprogramming.org/Tapered_Eval
        constexpr int knight_phase = 1;
        constexpr int bishop_phase = 1;
        constexpr int rook_phase   = 2;
        constexpr int queen_phase  = 4;

        constexpr int total_phase = knight_phase * 4 + bishop_phase * 4 + rook_phase * 4 + queen_phase * 2;

        int phase = total_phase;

        phase -= knight_phase * num_knights;
        phase -= bishop_phase * num_bishops;
        phase -= rook_phase   * num_rooks;
        phase -= queen_phase  * num_queens;

        return (phase * 256 + (total_phase / 2)) / total_phase;
}

// white, black
std::pair<int, int> count_mobility(const libchess::Position & pos)
{
	int scores[2] = { 0, 0 };

	libchess::Bitboard occupancy = pos.occupancy_bb();
	libchess::Bitboard occ       = occupancy & ~pos.piece_type_bb(libchess::constants::PAWN);
	while (occupancy) {
		auto sq = occupancy.forward_bitscan();
		occupancy.forward_popbit();

		auto piece = pos.piece_on(sq);
		auto color = piece->color();
		auto type  = piece->type();

		scores[color] += libchess::lookups::non_pawn_piece_type_attacks(type, sq, occ).popcount();
	}

	return { scores[libchess::constants::WHITE], scores[libchess::constants::BLACK] };
}

// white, black
std::pair<int, int> development(const libchess::Position & pos)
{
	int scores[2] = { 0, 0 };

	libchess::Bitboard occupancy = pos.occupancy_bb();
	while (occupancy) {
		auto sq = occupancy.forward_bitscan();
		occupancy.forward_popbit();

		auto piece = pos.piece_on(sq);
		auto type  = piece->type();
		if (type == libchess::constants::KING)
			continue;

		auto color = piece->color();
		int  y     = sq.rank();

		if (color == libchess::constants::WHITE && y > 1)
			scores[libchess::constants::WHITE]++;
		else if (color == libchess::constants::BLACK && y < 6)
			scores[libchess::constants::BLACK]++;
	}

	return { scores[libchess::constants::WHITE], scores[libchess::constants::BLACK] };
}
