#include "eval_par.h"
#include "psq.h"


int eval_piece(libchess::PieceType piece, const eval_par & parameters);
int eval(libchess::Position & pos, const eval_par & parameters, const PSQ & psq);
