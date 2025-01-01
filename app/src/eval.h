#include "eval_par.h"

int eval(const libchess::Position & pos, const eval_par & parameters);

// exported for testing
bool is_piece   (const libchess::Position & pos, const libchess::Color side, const libchess::PieceType pt, const int file, const int rank);
int  game_phase (const int counts[2][6], const eval_par & parameters);
void count_board(const libchess::Position & pos, int counts[2][6]);
void scan_pawns (const libchess::Position & pos, int whiteYmax[8], int blackYmin[8], int n_pawns_w[8], int n_pawns_b[8]);
int  calc_psq   (const libchess::Position & pos, const int phase, const eval_par & parameters);
int  calc_passed_pawns(const libchess::Position & pos, int whiteYmax[8], int blackYmin[8], int n_pawns_w[8], int n_pawns_b[8], const eval_par & parameters);
int  king_shield(const libchess::Position & pos, const libchess::Color side);
