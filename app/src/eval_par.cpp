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
	m.insert({ "tune_bishop_count", &bishop_count });
	m.insert({ "tune_too_many_pawns", &too_many_pawns });
	m.insert({ "tune_zero_pawns", &zero_pawns });
	m.insert({ "tune_isolated_pawns", &isolated_pawns });
	m.insert({ "tune_rook_on_open_file", &rook_on_open_file });
	m.insert({ "tune_double_pawns", &double_pawns });
	m.insert({ "tune_bishop_open_diagonal", &bishop_open_diagonal });
	m.insert({ "tune_pawn", &pawn });
	m.insert({ "tune_knight", &knight });
	m.insert({ "tune_bishop", &bishop });
	m.insert({ "tune_rook", &rook });
	m.insert({ "tune_queen", &queen });
	m.insert({ "tune_king_shield", &king_shield });
	m.insert({ "tune_psq_mul", &psq_mul });
	m.insert({ "tune_psq_div", &psq_div });
	m.insert({ "tune_edge_black_rank", &edge_black_rank });
	m.insert({ "tune_edge_black_file", &edge_black_file });
	m.insert({ "tune_edge_white_rank", &edge_white_rank });
	m.insert({ "tune_edge_white_file", &edge_white_file });
	m.insert({ "tune_big_delta_promotion", &big_delta_promotion });
	m.insert({ "tune_big_delta", &big_delta });
#if 0
	m.insert({ "tune_mobility", &mobility });
	m.insert({ "tune_mobil_pawn", &mobil_pawn });
	m.insert({ "tune_mobil_knight", &mobil_knight });
	m.insert({ "tune_mobil_bishop", &mobil_bishop });
	m.insert({ "tune_mobil_rook", &mobil_rook });
	m.insert({ "tune_mobil_queen", &mobil_queen });
	m.insert({ "tune_mobil_king", &mobil_king });
#endif
	m.insert({ "tune_pp_scores_mg_0", &pp_scores_mg_0 });
	m.insert({ "tune_pp_scores_mg_1", &pp_scores_mg_1 });
	m.insert({ "tune_pp_scores_mg_2", &pp_scores_mg_2 });
	m.insert({ "tune_pp_scores_mg_3", &pp_scores_mg_3 });
	m.insert({ "tune_pp_scores_mg_4", &pp_scores_mg_4 });
	m.insert({ "tune_pp_scores_mg_5", &pp_scores_mg_5 });
	m.insert({ "tune_pp_scores_mg_6", &pp_scores_mg_6 });
	m.insert({ "tune_pp_scores_mg_7", &pp_scores_mg_7 });
	m.insert({ "tune_pp_scores_eg_0", &pp_scores_eg_0 });
	m.insert({ "tune_pp_scores_eg_1", &pp_scores_eg_1 });
	m.insert({ "tune_pp_scores_eg_2", &pp_scores_eg_2 });
	m.insert({ "tune_pp_scores_eg_3", &pp_scores_eg_3 });
	m.insert({ "tune_pp_scores_eg_4", &pp_scores_eg_4 });
	m.insert({ "tune_pp_scores_eg_5", &pp_scores_eg_5 });
	m.insert({ "tune_pp_scores_eg_6", &pp_scores_eg_6 });
	m.insert({ "tune_pp_scores_eg_7", &pp_scores_eg_7 });
	m.insert({ "tune_pp_scores_mg_0", &pp_scores_mg_0 });
	m.insert({ "tune_pp_scores_mg_1", &pp_scores_mg_1 });
	m.insert({ "tune_pp_scores_mg_2", &pp_scores_mg_2 });
	m.insert({ "tune_pp_scores_mg_3", &pp_scores_mg_3 });
	m.insert({ "tune_pp_scores_mg_4", &pp_scores_mg_4 });
	m.insert({ "tune_pp_scores_mg_5", &pp_scores_mg_5 });
	m.insert({ "tune_pp_scores_mg_6", &pp_scores_mg_6 });
	m.insert({ "tune_pp_scores_mg_7", &pp_scores_mg_7 });
	m.insert({ "tune_pp_scores_eg_0", &pp_scores_eg_0 });
	m.insert({ "tune_pp_scores_eg_1", &pp_scores_eg_1 });
	m.insert({ "tune_pp_scores_eg_2", &pp_scores_eg_2 });
	m.insert({ "tune_pp_scores_eg_3", &pp_scores_eg_3 });
	m.insert({ "tune_pp_scores_eg_4", &pp_scores_eg_4 });
	m.insert({ "tune_pp_scores_eg_5", &pp_scores_eg_5 });
	m.insert({ "tune_pp_scores_eg_6", &pp_scores_eg_6 });
	m.insert({ "tune_pp_scores_eg_7", &pp_scores_eg_7 });
}

eval_par::~eval_par()
{
}

std::vector<libchess::TunableParameter> eval_par::get_tunable_parameters() const
{
	std::vector<libchess::TunableParameter> list;

	for(auto & it : m)
		list.push_back({ it.first, *it.second });

	return list;
}

void eval_par::set_eval(const std::string & name, int value)
{
	auto it = m.find(name);

	*it->second = value;
}

eval_par default_parameters;
