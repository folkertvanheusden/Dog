#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <numeric>
#include <string>

#include "libchess/Tuner.h"

#include "tune.h"

class eval_par
{
public:
	std::map<std::string, int *> m;

	int bishop_count { TUNE_BISHOP_COUNT };

	int too_many_pawns { TUNE_TOO_MANY_PAWNS };

	int zero_pawns { TUNE_ZERO_PAWNS };

	int isolated_pawns { TUNE_ISOLATED_PAWNS };

	int rook_on_open_file { TUNE_ROOK_ON_OPEN_FILE };

	int double_pawns { TUNE_DOUBLE_PAWNS };

	int bishop_open_diagonal { TUNE_BISHOP_OPEN_DIAGONAL };

	int pawn { TUNE_PAWN };

	int knight { TUNE_KNIGHT };

	int bishop { TUNE_BISHOP };

	int rook { TUNE_ROOK };

	int queen { TUNE_QUEEN };

	int king_shield { TUNE_KING_SHIELD };

	int psq_mul { TUNE_PSQ_MUL };

	int psq_div { TUNE_PSQ_DIV };

	int edge_black_rank { TUNE_EDGE_BLACK_RANK };

	int edge_black_file { TUNE_EDGE_BLACK_FILE };

	int edge_white_rank { TUNE_EDGE_WHITE_RANK };

	int edge_white_file { TUNE_EDGE_WHITE_FILE };

	int big_delta_promotion { TUNE_BIG_DELTA_PROMOTION };

	int big_delta { TUNE_BIG_DELTA };

#if 0
	int mobility { TUNE_MOBILITY };

	int mobil_pawn { TUNE_MOBIL_PAWN };

	int mobil_knight { TUNE_MOBIL_KNIGHT };

	int mobil_bishop { TUNE_MOBIL_BISHOP };

	int mobil_rook { TUNE_MOBIL_ROOK };

	int mobil_queen { TUNE_MOBIL_QUEEN };

	int mobil_king { TUNE_MOBIL_KING };
#endif

	int pp_scores_mg_0 { TUNE_PP_SCORES_MG_0 };

	int pp_scores_mg_1 { TUNE_PP_SCORES_MG_1 };

	int pp_scores_mg_2 { TUNE_PP_SCORES_MG_2 };

	int pp_scores_mg_3 { TUNE_PP_SCORES_MG_3 };

	int pp_scores_mg_4 { TUNE_PP_SCORES_MG_4 };

	int pp_scores_mg_5 { TUNE_PP_SCORES_MG_5 };

	int pp_scores_mg_6 { TUNE_PP_SCORES_MG_6 };

	int pp_scores_mg_7 { TUNE_PP_SCORES_MG_7 };

	int pp_scores_eg_0 { TUNE_PP_SCORES_EG_0 };

	int pp_scores_eg_1 { TUNE_PP_SCORES_EG_1 };

	int pp_scores_eg_2 { TUNE_PP_SCORES_EG_2 };

	int pp_scores_eg_3 { TUNE_PP_SCORES_EG_3 };

	int pp_scores_eg_4 { TUNE_PP_SCORES_EG_4 };

	int pp_scores_eg_5 { TUNE_PP_SCORES_EG_5 };

	int pp_scores_eg_6 { TUNE_PP_SCORES_EG_6 };

	int pp_scores_eg_7 { TUNE_PP_SCORES_EG_7 };

	int *pp_scores[2][8] =
	{
		{
			&pp_scores_mg_0,
			&pp_scores_mg_1,
			&pp_scores_mg_2,
			&pp_scores_mg_3,
			&pp_scores_mg_4,
			&pp_scores_mg_5,
			&pp_scores_mg_6,
			&pp_scores_mg_7,
		},
		{
			&pp_scores_eg_0,
			&pp_scores_eg_1,
			&pp_scores_eg_2,
			&pp_scores_eg_3,
			&pp_scores_eg_4,
			&pp_scores_eg_5,
			&pp_scores_eg_6,
			&pp_scores_eg_7,
		}
	};

	eval_par();
	~eval_par();

	std::vector<libchess::TunableParameter> get_tunable_parameters() const;

	void set_eval(const std::string & name, int value);
};

extern eval_par default_parameters;
