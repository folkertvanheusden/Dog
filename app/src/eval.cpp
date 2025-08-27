#include <cassert>
#include <libchess/Position.h>
#include "eval.h"
#include "nnue.h"


using namespace libchess;

int nnue_evaluate(Eval *const e, const Position & pos)
{
        return e->evaluate(pos.side_to_move() == constants::WHITE);
}

int nnue_evaluate(Eval *const e, const Color & c)
{
        return e->evaluate(c == constants::WHITE);
}

void init_move(Eval *const e, const libchess::Position & pos)
{
	e->set(pos);
}

void remove_piece(Eval *const e, const Square & loc, const PieceType & pt, const bool is_white, std::vector<undo_t> *const undos)
{
	e->remove_piece(pt, loc, is_white);
	undos->push_back({ true, loc, pt, is_white });
}

void add_piece(Eval *const e, const Square & loc, const PieceType & pt, const bool is_white, std::vector<undo_t> *const undos)
{
	e->add_piece(pt, loc, is_white);
	undos->push_back({ false, loc, pt, is_white });
}

void move_piece(Eval *const e, const Square & from, const Square & to, const PieceType & pt, const bool is_white, std::vector<undo_t> *const undos)
{
	remove_piece(e, from, pt, is_white, undos);
	add_piece   (e, to,   pt, is_white, undos);
}

std::vector<undo_t> make_move(Eval *const e, Position & pos, const Move & move)
{
	std::vector<undo_t> actions;

	Square from_square = move.from_square();
	Square to_square   = move.to_square  ();

	auto moving_pt    = pos.piece_type_on(from_square);
	assert(moving_pt.has_value());
	auto captured_pt  = pos.piece_type_on(to_square  );
	auto promotion_pt = move.promotion_piece_type();

	bool is_white     = pos.side_to_move() == constants::WHITE;

	switch(move.type()) {
		case Move::Type::NORMAL:
		case Move::Type::DOUBLE_PUSH:
			assert(moving_pt.has_value());
			move_piece(e, from_square, to_square, *moving_pt, is_white, &actions);
			assert(*moving_pt == constants::PAWN || move.type() != Move::Type::DOUBLE_PUSH);
			break;
		case Move::Type::CAPTURE:
			assert(captured_pt.has_value());
			remove_piece(e, to_square, *captured_pt, !is_white, &actions);
			move_piece(e, from_square, to_square, *moving_pt, is_white, &actions);
			break;
		case Move::Type::ENPASSANT:
			assert(*moving_pt == constants::PAWN);
			move_piece(e, from_square, to_square, constants::PAWN, is_white, &actions);
			assert(pos.piece_type_on(is_white ? Square(to_square - 8) : Square(to_square + 8)) == constants::PAWN);
			remove_piece(e, is_white ? Square(to_square - 8) : Square(to_square + 8), constants::PAWN, !is_white, &actions);
			break;
		case Move::Type::CASTLING:
			assert(*moving_pt == constants::KING);
			move_piece(e, from_square, to_square, constants::KING, is_white, &actions);
			switch (to_square) {
				case constants::C1:
					assert(is_white);
					move_piece(e, constants::A1, constants::D1, constants::ROOK, true, &actions);
					break;
				case constants::G1:
					assert(is_white);
					move_piece(e, constants::H1, constants::F1, constants::ROOK, true, &actions);
					break;
				case constants::C8:
					assert(!is_white);
					move_piece(e, constants::A8, constants::D8, constants::ROOK, false, &actions);
					break;
				case constants::G8:
					assert(!is_white);
					move_piece(e, constants::H8, constants::F8, constants::ROOK, false, &actions);
					break;
				default:
					assert(false);
					break;
			}
			break;
		case Move::Type::PROMOTION:
			assert(*moving_pt == constants::PAWN);
			assert(*promotion_pt != constants::PAWN);
			remove_piece(e, from_square, constants::PAWN, is_white, &actions);
			add_piece   (e, to_square,  *promotion_pt,    is_white, &actions);
			break;
		case Move::Type::CAPTURE_PROMOTION:
			assert(*moving_pt == constants::PAWN);
			assert(*promotion_pt != constants::PAWN);
			remove_piece(e, to_square,  *captured_pt,    !is_white, &actions);
			remove_piece(e, from_square, constants::PAWN, is_white, &actions);
			add_piece   (e, to_square,  *promotion_pt,    is_white, &actions);
			break;
		default:
			printf("type is %d\n", int(move.type()));
			assert(false);
			break;
	}

	pos.make_move(move);

	return actions;
}

void unmake_move(Eval *const e, Position & pos, const std::vector<undo_t> & actions)
{
	for(auto & action: actions) {
		if (action.is_put)
			e->add_piece(action.type, action.location, action.is_white);
		else
			e->remove_piece(action.type, action.location, action.is_white);
	}

	pos.unmake_move();
}
