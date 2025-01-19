#include <libchess/Position.h>
#include "nnue.h"


// TODO replae the for-loop into something incremental during search
int nnue_evaluate(Eval *const e, const libchess::Position & pos)
{
	Eval temp_e;
        for(libchess::PieceType type : libchess::constants::PIECE_TYPES) {
                libchess::Bitboard piece_bb_w = pos.piece_type_bb(type, libchess::constants::WHITE);
                while (piece_bb_w) {
                        libchess::Square sq = piece_bb_w.forward_bitscan();
                        piece_bb_w.forward_popbit();
			temp_e.add_piece(type, sq, true);
                }

                libchess::Bitboard piece_bb_b = pos.piece_type_bb(type, libchess::constants::BLACK);
                while (piece_bb_b) {
                        libchess::Square sq = piece_bb_b.forward_bitscan();
                        piece_bb_b.forward_popbit();
			temp_e.add_piece(type, sq, false);
                }
        }

        return temp_e.evaluate(pos.side_to_move() == libchess::constants::WHITE);
}
