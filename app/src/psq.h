#include "libchess/Position.h"

extern int PawnPSTMG[64];
extern int PawnPSTEG[64];
extern int KnightPSTMG[64];
extern int KnightPSTEG[64];
extern int BishopPSTMG[64];
extern int BishopPSTEG[64];
extern int RookPSTMG[64];
extern int RookPSTEG[64];
extern int QueenPSTMG[64];
extern int QueenPSTEG[64];
extern int KingPSTMG[64];
extern int KingPSTEG[64];

int psq(const libchess::Square sq, const libchess::Color c, const libchess::PieceType t, const int phase);
