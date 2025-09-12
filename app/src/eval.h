#include "nnue.h"

int nnue_evaluate(const Eval *const e, const libchess::Position & pos);
int nnue_evaluate(const Eval *const e, const libchess::Color & c);

struct undo_t
{
	libchess::Square    location;
	libchess::PieceType type;
	bool                is_white;
	bool                is_put;  // else remove
};

void init_move  (Eval *const e, const libchess::Position & pos);
void unmake_move(Eval *const e, libchess::Position & pos, const std::pair<int, std::array<undo_t, 4> > & actions);
std::pair<int, std::array<undo_t, 4> > make_move(Eval *const e, libchess::Position & pos, const libchess::Move & move);
