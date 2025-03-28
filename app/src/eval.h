#include "nnue.h"

int nnue_evaluate(Eval *const e, const libchess::Position & pos);
int nnue_evaluate(Eval *const e, const libchess::Color & c);

struct undo_t
{
	bool                is_put;  // else remove
	libchess::Square    location;
	libchess::PieceType type;
	bool                is_white;
};

void                init_move  (Eval *const e, const libchess::Position & pos);
void                unmake_move(Eval *const e, libchess::Position & pos, const std::vector<undo_t> & actions);
std::vector<undo_t> make_move  (Eval *const e, libchess::Position & pos, const libchess::Move & move);
