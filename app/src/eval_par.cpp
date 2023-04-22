#include <assert.h>
#include <cmath>
#include <functional>
#include <numeric>

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__)
#include "libchess/Tuner.h"
#else
#include "tuner.h"
#endif

#include "eval_par.h"


eval_par::eval_par()
{
}

eval_par::~eval_par()
{
}

std::vector<libchess::TunableParameterP> eval_par::get_tunable_parameters() const
{
	std::vector<libchess::TunableParameterP> list;

	list.push_back(tune_bishop_count);
	list.push_back(tune_too_many_pawns);
	list.push_back(tune_zero_pawns);
	list.push_back(tune_isolated_pawns);
	list.push_back(tune_rook_on_open_file);
//	list.push_back(tune_mobility);
	list.push_back(tune_double_pawns);
//	list.push_back(tune_king_attacks);
	list.push_back(tune_bishop_open_diagonal);
	list.push_back(tune_pawn);
	list.push_back(tune_knight);
	list.push_back(tune_bishop);
	list.push_back(tune_rook);
	list.push_back(tune_queen);
//	list.push_back(tune_king);
//	list.push_back(tune_find_forks);
	list.push_back(tune_king_shield);
//	list.push_back(tune_development);
	list.push_back(tune_psq_mul);
	list.push_back(tune_psq_div);
	list.push_back(tune_edge_black_rank);
	list.push_back(tune_edge_black_file);
	list.push_back(tune_edge_white_rank);
	list.push_back(tune_edge_white_file);
	list.push_back(tune_big_delta_promotion);
	list.push_back(tune_big_delta);

	for(int i=0; i<2; i++) {
		for(int y=1; y<7; y++)
			list.push_back(tune_pp_scores[i][y]);
	}

	return list;
}

void eval_par::set_eval(const std::string & name, int value)
{
	if (name == tune_bishop_count.name())
		tune_bishop_count.set_value(value);
	else if (name == tune_too_many_pawns.name())
		tune_too_many_pawns.set_value(value);
	else if (name == tune_zero_pawns.name())
		tune_zero_pawns.set_value(value);
	else if (name == tune_isolated_pawns.name())
		tune_isolated_pawns.set_value(value);
	else if (name == tune_rook_on_open_file.name())
		tune_rook_on_open_file.set_value(value);
	else if (name == tune_double_pawns.name())
		tune_double_pawns.set_value(value);
	else if (name == tune_bishop_open_diagonal.name())
		tune_bishop_open_diagonal.set_value(value);
	else if (name == tune_pawn.name())
		tune_pawn.set_value(value);
	else if (name == tune_knight.name())
		tune_knight.set_value(value);
	else if (name == tune_bishop.name())
		tune_bishop.set_value(value);
	else if (name == tune_rook.name())
		tune_rook.set_value(value);
	else if (name == tune_queen.name())
		tune_queen.set_value(value);
	else if (name == tune_king_shield.name())
		tune_king_shield.set_value(value);
	else if (name == tune_psq_mul.name())
		tune_psq_mul.set_value(value);
	else if (name == tune_edge_black_rank.name())
		tune_edge_black_rank.set_value(value);
	else if (name == tune_edge_black_file.name())
		tune_edge_black_file.set_value(value);
	else if (name == tune_edge_white_rank.name())
		tune_edge_white_rank.set_value(value);
	else if (name == tune_edge_white_file.name())
		tune_edge_white_file.set_value(value);
	else if (name == tune_big_delta_promotion.name())
		tune_big_delta_promotion.set_value(value);
	else if (name == tune_big_delta.name())
		tune_big_delta.set_value(value);
	else if (name == tune_psq_div.name()) {
		if (value == 0)
			value = 1;
		tune_psq_div.set_value(value);
	}
	else {
		bool found = false;

		for(int i=0; i<2; i++) {
			for(int y=1; y<7; y++) {
				if (tune_pp_scores[i][y].name() == name) {
					tune_pp_scores[i][y].set_value(value);
					i = 2;
					found = true;
					break;
				}
			}
		}

		assert(found);
	}
}

eval_par default_parameters;
