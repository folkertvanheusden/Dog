#include <assert.h>
#include <cmath>
#include <functional>
#include <numeric>

#include "eval_par.h"


eval_par::eval_par()
{
}

eval_par::eval_par(const std::vector<libchess::TunableParameter> & params)
{
	for(auto & p : params)
		*m.find(p.name())->second = p.value();

	if (*m.find("tune_psq_div")->second == 0)
		*m.find("tune_psq_div")->second = 1;

	piece_values[0] = *m.find("tune_pawn"  )->second;
	piece_values[1] = *m.find("tune_knight")->second;
	piece_values[2] = *m.find("tune_bishop")->second;
	piece_values[3] = *m.find("tune_rook"  )->second;
	piece_values[4] = *m.find("tune_queen" )->second;
	piece_values[5] = 10000;
}

eval_par::~eval_par()
{
}

std::vector<libchess::TunableParameter> eval_par::get_tunable_parameters() const
{
	std::vector<libchess::TunableParameter> list;
	list.reserve(m.size());
	for(auto & it : m)
		list.push_back({ it.first, *it.second });

	return list;
}

void eval_par::set_eval(const std::string & name, int value)
{
	auto it = m.find(name);

	*it->second = value;
}

int eval_par::eval_piece(const libchess::PieceType piece) const
{
	return piece_values[piece];
}

eval_par default_parameters;
