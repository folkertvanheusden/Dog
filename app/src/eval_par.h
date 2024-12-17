#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <numeric>
#include <string>

#include "my_tuner.h"
#include "tune.h"


class eval_par
{
public:
	int bishop_count { TUNE_BISHOP_COUNT };

	int too_many_pawns { TUNE_TOO_MANY_PAWNS };
	int zero_pawns { TUNE_ZERO_PAWNS };
	int isolated_pawns { TUNE_ISOLATED_PAWNS };
	int double_pawns { TUNE_DOUBLE_PAWNS };

	int rook_on_open_file { TUNE_ROOK_ON_OPEN_FILE };
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
	eval_par(const std::vector<libchess::TunableParameter> & params);
	~eval_par();

	std::map<std::string, int *> m {
		{ "tune_bishop_count", &bishop_count },
		{ "tune_too_many_pawns", &too_many_pawns },
		{ "tune_zero_pawns", &zero_pawns },
		{ "tune_isolated_pawns", &isolated_pawns },
		{ "tune_rook_on_open_file", &rook_on_open_file },
		{ "tune_double_pawns", &double_pawns },
		{ "tune_bishop_open_diagonal", &bishop_open_diagonal },
		{ "tune_pawn", &pawn },
		{ "tune_knight", &knight },
		{ "tune_bishop", &bishop },
		{ "tune_rook", &rook },
		{ "tune_queen", &queen },
		{ "tune_king_shield", &king_shield },
		{ "tune_psq_mul", &psq_mul },
		{ "tune_psq_div", &psq_div },
		{ "tune_edge_black_rank", &edge_black_rank },
		{ "tune_edge_black_file", &edge_black_file },
		{ "tune_edge_white_rank", &edge_white_rank },
		{ "tune_edge_white_file", &edge_white_file },
		{ "tune_big_delta_promotion", &big_delta_promotion },
		{ "tune_big_delta", &big_delta },
#if 0
		{ "tune_mobility", &mobility },
		{ "tune_mobil_pawn", &mobil_pawn },
		{ "tune_mobil_knight", &mobil_knight },
		{ "tune_mobil_bishop", &mobil_bishop },
		{ "tune_mobil_rook", &mobil_rook },
		{ "tune_mobil_queen", &mobil_queen },
		{ "tune_mobil_king", &mobil_king },
#endif
		{ "tune_pp_scores_mg_0", &pp_scores_mg_0 },
		{ "tune_pp_scores_mg_1", &pp_scores_mg_1 },
		{ "tune_pp_scores_mg_2", &pp_scores_mg_2 },
		{ "tune_pp_scores_mg_3", &pp_scores_mg_3 },
		{ "tune_pp_scores_mg_4", &pp_scores_mg_4 },
		{ "tune_pp_scores_mg_5", &pp_scores_mg_5 },
		{ "tune_pp_scores_mg_6", &pp_scores_mg_6 },
		{ "tune_pp_scores_mg_7", &pp_scores_mg_7 },
		{ "tune_pp_scores_eg_0", &pp_scores_eg_0 },
		{ "tune_pp_scores_eg_1", &pp_scores_eg_1 },
		{ "tune_pp_scores_eg_2", &pp_scores_eg_2 },
		{ "tune_pp_scores_eg_3", &pp_scores_eg_3 },
		{ "tune_pp_scores_eg_4", &pp_scores_eg_4 },
		{ "tune_pp_scores_eg_5", &pp_scores_eg_5 },
		{ "tune_pp_scores_eg_6", &pp_scores_eg_6 },
		{ "tune_pp_scores_eg_7", &pp_scores_eg_7 },
		{ "tune_pp_scores_mg_0", &pp_scores_mg_0 },
		{ "tune_pp_scores_mg_1", &pp_scores_mg_1 },
		{ "tune_pp_scores_mg_2", &pp_scores_mg_2 },
		{ "tune_pp_scores_mg_3", &pp_scores_mg_3 },
		{ "tune_pp_scores_mg_4", &pp_scores_mg_4 },
		{ "tune_pp_scores_mg_5", &pp_scores_mg_5 },
		{ "tune_pp_scores_mg_6", &pp_scores_mg_6 },
		{ "tune_pp_scores_mg_7", &pp_scores_mg_7 },
		{ "tune_pp_scores_eg_0", &pp_scores_eg_0 },
		{ "tune_pp_scores_eg_1", &pp_scores_eg_1 },
		{ "tune_pp_scores_eg_2", &pp_scores_eg_2 },
		{ "tune_pp_scores_eg_3", &pp_scores_eg_3 },
		{ "tune_pp_scores_eg_4", &pp_scores_eg_4 },
		{ "tune_pp_scores_eg_5", &pp_scores_eg_5 },
		{ "tune_pp_scores_eg_6", &pp_scores_eg_6 },
		{ "tune_pp_scores_eg_7", &pp_scores_eg_7 },
	};

	std::vector<libchess::TunableParameter> get_tunable_parameters() const;

	void set_eval(const std::string & name, int value);
};

extern eval_par default_parameters;
