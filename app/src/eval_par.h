#pragma once

#include <cmath>
#include <functional>
#include <numeric>

#include "tuner.h"

#include "tune.h"

class eval_par
{
public:
	int bishop_count { TUNE_BISHOP_COUNT };
	libchess::TunableParameterP tune_bishop_count{ "tune_bishop_count", &bishop_count };

	int too_many_pawns { TUNE_TOO_MANY_PAWNS };
	libchess::TunableParameterP tune_too_many_pawns{ "tune_too_many_pawns", &too_many_pawns };

	int zero_pawns { TUNE_ZERO_PAWNS };
	libchess::TunableParameterP tune_zero_pawns{ "tune_zero_pawns", &zero_pawns };

	int isolated_pawns { TUNE_ISOLATED_PAWNS };
	libchess::TunableParameterP tune_isolated_pawns{ "tune_isolated_pawns", &isolated_pawns };

	int rook_on_open_file { TUNE_ROOK_ON_OPEN_FILE };
	libchess::TunableParameterP tune_rook_on_open_file{ "tune_rook_on_open_file", &rook_on_open_file };

	int double_pawns { TUNE_DOUBLE_PAWNS };
	libchess::TunableParameterP tune_double_pawns{ "tune_double_pawns", &double_pawns };

	int bishop_open_diagonal { TUNE_BISHOP_OPEN_DIAGONAL };
	libchess::TunableParameterP tune_bishop_open_diagonal{ "tune_bishop_open_diagonal", &bishop_open_diagonal };

	int pawn { TUNE_PAWN };
	libchess::TunableParameterP tune_pawn{ "tune_pawn", &pawn };

	int knight { TUNE_KNIGHT };
	libchess::TunableParameterP tune_knight{ "tune_knight", &knight };

	int bishop { TUNE_BISHOP };
	libchess::TunableParameterP tune_bishop{ "tune_bishop", &bishop };

	int rook { TUNE_ROOK };
	libchess::TunableParameterP tune_rook{ "tune_rook", &rook };

	int queen { TUNE_QUEEN };
	libchess::TunableParameterP tune_queen{ "tune_queen", &queen };

	int king_shield { TUNE_KING_SHIELD };
	libchess::TunableParameterP tune_king_shield{ "tune_king_shield", &king_shield };

	int psq_mul { TUNE_PSQ_MUL };
	libchess::TunableParameterP tune_psq_mul{ "tune_psq_mul", &psq_mul };

	int psq_div { TUNE_PSQ_DIV };
	libchess::TunableParameterP tune_psq_div{ "tune_psq_div", &psq_div };

	int edge_black_rank { TUNE_EDGE_BLACK_RANK };
	libchess::TunableParameterP tune_edge_black_rank{ "tune_edge_black_rank", &edge_black_rank };

	int edge_black_file { TUNE_EDGE_BLACK_FILE };
	libchess::TunableParameterP tune_edge_black_file{ "tune_edge_black_file", &edge_black_file };

	int edge_white_rank { TUNE_EDGE_WHITE_RANK };
	libchess::TunableParameterP tune_edge_white_rank{ "tune_edge_white_rank", &edge_white_rank };

	int edge_white_file { TUNE_EDGE_WHITE_FILE };
	libchess::TunableParameterP tune_edge_white_file{ "tune_edge_white_file", &edge_white_file };

	int big_delta_promotion { TUNE_BIG_DELTA_PROMOTION };
	libchess::TunableParameterP tune_big_delta_promotion{ "tune_big_delta_promotion", &big_delta_promotion };

	int big_delta { TUNE_BIG_DELTA };
	libchess::TunableParameterP tune_big_delta{ "tune_big_delta", &big_delta };

#if 0
	int mobility { TUNE_MOBILITY };
	libchess::TunableParameterP tune_mobility{ "tune_mobility", mobility };

	int mobil_pawn { TUNE_MOBIL_PAWN };
	libchess::TunableParameterP tune_mobil_pawn{ "tune_mobil_pawn", mobil_pawn };

	int mobil_knight { TUNE_MOBIL_KNIGHT };
	libchess::TunableParameterP tune_mobil_knight{ "tune_mobil_knight", mobil_knight };

	int mobil_bishop { TUNE_MOBIL_BISHOP };
	libchess::TunableParameterP tune_mobil_bishop{ "tune_mobil_bishop", mobil_bishop };

	int mobil_rook { TUNE_MOBIL_ROOK };
	libchess::TunableParameterP tune_mobil_rook{ "tune_mobil_rook", mobil_rook };

	int mobil_queen { TUNE_MOBIL_QUEEN };
	libchess::TunableParameterP tune_mobil_queen{ "tune_mobil_queen", mobil_queen };

	int mobil_king { TUNE_MOBIL_KING };
	libchess::TunableParameterP tune_mobil_king{ "tune_mobil_king", mobil_king };
#endif

	int pp_scores_mg_0 { TUNE_PP_SCORES_MG_0 };
	libchess::TunableParameterP tune_pp_scores_mg_0{ "tune_pp_scores_mg_0", &pp_scores_mg_0 };

	int pp_scores_mg_1 { TUNE_PP_SCORES_MG_1 };
	libchess::TunableParameterP tune_pp_scores_mg_1{ "tune_pp_scores_mg_1", &pp_scores_mg_1 };

	int pp_scores_mg_2 { TUNE_PP_SCORES_MG_2 };
	libchess::TunableParameterP tune_pp_scores_mg_2{ "tune_pp_scores_mg_2", &pp_scores_mg_2 };

	int pp_scores_mg_3 { TUNE_PP_SCORES_MG_3 };
	libchess::TunableParameterP tune_pp_scores_mg_3{ "tune_pp_scores_mg_3", &pp_scores_mg_3 };

	int pp_scores_mg_4 { TUNE_PP_SCORES_MG_4 };
	libchess::TunableParameterP tune_pp_scores_mg_4{ "tune_pp_scores_mg_4", &pp_scores_mg_4 };

	int pp_scores_mg_5 { TUNE_PP_SCORES_MG_5 };
	libchess::TunableParameterP tune_pp_scores_mg_5{ "tune_pp_scores_mg_5", &pp_scores_mg_5 };

	int pp_scores_mg_6 { TUNE_PP_SCORES_MG_6 };
	libchess::TunableParameterP tune_pp_scores_mg_6{ "tune_pp_scores_mg_6", &pp_scores_mg_6 };

	int pp_scores_mg_7 { TUNE_PP_SCORES_MG_7 };
	libchess::TunableParameterP tune_pp_scores_mg_7{ "tune_pp_scores_mg_7", &pp_scores_mg_7 };

	int pp_scores_eg_0 { TUNE_PP_SCORES_EG_0 };
	libchess::TunableParameterP tune_pp_scores_eg_0{ "tune_pp_scores_eg_0", &pp_scores_eg_0 };

	int pp_scores_eg_1 { TUNE_PP_SCORES_EG_1 };
	libchess::TunableParameterP tune_pp_scores_eg_1{ "tune_pp_scores_eg_1", &pp_scores_eg_1 };

	int pp_scores_eg_2 { TUNE_PP_SCORES_EG_2 };
	libchess::TunableParameterP tune_pp_scores_eg_2{ "tune_pp_scores_eg_2", &pp_scores_eg_2 };

	int pp_scores_eg_3 { TUNE_PP_SCORES_EG_3 };
	libchess::TunableParameterP tune_pp_scores_eg_3{ "tune_pp_scores_eg_3", &pp_scores_eg_3 };

	int pp_scores_eg_4 { TUNE_PP_SCORES_EG_4 };
	libchess::TunableParameterP tune_pp_scores_eg_4{ "tune_pp_scores_eg_4", &pp_scores_eg_4 };

	int pp_scores_eg_5 { TUNE_PP_SCORES_EG_5 };
	libchess::TunableParameterP tune_pp_scores_eg_5{ "tune_pp_scores_eg_5", &pp_scores_eg_5 };

	int pp_scores_eg_6 { TUNE_PP_SCORES_EG_6 };
	libchess::TunableParameterP tune_pp_scores_eg_6{ "tune_pp_scores_eg_6", &pp_scores_eg_6 };

	int pp_scores_eg_7 { TUNE_PP_SCORES_EG_7 };
	libchess::TunableParameterP tune_pp_scores_eg_7{ "tune_pp_scores_eg_7", &pp_scores_eg_7 };

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

	libchess::TunableParameterP tune_pp_scores[2][8]
	      { { { "tune_pp_scores_mg_0", &pp_scores_mg_0 },
		  { "tune_pp_scores_mg_1", &pp_scores_mg_1 },
		  { "tune_pp_scores_mg_2", &pp_scores_mg_2 },
		  { "tune_pp_scores_mg_3", &pp_scores_mg_3 },
		  { "tune_pp_scores_mg_4", &pp_scores_mg_4 },
		  { "tune_pp_scores_mg_5", &pp_scores_mg_5 },
		  { "tune_pp_scores_mg_6", &pp_scores_mg_6 },
		  { "tune_pp_scores_mg_7", &pp_scores_mg_7 } },

		{ { "tune_pp_scores_eg_0", &pp_scores_eg_0 },
		  { "tune_pp_scores_eg_1", &pp_scores_eg_1 },
		  { "tune_pp_scores_eg_2", &pp_scores_eg_2 },
		  { "tune_pp_scores_eg_3", &pp_scores_eg_3 },
		  { "tune_pp_scores_eg_4", &pp_scores_eg_4 },
		  { "tune_pp_scores_eg_5", &pp_scores_eg_5 },
		  { "tune_pp_scores_eg_6", &pp_scores_eg_6 },
		  { "tune_pp_scores_eg_7", &pp_scores_eg_7 } } };

	eval_par();
	~eval_par();

	std::vector<libchess::TunableParameterP> get_tunable_parameters() const;

	void set_eval(const std::string & name, int value);
};

extern eval_par default_parameters;
