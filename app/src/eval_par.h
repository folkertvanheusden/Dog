#pragma once

#include <cmath>
#include <functional>
#include <numeric>

#ifdef __ANDROID__
#include "libchess/Tuner.h"
#elif defined(linux) || defined(_WIN32)
#include "libchess/Tuner.h"
#else
#include "tuner.h"
#endif

#include "tune.h"

class eval_par
{
public:
	libchess::TunableParameter tune_bishop_count{"tune_bishop_count", TUNE_BISHOP_COUNT };
	libchess::TunableParameter tune_too_many_pawns{"tune_too_many_pawns", TUNE_TOO_MANY_PAWNS };
	libchess::TunableParameter tune_zero_pawns{"tune_zero_pawns", TUNE_ZERO_PAWNS };
	libchess::TunableParameter tune_isolated_pawns{"tune_isolated_pawns", TUNE_ISOLATED_PAWNS };
	libchess::TunableParameter tune_rook_on_open_file{"tune_rook_on_open_file", TUNE_ROOK_ON_OPEN_FILE };
//	libchess::TunableParameter tune_mobility{"tune_mobility", TUNE_MOBILITY };
	libchess::TunableParameter tune_double_pawns{"tune_double_pawns", TUNE_DOUBLE_PAWNS };
//	libchess::TunableParameter tune_king_attacks{"tune_king_attacks", TUNE_KING_ATTACKS };
	libchess::TunableParameter tune_bishop_open_diagonal{"tune_bishop_open_diagonal", TUNE_BISHOP_OPEN_DIAGONAL };
	libchess::TunableParameter tune_pawn{"tune_pawn", TUNE_PAWN };
	libchess::TunableParameter tune_knight{"tune_knight", TUNE_KNIGHT };
	libchess::TunableParameter tune_bishop{"tune_bishop", TUNE_BISHOP };
	libchess::TunableParameter tune_rook{"tune_rook", TUNE_ROOK };
	libchess::TunableParameter tune_queen{"tune_queen", TUNE_QUEEN };
	libchess::TunableParameter tune_big_delta{"tune_big_delta", TUNE_BIG_DELTA };
	libchess::TunableParameter tune_big_delta_promotion{"tune_big_delta_promotion", TUNE_BIG_DELTA_PROMOTION };
	libchess::TunableParameter tune_pp_scores[2][8]
		{ { { "tune_pp_scores_mg_0", TUNE_PP_SCORES_MG_0 },
		 { "tune_pp_scores_mg_1", TUNE_PP_SCORES_MG_1 },
		 { "tune_pp_scores_mg_2", TUNE_PP_SCORES_MG_2 },
		 { "tune_pp_scores_mg_3", TUNE_PP_SCORES_MG_3 },
		 { "tune_pp_scores_mg_4", TUNE_PP_SCORES_MG_4 },
		 { "tune_pp_scores_mg_5", TUNE_PP_SCORES_MG_5 },
		 { "tune_pp_scores_mg_6", TUNE_PP_SCORES_MG_6 },
		 { "tune_pp_scores_mg_7", TUNE_PP_SCORES_MG_7 } },

		{ { "tune_pp_scores_eg_0", TUNE_PP_SCORES_EG_0 },
		 { "tune_pp_scores_eg_1", TUNE_PP_SCORES_EG_1 },
		 { "tune_pp_scores_eg_2", TUNE_PP_SCORES_EG_2 },
		 { "tune_pp_scores_eg_3", TUNE_PP_SCORES_EG_3 },
		 { "tune_pp_scores_eg_4", TUNE_PP_SCORES_EG_4 },
		 { "tune_pp_scores_eg_5", TUNE_PP_SCORES_EG_5 },
		 { "tune_pp_scores_eg_6", TUNE_PP_SCORES_EG_6 },
		 { "tune_pp_scores_eg_7", TUNE_PP_SCORES_EG_7 } } };
//	libchess::TunableParameter tune_find_forks{"tune_find_forks", TUNE_FIND_FORKS };
	libchess::TunableParameter tune_king_shield{"tune_king_shield", TUNE_KING_SHIELD };
//	libchess::TunableParameter tune_development{"tune_development", TUNE_DEVELOPMENT };
	libchess::TunableParameter tune_psq_mul{"tune_psq_mul", TUNE_PSQ_MUL };
	libchess::TunableParameter tune_psq_div{"tune_psq_div", TUNE_PSQ_DIV };
	libchess::TunableParameter tune_edge_black_rank{"tune_edge_black_rank", TUNE_EDGE_BLACK_RANK };
	libchess::TunableParameter tune_edge_black_file{"tune_edge_black_file", TUNE_EDGE_BLACK_FILE };
	libchess::TunableParameter tune_edge_white_rank{"tune_edge_white_rank", TUNE_EDGE_WHITE_RANK };
	libchess::TunableParameter tune_edge_white_file{"tune_edge_white_file", TUNE_EDGE_WHITE_FILE };

	eval_par();
	~eval_par();

	std::vector<libchess::TunableParameter> get_tunable_parameters() const;

	void set_eval(const std::string & name, int value);
};

extern eval_par default_parameters;
