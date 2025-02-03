#pragma once
#include <cstdint>

class chess_stats
{
public:
	struct _data_ {
		uint32_t  nodes;
		uint32_t  qnodes;
		uint32_t  n_standing_pat;
		uint32_t  n_draws;
		uint32_t  n_qs_early_stop;

		uint32_t  alpha_distance;
		uint32_t  beta_distance;
		int       n_alpha_distances;
		int       n_beta_distances;

		uint32_t  tt_query;
		uint32_t  tt_hit;
		uint32_t  tt_store;
		uint32_t  tt_invalid;
		uint32_t  qtt_query;
		uint32_t  qtt_hit;
		uint32_t  qtt_store;

		uint32_t  n_null_move;
		uint32_t  n_null_move_hit;

		uint32_t  n_lmr;
		uint32_t  n_lmr_hit;

		uint32_t  n_static_eval;
		uint32_t  n_static_eval_hit;

		uint64_t  n_moves_cutoff;
		uint64_t  nmc_nodes;
		uint64_t  n_qmoves_cutoff;
		uint64_t  nmc_qnodes;

		uint64_t  syzygy_queries;
		uint64_t  syzygy_query_hits;
	} data;

	chess_stats();
	virtual ~chess_stats();

	void reset();
	void add(const chess_stats & in);
};
