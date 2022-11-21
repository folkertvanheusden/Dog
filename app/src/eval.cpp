#include <cstring>
#include <map>

#include "libchess/Position.h"
#include "eval_par.h"
#include "eval.h"
#include "psq.h"

int eval_piece(libchess::PieceType piece, const eval_par & parameters)
{
	int values[] = { parameters.tune_pawn.value(), parameters.tune_knight.value(), parameters.tune_bishop.value(), parameters.tune_rook.value(), parameters.tune_queen.value(), 10000 };

	return values[piece];
}

int game_phase(int counts[2][6], const eval_par & parameters)
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

int find_forks(libchess::Position & pos)
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

int count_king_attacks(libchess::Position & pos, libchess::Color side)
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

bool is_piece(libchess::Position & pos, libchess::Color side, libchess::PieceType pt, libchess::Square sq)
{
	std::optional<libchess::Piece> p = pos.piece_on(sq);

	if (p.has_value() == false)
		return false;

	return p.value().color() == side && p.value().type() == pt;
}

bool is_piece(libchess::Position & pos, libchess::Color side, libchess::PieceType pt, int file, int rank)
{
	libchess::Square sq = libchess::Square::from(libchess::File(file), libchess::Rank(rank)).value();
	std::optional<libchess::Piece> p = pos.piece_on(sq);

	if (p.has_value() == false)
		return false;

	return p.value().color() == side && p.value().type() == pt;
}

int king_shield(libchess::Position & pos, libchess::Color side)
{
	auto ksq = pos.king_square(libchess::constants::WHITE);
	if (ksq.rank())
		return 0;

	int kx = ksq.file(), checkx = kx;
	int checky, ky = ksq.rank();

	if (side == libchess::constants::WHITE)
		checky = ky == 7 ? 6 : ky + 1;
	else
		checky = ky == 0 ? 1 : ky - 1;

	int cnt = 0;

	if (checkx) {
		cnt += is_piece(pos, side, libchess::constants::PAWN, checkx - 1, checky);
		cnt += is_piece(pos, side, libchess::constants::PAWN, checkx - 1, ky);
	}

	cnt += is_piece(pos, side, libchess::constants::PAWN, checkx, checky);

	if (checkx < 7) {
		cnt += is_piece(pos, side, libchess::constants::PAWN, checkx + 1, checky);
		cnt += is_piece(pos, side, libchess::constants::PAWN, checkx + 1, ky);
	}

	return cnt;
}

#if 0
int development(libchess::Position & pos)
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

int eval(libchess::Position & pos, const eval_par & parameters)
{
	int score = 0;

	int counts[2][6];
	memset(counts, 0x00, sizeof counts);

	int whiteYmax[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
        int blackYmin[8] = { 8, 8, 8, 8, 8, 8, 8, 8 };

	for(libchess::Color color : libchess::constants::COLORS) {
		for(libchess::PieceType type : libchess::constants::PIECE_TYPES) {
			libchess::Bitboard piece_bb = pos.piece_type_bb(type, color);

			counts[color][type] += piece_bb.popcount();

			if (type == libchess::constants::PAWN) {
				while (piece_bb) {
					libchess::Square sq = piece_bb.forward_bitscan();
					piece_bb.forward_popbit();

					int x = sq.file();
					int y = sq.rank();

					if (color == libchess::constants::WHITE)
						whiteYmax[x] = std::max(whiteYmax[x], y);
					else
						blackYmin[x] = std::min(blackYmin[x], y);
				}
			}
		}
	}

	int phase = game_phase(counts, parameters);

	for(libchess::Color color : libchess::constants::COLORS) {
		int mul = color == libchess::constants::WHITE ? 1 : -1;

		for(libchess::PieceType type : libchess::constants::PIECE_TYPES) {
			libchess::Bitboard piece_bb = pos.piece_type_bb(type, color);

			// material
			score += eval_piece(type, parameters) * piece_bb.popcount() * mul;

			// psq
			while (piece_bb) {
				libchess::Square sq = piece_bb.forward_bitscan();
				piece_bb.forward_popbit();

				int psq_score = (psq(sq, color, type, phase) * mul * parameters.tune_psq_mul.value()) / parameters.tune_psq_div.value();
				score += psq_score;

				// passed pawns
				if (type == libchess::constants::PAWN) {
					int x = sq.file();
					int y = sq.rank();

					if (color == libchess::constants::WHITE) {
						bool left = (x > 0 && (blackYmin[x - 1] <= y || blackYmin[x - 1] == 8)) || x == 0;
						bool front = blackYmin[x] < y || blackYmin[x] == 8;
						bool right = (x < 7 && (blackYmin[x + 1] <= y || blackYmin[x + 1] == 8)) || x == 7;

						if (left && front && right)
							score += parameters.tune_pp_scores[false][y].value();
						//score += (parameters.tune_pp_scores[0][y].value() * (256 - phase) + parameters.tune_pp_scores[1][y].value() * phase) / 256;
					}
					else {
						bool left = (x > 0 && (whiteYmax[x - 1] >= y || whiteYmax[x - 1] == -1)) || x == 0;
						bool front = whiteYmax[x] > y || whiteYmax[x] == -1;
						bool right = (x < 7 && (whiteYmax[x + 1] >= y || whiteYmax[x + 1] == -1)) || x == 7;

						if (left && front && right)
							score -= parameters.tune_pp_scores[false][7 - y].value();
						// score -= (parameters.tune_pp_scores[0][7 - y].value() * (256 - phase) + parameters.tune_pp_scores[1][7 - y].value() * phase) / 256;
					}
				}
			}
		}
	}

	if (phase >= 224) { // endgame?
		int scores[] = { 20, 10, 5, 0, 0, 5, 10, 20 };  

		libchess::Square kw = pos.king_square(libchess::constants::WHITE);
		libchess::Square kb = pos.king_square(libchess::constants::BLACK);

		score += scores[kb.rank()] * parameters.tune_edge_black_rank.value();
		score += scores[kb.file()] * parameters.tune_edge_black_file.value();

		score -= scores[kw.rank()] * parameters.tune_edge_white_rank.value();
		score -= scores[kw.file()] * parameters.tune_edge_white_file.value();
	}

	// score += development(pos) * parameters.tune_development.value();

	// forks
	//score += find_forks(pos);

	// number of bishops
	score += ((counts[libchess::constants::WHITE][libchess::constants::BISHOP] >= 2) - (counts[libchess::constants::BLACK][libchess::constants::BISHOP] >= 2)) * parameters.tune_bishop_count.value();

	// 8 pawns: not good
	score += ((counts[libchess::constants::WHITE][libchess::constants::PAWN] == 8) - (counts[libchess::constants::BLACK][libchess::constants::PAWN] == 8)) * parameters.tune_too_many_pawns.value();

	// 0 pawns: also not good
	score += ((counts[libchess::constants::WHITE][libchess::constants::PAWN] == 0) - (counts[libchess::constants::BLACK][libchess::constants::PAWN] == 0)) * parameters.tune_zero_pawns.value();

	const auto bb_pawns_w = pos.piece_type_bb(libchess::constants::PAWN, libchess::constants::WHITE);
	const auto bb_pawns_b = pos.piece_type_bb(libchess::constants::PAWN, libchess::constants::BLACK);

	int n_pawns_w[8], n_pawns_b[8];

	for(libchess::File x=libchess::constants::FILE_A; x<=libchess::constants::FILE_H; x++) {
		n_pawns_w[x] = (bb_pawns_w & libchess::lookups::file_mask(x)).popcount();

		n_pawns_b[x] = (bb_pawns_b & libchess::lookups::file_mask(x)).popcount();
	}

	const auto bb_rooks_w = pos.piece_type_bb(libchess::constants::ROOK, libchess::constants::WHITE);
	const auto bb_rooks_b = pos.piece_type_bb(libchess::constants::ROOK, libchess::constants::BLACK);

	for(libchess::File x=libchess::constants::FILE_A; x<=libchess::constants::FILE_H; x++) {
		// double pawns
		if (n_pawns_w[x] >= 2)
			score -= (n_pawns_w[x] - 1) * parameters.tune_double_pawns.value();
		if (n_pawns_b[x] >= 2)
			score += (n_pawns_b[x] - 1) * parameters.tune_double_pawns.value();

		// rooks on open files
		int n_rooks_w = (bb_rooks_w & libchess::lookups::file_mask(x)).popcount();
		int n_rooks_b = (bb_rooks_b & libchess::lookups::file_mask(x)).popcount();

		score += ((n_pawns_w[x] == 0 && n_rooks_w > 0) - (n_pawns_b[x] == 0 && n_rooks_b > 0)) * parameters.tune_rook_on_open_file.value();

		// isolated pawns
		int wleft = x > 0 ? n_pawns_w[x - 1] : 0;
		int wright = x < 7 ? n_pawns_w[x + 1] : 0;
		score += (wleft == 0 && wright == 0) * parameters.tune_isolated_pawns.value();

		int bleft = x > 0 ? n_pawns_b[x - 1] : 0;
		int bright = x < 7 ? n_pawns_b[x + 1] : 0;
		score -= (bleft == 0 && bright == 0) * parameters.tune_isolated_pawns.value();
	}

	// score += count_mobility(pos) * parameters.tune_mobility.value() / 10;

	// score -= (count_king_attacks(pos, libchess::constants::WHITE) - count_king_attacks(pos, libchess::constants::BLACK)) * parameters.tune_king_attacks.value();

	score += (king_shield(pos, libchess::constants::WHITE) - king_shield(pos, libchess::constants::BLACK)) * parameters.tune_king_shield.value();

	if (pos.side_to_move() != libchess::constants::WHITE)
		return -score;

	return score;
}
