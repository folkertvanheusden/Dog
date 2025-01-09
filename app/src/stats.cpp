#include <cstring>

#include "stats.h"


chess_stats::chess_stats()
{
	reset();
}

chess_stats::~chess_stats()
{
}

void chess_stats::reset()
{
	memset(&data, 0x00, sizeof data);
}

void chess_stats::add(const chess_stats & source)
{
	this->data.nodes           += source.data.nodes;
	this->data.qnodes          += source.data.qnodes;
	this->data.n_draws         += source.data.n_draws;
	this->data.n_standing_pat  += source.data.n_standing_pat;
	this->data.n_qs_early_stop += source.data.n_qs_early_stop;

        this->data.tt_query += source.data.tt_query;
        this->data.tt_hit   += source.data.tt_hit;
        this->data.tt_store += source.data.tt_store;

	this->data.n_null_move     += source.data.n_null_move;
	this->data.n_null_move_hit += source.data.n_null_move_hit;

	this->data.n_lmr     += source.data.n_lmr;
	this->data.n_lmr_hit += source.data.n_lmr_hit;

	this->data.n_static_eval     += source.data.n_static_eval;
	this->data.n_static_eval_hit += source.data.n_static_eval_hit;

	this->data.n_moves_cutoff  += source.data.n_moves_cutoff;
	this->data.nmc_nodes       += source.data.nmc_nodes;
	this->data.n_qmoves_cutoff += source.data.n_qmoves_cutoff;
	this->data.nmc_qnodes      += source.data.nmc_qnodes;
}
