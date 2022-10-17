#pragma once

#include <cmath>
#include <functional>
#include <numeric>
#if defined(linux) || defined(_WIN32)
#include "libchess/Tuner.h"
#else
#include "tuner.h"
#endif

class eval_par
{
public:
	libchess::TunableParameter tune_bishop_count{"tune_bishop_count", 51};
	libchess::TunableParameter tune_too_many_pawns{"tune_too_many_pawns", 6};
	libchess::TunableParameter tune_zero_pawns{"tune_zero_pawns", 7};
	libchess::TunableParameter tune_isolated_pawns{"tune_isolated_pawns", -14};
	libchess::TunableParameter tune_rook_on_open_file{"tune_rook_on_open_file", 41};
//	libchess::TunableParameter tune_mobility{"tune_mobility", 1};
	libchess::TunableParameter tune_double_pawns{"tune_double_pawns", 28};
//	libchess::TunableParameter tune_king_attacks{"tune_king_attacks", 41};
	libchess::TunableParameter tune_bishop_open_diagonal{"tune_bishop_open_diagonal", 41};
	libchess::TunableParameter tune_pawn{"tune_pawn", 128};
	libchess::TunableParameter tune_knight{"tune_knight", 476};
	libchess::TunableParameter tune_bishop{"tune_bishop", 476};
	libchess::TunableParameter tune_rook{"tune_rook", 749};
	libchess::TunableParameter tune_queen{"tune_queen", 1501};
	libchess::TunableParameter tune_big_delta{"tune_big_delta", 1454};
	libchess::TunableParameter tune_big_delta_promotion{"tune_big_delta_promotion", 1060};
//	libchess::TunableParameter tune_king{"tune_king", 10000};
	libchess::TunableParameter tune_pp_scores[2][8]
		{ { { "tune_pp_scores_mg_0", 0 },
		 { "tune_pp_scores_mg_1", -7 },
		 { "tune_pp_scores_mg_2", -11 },
		 { "tune_pp_scores_mg_3", 9 },
		 { "tune_pp_scores_mg_4", 35 },
		 { "tune_pp_scores_mg_5", 69 },
		 { "tune_pp_scores_mg_6", 163 },
		 { "tune_pp_scores_mg_7", 0 } },

		{ { "tune_pp_scores_eg_0", 0 },
		 { "tune_pp_scores_eg_1", -579 },
		 { "tune_pp_scores_eg_2", 600 },
		 { "tune_pp_scores_eg_3", -434 },
		 { "tune_pp_scores_eg_4", -164 },
		 { "tune_pp_scores_eg_5", -115 },
		 { "tune_pp_scores_eg_6", -65 },
		 { "tune_pp_scores_eg_7", 0 } } };
//	libchess::TunableParameter tune_find_forks{"tune_find_forks", 0};
	libchess::TunableParameter tune_king_shield{"tune_king_shield", 5};
//	libchess::TunableParameter tune_development{"tune_development", 0};
	libchess::TunableParameter tune_psq_mul{"tune_psq_mul", 588};
	libchess::TunableParameter tune_psq_div{"tune_psq_div", 764};
//	libchess::TunableParameter tune_king_knight_distance{"tune_king_knight_distance", 20};
//	libchess::TunableParameter tune_king_RQ_distance{"tune_king_RQ_distance", 30};
	libchess::TunableParameter tune_edge_black_rank{"tune_edge_black_rank", 0};
	libchess::TunableParameter tune_edge_black_file{"tune_edge_black_file", 3};
	libchess::TunableParameter tune_edge_white_rank{"tune_edge_white_rank", 1};
	libchess::TunableParameter tune_edge_white_file{"tune_edge_white_file", 2};

	eval_par();
	~eval_par();

	std::vector<libchess::TunableParameter> get_tunable_parameters() const;

	void set_eval(const std::string & name, int value);
};

extern eval_par default_parameters;

