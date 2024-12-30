#include <optional>

#include "fathom/src/tbprobe.h"
#include "libchess/Position.h"

struct pos {
	uint64_t white;
	uint64_t black;
	uint64_t kings;
	uint64_t queens;
	uint64_t rooks;
	uint64_t bishops;
	uint64_t knights;
	uint64_t pawns;
	uint8_t  castling;
	uint8_t  rule50;
	uint8_t  ep;
	bool     turn;
};

static std::optional<std::pair<libchess::Move, int> > get_best_dtz_move(struct pos & pos, unsigned *results, unsigned wdl)
{
	unsigned selected_move = 0;
	int      best_dtz      = 1000;

	std::optional<std::pair<libchess::Move, int> > rc;

	for(unsigned i = 0; results[i] != TB_RESULT_FAILED; i++) {
		// printf("%d], %d, %d|%d\n", i, results[i], TB_GET_WDL(results[i]), wdl);
		if (TB_GET_WDL(results[i]) == wdl) {
			int dtz = TB_GET_DTZ(results[i]);

			if (dtz < best_dtz) {
				selected_move = results[i];
				best_dtz = dtz;
			}
		}
	}

	if (selected_move != 0) {
		// printf("selected move: %u\n", selected_move);

		unsigned from     = TB_GET_FROM(selected_move);
		unsigned to       = TB_GET_TO(selected_move);
		unsigned promotes = TB_GET_PROMOTES(selected_move);
		char     to_type  = 0x00;

		switch (promotes) {
			case TB_PROMOTES_QUEEN:
				to_type = 'q'; break;
			case TB_PROMOTES_ROOK:
				to_type = 'r'; break;
			case TB_PROMOTES_BISHOP:
				to_type = 'b'; break;
			case TB_PROMOTES_KNIGHT:
				to_type = 'n'; break;
		}

		char move_str[6] { char('a' + (from & 7)), char('1' + (from >> 3)), char('a' + (to & 7)), char('1' + (to >> 3)) };
		if (to_type)
			move_str[5] = to_type;

		auto move = libchess::Move::from(move_str);

		if (move.has_value())
			rc = std::pair{ move.value(), best_dtz };
	}

	return rc;
}

pos gen_parameters(const libchess::Position & lpos)
{
	pos pos { };
	pos.turn    = lpos.side_to_move() == libchess::constants::WHITE;
	pos.white   = lpos.color_bb(libchess::constants::WHITE);
	pos.black   = lpos.color_bb(libchess::constants::BLACK);
	pos.kings   = lpos.piece_type_bb(libchess::constants::KING);
	pos.queens  = lpos.piece_type_bb(libchess::constants::QUEEN);
	pos.rooks   = lpos.piece_type_bb(libchess::constants::ROOK);
	pos.bishops = lpos.piece_type_bb(libchess::constants::BISHOP);
	pos.knights = lpos.piece_type_bb(libchess::constants::KNIGHT);
	pos.pawns   = lpos.piece_type_bb(libchess::constants::PAWN);

	auto crights = lpos.castling_rights();
	pos.castling = 0;
	if (crights.is_allowed(libchess::constants::WHITE_KINGSIDE))
		pos.castling = TB_CASTLING_K;
	if (crights.is_allowed(libchess::constants::WHITE_QUEENSIDE))
		pos.castling = TB_CASTLING_Q;
	if (crights.is_allowed(libchess::constants::BLACK_KINGSIDE))
		pos.castling = TB_CASTLING_k;
	if (crights.is_allowed(libchess::constants::BLACK_QUEENSIDE))
		pos.castling = TB_CASTLING_q;

	pos.rule50 = lpos.halfmoves();

	std::optional<libchess::Square> ep = lpos.enpassant_square();
	pos.ep = ep.has_value() ? ep.value() : 0;

#if 0
	printf("# white: %lx\n", pos.white);
	printf("# black: %lx\n", pos.black);
	printf("# kings: %lx\n", pos.kings);
	printf("# queens: %lx\n", pos.queens);
	printf("# rooks: %lx\n", pos.rooks);
	printf("# bishops: %lx\n", pos.bishops);
	printf("# knights: %lx\n", pos.knights);
	printf("# pawns: %lx\n", pos.pawns);
	printf("# rule50: %d\n", pos.rule50);
	printf("# castling: %d\n", pos.castling);
	printf("# ep: %d\n", pos.ep);
	printf("# turn: %d\n", pos.turn);
#endif

	return pos;

}

std::optional<std::pair<libchess::Move, int> > probe_fathom_root(const libchess::Position & lpos)
{
	unsigned results[TB_MAX_MOVES] { };
	auto     pos = gen_parameters(lpos);
	unsigned res = tb_probe_root(pos.white, pos.black, pos.kings, pos.queens, pos.rooks, pos.bishops, pos.knights, pos.pawns, pos.rule50, pos.castling, pos.ep, pos.turn, results);

	if (res == TB_RESULT_FAILED) {
		printf("# TB_RESULT_FAILED\n");
		return { };
	}

	std::optional<std::pair<libchess::Move, int> > m;

	m = get_best_dtz_move(pos, results, TB_WIN);
	if (m.has_value())
		return { { m.value().first, 10000 - m.value().second } };

	m = get_best_dtz_move(pos, results, TB_CURSED_WIN);
	if (m.has_value())
		return { { m.value().first, 10000 - m.value().second } };

	m = get_best_dtz_move(pos, results, TB_DRAW);
	if (m.has_value())
		return { { m.value().first, - m.value().second } };  // Dog doesn't like draws

	m = get_best_dtz_move(pos, results, TB_BLESSED_LOSS);
	if (m.has_value())
		return { { m.value().first, -(10000 - m.value().second) } };

	m = get_best_dtz_move(pos, results, TB_LOSS);
	if (m.has_value())
		return { { m.value().first, -(10000 - m.value().second) } };

	return { };
}

std::optional<int> probe_fathom_nonroot(const libchess::Position & lpos)
{
	auto     pos = gen_parameters(lpos);
	unsigned res = tb_probe_wdl(pos.white, pos.black, pos.kings, pos.queens, pos.rooks, pos.bishops, pos.knights, pos.pawns, pos.rule50, pos.castling, pos.ep, pos.turn);
	if (res == TB_RESULT_FAILED) {
		// no hit
		return { };
	}

	int score  = 0;
	int result = TB_GET_WDL(res);
	if (result == TB_LOSS || result == TB_BLESSED_LOSS)
		score = -9999;
	else if (result == TB_DRAW)
		score = 0;
	else if (result == TB_CURSED_WIN || result == TB_WIN)
		score = 9999;
	else {
		printf("# unexpected return code from fathom: %d (%d)\n", result, res);
		return { };
	}

	return score;
}

void fathom_init(const std::string & path)
{
	tb_init(path.c_str());
	printf("# %d men syzygy\n", TB_LARGEST);
}

void fathom_deinit()
{
	tb_free();
}
