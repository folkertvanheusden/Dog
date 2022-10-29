#pragma once

#include <stdint.h>

#include "libchess/Position.h"
#include "eval_par.h"


class PSQ
{
private:
	int16_t PawnPSTMG[64];
	int16_t KnightPSTMG[64];
	int16_t BishopPSTMG[64];
	int16_t RookPSTMG[64];
	int16_t QueenPSTMG[64];
	int16_t KingPSTMG[64];
	int16_t PawnPSTEG[64];
	int16_t KnightPSTEG[64];
	int16_t BishopPSTEG[64];
	int16_t RookPSTEG[64];
	int16_t QueenPSTEG[64];
	int16_t KingPSTEG[64];

	int16_t *const idx[2][8] = {
	        { PawnPSTMG, KnightPSTMG, BishopPSTMG, RookPSTMG, QueenPSTMG, KingPSTMG},
	        { PawnPSTEG, KnightPSTEG, BishopPSTEG, RookPSTEG, QueenPSTEG, KingPSTEG}
	};

public:
	PSQ(const eval_par & in);
	virtual ~PSQ();

	int psq(const libchess::Square sq, const libchess::Color c, const libchess::PieceType t, const int phase) const;
};
