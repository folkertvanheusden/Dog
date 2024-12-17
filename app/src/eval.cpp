#include <cstring>
#include <map>

#include "libchess/Position.h"
#include "eval_par.h"
#include "eval.h"
#include "psq.h"

int eval_piece(const libchess::PieceType piece, const eval_par & parameters)
{
	int values[] = { parameters.pawn, parameters.knight, parameters.bishop, parameters.rook, parameters.queen, 10000 };

	return values[piece];
}

int game_phase(const int counts[2][6], const eval_par & parameters)
{
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

#if 0
int count_mobility(libchess::Position & pos)
{
	int scores[2] = { 0, 0 };

	libchess::Bitboard occupancy = pos.occupancy_bb(), occ = occupancy & ~pos.piece_type_bb(libchess::constants::PAWN);
	while (occupancy) {
		// attacks_of_piece_on FIXME
		auto sq = occupancy.forward_bitscan();
		occupancy.forward_popbit();

		auto piece = pos.piece_on(sq);

		auto color = piece->color();

		auto type = piece->type();

		scores[color] += libchess::lookups::non_pawn_piece_type_attacks(type, sq, occ).popcount();
	}

	return scores[libchess::constants::WHITE] - scores[libchess::constants::BLACK];
}
#endif

#if 0
int find_forks(const libchess::Position & pos)
{
	int score = 0;

	libchess::Bitboard occupancy = pos.occupancy_bb();
	while (occupancy) {
		auto sq    = occupancy.forward_bitscan();
		occupancy.forward_popbit();

		auto piece = pos.piece_on(sq);
		auto color = piece->color();
		int  mul   = color == libchess::constants::WHITE ? 1 : -1;

		int n_attackers_w = pos.attackers_to(sq, libchess::constants::WHITE).popcount();
		if (n_attackers_w > 1)
			score -= (n_attackers_w - 1) * mul;

		int n_attackers_b = pos.attackers_to(sq, libchess::constants::BLACK).popcount();
		if (n_attackers_b > 1)
			score += (n_attackers_b - 1) * mul;
	}

	return score;
}
#endif

#if 0
int count_king_attacks(const libchess::Position & pos, const libchess::Color side)
{
	auto opp = !side;
	auto ksq = pos.king_square(side);
	auto x = ksq.file();
	auto y = ksq.rank();

	int score = 0;

	if (y > 0) {
		if (x > 0)
			score += pos.attackers_to(ksq - 9, opp).popcount();

		score += pos.attackers_to(ksq - 8, opp).popcount();

		if (x < 7)
			score += pos.attackers_to(ksq - 7, opp).popcount();
	}

	if (x > 0)
		score += pos.attackers_to(ksq - 1, opp).popcount();

	if (x < 7)
		score += pos.attackers_to(ksq + 1, opp).popcount();

	if (y < 7) {
		if (x < 7)
			score += pos.attackers_to(ksq + 9, opp).popcount();

		score += pos.attackers_to(ksq + 8, opp).popcount();

		if (x > 0)
			score += pos.attackers_to(ksq + 7, opp).popcount();
	}

	return score;
}
#endif

#if 0
bool is_piece(const libchess::Position & pos, const libchess::Color side, const libchess::PieceType pt, const libchess::Square sq)
{
	std::optional<libchess::Piece> p = pos.piece_on(sq);

	if (p.has_value() == false)
		return false;

	return p.value().color() == side && p.value().type() == pt;
}
#endif

bool is_piece(const libchess::Position & pos, const libchess::Color side, const libchess::PieceType pt, const int file, const int rank)
{
	libchess::Square sq = libchess::Square::from(libchess::File(file), libchess::Rank(rank)).value();
	std::optional<libchess::Piece> p = pos.piece_on(sq);

	if (p.has_value() == false)
		return false;

	return p.value().color() == side && p.value().type() == pt;
}

int king_shield(const libchess::Position & pos, const libchess::Color side)
{
	auto ksq    = pos.king_square(side);
	int  kx     = ksq.file();
	int  ky     = ksq.rank();
	int  checkx = kx;
	int  checky = 0;

	if (side == libchess::constants::WHITE)
		checky = ky == 7 ? 6 : ky + 1;
	else
		checky = ky == 0 ? 1 : ky - 1;

	int cnt = 0;
	if (checkx)
		cnt += is_piece(pos, side, libchess::constants::PAWN, checkx - 1, checky);
	cnt += is_piece(pos, side, libchess::constants::PAWN, checkx, checky);
	if (checkx < 7)
		cnt += is_piece(pos, side, libchess::constants::PAWN, checkx + 1, checky);

	return cnt;
}

#if 0
int development(const libchess::Position & pos)
{
	int score = 0;

	libchess::Bitboard occupancy = pos.occupancy_bb();
	while (occupancy) {
		auto sq = occupancy.forward_bitscan();
		occupancy.forward_popbit();

		auto piece = pos.piece_on(sq);

		auto type = piece->type();
		if (type == libchess::constants::KING)
			continue;

		auto color = piece->color();

		int y = sq.rank();

		if (color == libchess::constants::WHITE && y <= 1)
			score--;
		else if (color == libchess::constants::BLACK && y >= 6)
			score++;
	}

	return score;
}
#endif

void count_board(const libchess::Position & pos, int counts[2][6])
{
	for(libchess::Color color : libchess::constants::COLORS) {
		for(libchess::PieceType type : libchess::constants::PIECE_TYPES) {
			libchess::Bitboard piece_bb = pos.piece_type_bb(type, color);
			counts[color][type] += piece_bb.popcount();
		}
	}
}

void scan_pawns(const libchess::Position & pos, int whiteYmax[8], int blackYmin[8], int n_pawns_w[8], int n_pawns_b[8])
{
	for(libchess::Color color : libchess::constants::COLORS) {
		libchess::Bitboard piece_bb = pos.piece_type_bb(libchess::constants::PAWN, color);
		while (piece_bb) {
			libchess::Square sq = piece_bb.forward_bitscan();
			piece_bb.forward_popbit();

			int x = sq.file();
			int y = sq.rank();

			if (color == libchess::constants::WHITE) {
				whiteYmax[x] = std::max(whiteYmax[x], y);
				n_pawns_w[x]++;
			}
			else {
				blackYmin[x] = std::min(blackYmin[x], y);
				n_pawns_b[x]++;
			}
		}
	}
}

int calc_psq(const libchess::Position & pos, const int phase, const eval_par & parameters)
{
	int score = 0;

	for(libchess::PieceType type : libchess::constants::PIECE_TYPES) {
		libchess::Bitboard piece_bb_w = pos.piece_type_bb(type, libchess::constants::WHITE);
		while (piece_bb_w) {
			libchess::Square sq = piece_bb_w.forward_bitscan();
			piece_bb_w.forward_popbit();
			score += psq(sq, libchess::constants::WHITE, type, phase);
		}

		libchess::Bitboard piece_bb_b = pos.piece_type_bb(type, libchess::constants::BLACK);
		while (piece_bb_b) {
			libchess::Square sq = piece_bb_b.forward_bitscan();
			piece_bb_b.forward_popbit();
			score -= psq(sq, libchess::constants::BLACK, type, phase);
		}
	}

	return score * parameters.psq_mul / parameters.psq_div;
}

int calc_passed_pawns(const libchess::Position & pos, int whiteYmax[8], int blackYmin[8], int n_pawns_w[8], int n_pawns_b[8], const eval_par & parameters)
{
	int score = 0;

	{
		libchess::Bitboard piece_bb_w = pos.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE);

		while(piece_bb_w) {
			libchess::Square sq = piece_bb_w.forward_bitscan();
			piece_bb_w.forward_popbit();
			int x = sq.file();
			int y = sq.rank();

			bool left = (x > 0 && (blackYmin[x - 1] <= y || blackYmin[x - 1] == 8)) || x == 0;
			bool front = blackYmin[x] < y || blackYmin[x] == 8;
			bool right = (x < 7 && (blackYmin[x + 1] <= y || blackYmin[x + 1] == 8)) || x == 7;

			if (left && front && right)
				score += *parameters.pp_scores[false][y];
			//score += (parameters.pp_scores[0][y].value() * (256 - phase) + parameters.pp_scores[1][y].value() * phase) / 256;
		}
	}

	{
		libchess::Bitboard piece_bb_b = pos.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK);

		while(piece_bb_b) {
			libchess::Square sq = piece_bb_b.forward_bitscan();
			piece_bb_b.forward_popbit();
			int x = sq.file();
			int y = sq.rank();

			bool left = (x > 0 && (whiteYmax[x - 1] >= y || whiteYmax[x - 1] == -1)) || x == 0;
			bool front = whiteYmax[x] > y || whiteYmax[x] == -1;
			bool right = (x < 7 && (whiteYmax[x + 1] >= y || whiteYmax[x + 1] == -1)) || x == 7;

			if (left && front && right)
				score -= *parameters.pp_scores[false][7 - y];
			// score -= (parameters.pp_scores[0][7 - y].value() * (256 - phase) + parameters.pp_scores[1][7 - y].value() * phase) / 256;
		}
	}

	return score;
}

int eval(const libchess::Position & pos, const eval_par & parameters)
{
	int score = 0;

	int counts[2][6] { };
	count_board(pos, counts);

	// material
	for(libchess::PieceType type : libchess::constants::PIECE_TYPES) {
		score += eval_piece(type, parameters) * counts[libchess::constants::WHITE][type];
		score -= eval_piece(type, parameters) * counts[libchess::constants::BLACK][type];
	}

	int whiteYmax[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
        int blackYmin[8] = { 8, 8, 8, 8, 8, 8, 8, 8 };
	int n_pawns_w[8] { };
	int n_pawns_b[8] { };
	scan_pawns(pos, whiteYmax, blackYmin, n_pawns_w, n_pawns_b);

	int phase = game_phase(counts, parameters);

	score += calc_psq(pos, phase, parameters);

	score += calc_passed_pawns(pos, whiteYmax, blackYmin, n_pawns_w, n_pawns_b, parameters);

	if (phase >= 224) { // endgame?
		int scores[] = { 20, 10, 5, 0, 0, 5, 10, 20 };  

		libchess::Square kw = pos.king_square(libchess::constants::WHITE);
		libchess::Square kb = pos.king_square(libchess::constants::BLACK);

		score += scores[kb.rank()] * parameters.edge_black_rank;
		score += scores[kb.file()] * parameters.edge_black_file;

		score -= scores[kw.rank()] * parameters.edge_white_rank;
		score -= scores[kw.file()] * parameters.edge_white_file;
	}

//	score += development(pos) * parameters.development;

	// forks
//	score += find_forks(pos);

	// number of bishops
	score += ((counts[libchess::constants::WHITE][libchess::constants::BISHOP] >= 2) - (counts[libchess::constants::BLACK][libchess::constants::BISHOP] >= 2)) * parameters.bishop_count;

	// 8 pawns: not good
	score += ((counts[libchess::constants::WHITE][libchess::constants::PAWN] == 8) - (counts[libchess::constants::BLACK][libchess::constants::PAWN] == 8)) * parameters.too_many_pawns;

	// 0 pawns: also not good
	score += ((counts[libchess::constants::WHITE][libchess::constants::PAWN] == 0) - (counts[libchess::constants::BLACK][libchess::constants::PAWN] == 0)) * parameters.zero_pawns;

	const auto bb_rooks_w = pos.piece_type_bb(libchess::constants::ROOK, libchess::constants::WHITE);
	const auto bb_rooks_b = pos.piece_type_bb(libchess::constants::ROOK, libchess::constants::BLACK);

	for(libchess::File x=libchess::constants::FILE_A; x<=libchess::constants::FILE_H; x++) {
		// double pawns
		if (n_pawns_w[x] >= 2)
			score -= (n_pawns_w[x] - 1) * parameters.double_pawns;
		if (n_pawns_b[x] >= 2)
			score += (n_pawns_b[x] - 1) * parameters.double_pawns;

		// rooks on open files
		int n_rooks_w = (bb_rooks_w & libchess::lookups::file_mask(x)).popcount();
		int n_rooks_b = (bb_rooks_b & libchess::lookups::file_mask(x)).popcount();

		score += ((n_pawns_w[x] == 0 && n_rooks_w > 0) - (n_pawns_b[x] == 0 && n_rooks_b > 0)) * parameters.rook_on_open_file;

		// isolated pawns
		int wleft = x > 0 ? n_pawns_w[x - 1] : 0;
		int wright = x < 7 ? n_pawns_w[x + 1] : 0;
		score += (wleft == 0 && wright == 0) * parameters.isolated_pawns;

		int bleft = x > 0 ? n_pawns_b[x - 1] : 0;
		int bright = x < 7 ? n_pawns_b[x + 1] : 0;
		score -= (bleft == 0 && bright == 0) * parameters.isolated_pawns;
	}

	// score += count_mobility(pos) * parameters.mobility / 10;

	// score -= (count_king_attacks(pos, libchess::constants::WHITE) - count_king_attacks(pos, libchess::constants::BLACK)) * parameters.king_attacks;

	score += (king_shield(pos, libchess::constants::WHITE) - king_shield(pos, libchess::constants::BLACK)) * parameters.king_shield;

	if (pos.side_to_move() != libchess::constants::WHITE)
		return -score;

	return score;
}
