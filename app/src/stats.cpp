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
	this->data.n_checkmate     += source.data.n_checkmate;
	this->data.n_stalemate     += source.data.n_stalemate;

        this->data.tt_query   += source.data.tt_query;
        this->data.tt_hit     += source.data.tt_hit;
        this->data.tt_store   += source.data.tt_store;
        this->data.tt_cutoff  += source.data.tt_cutoff;
        this->data.tt_invalid += source.data.tt_invalid;

	this->data.qtt_query  += source.data.qtt_query;
	this->data.qtt_hit    += source.data.qtt_hit;
	this->data.qtt_store  += source.data.qtt_store;
        this->data.qtt_cutoff += source.data.qtt_cutoff;

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

	this->data.large_stack     += source.data.large_stack;
}
