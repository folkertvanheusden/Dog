#include <cstring>
#include <map>

#include "psq.h"

#include "libchess/Position.h"


PSQ::PSQ(const eval_par & in)
{
#include "psq_inc.h"
}

PSQ::~PSQ()
{
}

int PSQ::psq(const libchess::Square sq, const libchess::Color c, const libchess::PieceType t, const int phase) const
{
	const int pos = sq;
	const int index = c == libchess::constants::WHITE ? pos : (pos ^ 56);

	return (idx[0][t][index] * (255 - phase) + idx[1][t][index] * phase) / 256;
}
