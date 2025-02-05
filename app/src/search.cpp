#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
#include <limits.h>
#include <sys/time.h>
#endif
#include <cinttypes>
#include <cmath>
#include <libchess/Position.h>
#include <libchess/UCIService.h>

#include "eval.h"
#include "inbuf.h"
#include "main.h"
#include "max-ascii.h"
#include "psq.h"
#include "search.h"
#include "str.h"
#if defined(linux) || defined(_WIN32) || defined(__APPLE__)
#include "syzygy.h"
#endif
#include "test.h"
#include "tt.h"
#include "tui.h"


#if !defined(ESP32)
#define N_LMR_DEPTH 64
#define N_LMR_MOVES 64
static uint8_t lmr_reductions[N_LMR_DEPTH][N_LMR_MOVES];
#endif
void init_lmr()
{
#if !defined(ESP32)
for(int depth=1; depth<N_LMR_DEPTH; depth++) {
	for(int n_played=0; n_played<N_LMR_MOVES; n_played++) {
		constexpr double lmr_mul  = 0.5;
		constexpr double lmr_base = 1.0;
		lmr_reductions[depth][n_played] = log(depth) * log(n_played + 1) * lmr_mul + lmr_base;
	}
}
#endif
}

inline int history_index(const libchess::Color & side, const libchess::PieceType & from_type, const libchess::Square & sq)
{
	return side * 6 * 64 + from_type * 64 + sq;
}

sort_movelist_compare::sort_movelist_compare(const search_pars_t & sp) :
	sp(sp)
{
	if (sp.pos.previous_move())
		previous_move_target = sp.pos.previous_move()->to_square();
}

void sort_movelist_compare::add_first_move(const libchess::Move move)
{
	assert(move.value());
	first_moves.push_back(move);
}

// MVV-LVA
int sort_movelist_compare::move_evaluater(const libchess::Move move) const
{
	for(size_t i=0; i<first_moves.size(); i++) {
		if (move == first_moves.at(i))
			return INT_MAX - i;
	}

	int  score      = 0;
	auto piece_from = sp.pos.piece_on(move.from_square());
	auto from_type  = piece_from->type();
	auto to_type    = from_type;

	if (sp.pos.is_promotion_move(move)) {
		to_type = *move.promotion_piece_type();

		int piece_val = to_type;
		assert(piece_val < 2048);
		score  += piece_val << 19;
	}

	if (sp.pos.is_capture_move(move)) {
		if (move.type() == libchess::Move::Type::ENPASSANT) {
			score += libchess::constants::PAWN << 19;
		}
		else {
			auto piece_to = sp.pos.piece_on(move.to_square());

			// victim
			int victim_val = piece_to->type();
			assert(victim_val < 2048);
			score += victim_val << 19;
		}

		if (from_type != libchess::constants::KING) {
			int add = (libchess::constants::QUEEN - from_type) * 256;
			assert(abs(add) < (1 << 19));
			score += add;
		}
	}
	else {
		int index = history_index(sp.pos.side_to_move(), from_type, move.to_square());
		score += sp.history[index];
	}

	return score;
}

void sort_movelist(libchess::MoveList & move_list, sort_movelist_compare & smc)
{
	move_list.sort([&smc](const libchess::Move move) { return smc.move_evaluater(move); });
}

bool is_check(libchess::Position & pos)
{
	return pos.attackers_to(pos.piece_type_bb(libchess::constants::KING, !pos.side_to_move()).forward_bitscan(), pos.side_to_move());
}

// https://www.reddit.com/r/chess/comments/se89db/a_writeup_on_definitions_of_insufficient_material/
bool is_insufficient_material_draw(const libchess::Position & pos)
{
	using namespace libchess::constants;

        // A king + any(pawn, rook, queen) is sufficient.
	if (pos.piece_type_bb(PAWN) || pos.piece_type_bb(QUEEN) || pos.piece_type_bb(ROOK))
		return false;

        // A king and more than one other type of piece is sufficient (e.g. knight + bishop).
	if ((pos.piece_type_bb(KNIGHT, WHITE) && pos.piece_type_bb(BISHOP, WHITE)) ||
	    (pos.piece_type_bb(KNIGHT, BLACK) && pos.piece_type_bb(BISHOP, BLACK)))
		return false;

        // A king and two (or more) knights is sufficient
	if (pos.piece_type_bb(KNIGHT, WHITE).popcount() >= 2 ||
	    pos.piece_type_bb(KNIGHT, BLACK).popcount() >= 2)
		return false;

        // King + bishop against king + any(knight, pawn) is sufficient.
        if ((pos.piece_type_bb(BISHOP, WHITE) && (pos.piece_type_bb(KNIGHT, BLACK) || pos.piece_type_bb(PAWN, BLACK))) ||
            (pos.piece_type_bb(BISHOP, BLACK) && (pos.piece_type_bb(KNIGHT, WHITE) || pos.piece_type_bb(PAWN, WHITE)))) {
                return false;
        }

        // King + knight against king + any(rook, bishop, knight, pawn) is sufficient.
        if (((pos.piece_type_bb(ROOK, WHITE) || pos.piece_type_bb(BISHOP, WHITE) || pos.piece_type_bb(KNIGHT, WHITE) || pos.piece_type_bb(PAWN, WHITE))
				&& pos.piece_type_bb(KNIGHT, BLACK)) ||
            ((pos.piece_type_bb(ROOK, BLACK) || pos.piece_type_bb(BISHOP, BLACK) || pos.piece_type_bb(KNIGHT, BLACK) || pos.piece_type_bb(PAWN, BLACK))
	    			&& pos.piece_type_bb(KNIGHT, WHITE)))
		return false;

        // King + bishop(s) is also sufficient if there's bishops on opposite colours (even king + bishop against king + bishop).
        constexpr uint64_t white_squares = 0x55aa55aa55aa55aall;
        constexpr uint64_t black_squares = 0xaa55aa55aa55aa55ll;
        const libchess::Bitboard piece_bb = pos.piece_type_bb(BISHOP);
        if ((piece_bb & black_squares) && (piece_bb & white_squares)) {
                return false;
        }

        return true;
}

libchess::MoveList gen_qs_moves(libchess::Position & pos)
{
	libchess::Color side = pos.side_to_move();

	if (pos.checkers_to(side))
		return pos.pseudo_legal_move_list();

	libchess::MoveList ml;
	pos.generate_promotions(ml, side);
	pos.generate_capture_moves(ml, side);

	return ml;
}

int qs(int alpha, const int beta, const int qsdepth, search_pars_t & sp)
{
	if (sp.stop->flag)
		return 0;
#if defined(ESP32)
	if (qsdepth > sp.md) {
		if (check_min_stack_size(1, sp))
			return 0;

		sp.md = qsdepth;
	}
#endif
	if (qsdepth >= 127)
		return nnue_evaluate(sp.nnue_eval, sp.pos);

	sp.cs.data.qnodes++;

	if (sp.pos.halfmoves() >= 100 || sp.pos.is_repeat() || is_insufficient_material_draw(sp.pos))
		return 0;

	// TT //
	int            start_alpha = alpha;
	std::optional<libchess::Move> tt_move { };
	uint64_t       hash        = sp.pos.hash();
	std::optional<tt_entry> te = tti.lookup(hash);
	sp.cs.data.qtt_query++;

        if (te.has_value()) {  // TT hit?
		sp.cs.data.qtt_hit++;
		if (te.value().m)  // move stored in TT?
			tt_move = libchess::Move(te.value().m);

		{
			int score      = te.value().score;
			int work_score = eval_from_tt(score, qsdepth);
			auto flag      = te.value().flags;
                        bool use       = flag == EXACT ||
                                        (flag == LOWERBOUND && work_score >= beta) ||
                                        (flag == UPPERBOUND && work_score <= alpha);
			if (use)
				return work_score;
		}
	}
	////////

	int  best_score = -32767;

	bool in_check   = sp.pos.in_check();
	if (!in_check) {
		// standing pat
		best_score = nnue_evaluate(sp.nnue_eval, sp.pos);
		if (best_score > alpha && best_score >= beta) {
			sp.cs.data.n_standing_pat++;
			return best_score;
		}

		if (alpha < best_score)
			alpha = best_score;
	}

	int  n_played  = 0;
	auto move_list = gen_qs_moves(sp.pos);
	std::optional<libchess::Move> m;

	sort_movelist_compare smc(sp);
	if (tt_move.has_value())
		smc.add_first_move(tt_move.value());
	sort_movelist(move_list, smc);

	for(auto move : move_list) {
		if (sp.pos.is_legal_generated_move(move) == false)
			continue;

		if (!in_check && sp.pos.is_capture_move(move)) {
			auto piece_to    = sp.pos.piece_on(move.to_square());
			int  eval_target = move.type() == libchess::Move::Type::ENPASSANT ? libchess::constants::PAWN : piece_to->type();
			auto piece_from  = sp.pos.piece_on(move.from_square());
			int  eval_killer = piece_from->type();
			if (eval_killer > eval_target && sp.pos.attackers_to(move.to_square(), !sp.pos.side_to_move()))
				continue;
		}

		n_played++;

		auto undo_actions = make_move(sp.nnue_eval, sp.pos, move);
		int score = -qs(-beta, -alpha, qsdepth + 1, sp);
		unmake_move(sp.nnue_eval, sp.pos, undo_actions);

		if (score > best_score) {
			best_score = score;
			m          = move;

			if (score > alpha) {
				if (score >= beta) {
					sp.cs.data.n_qmoves_cutoff += n_played;
					sp.cs.data.nmc_qnodes++;
					break;
				}

				alpha = score;
			}
		}

		if (n_played >= 3 && best_score >= 9800)
			break;
	}

	if (n_played == 0) {
		if (in_check)
			best_score = -10000 + qsdepth;
		else if (best_score == -32767)
			best_score = nnue_evaluate(sp.nnue_eval, sp.pos);
	}

	assert(best_score >= -10000);
	assert(best_score <=  10000);

	if (sp.stop->flag == false && (te.has_value() == false || te.value().depth == 0)) {
		sp.cs.data.qtt_store++;

		tt_entry_flag flag = EXACT;
		if (best_score <= start_alpha)
			flag = UPPERBOUND;
		else if (best_score >= beta)
			flag = LOWERBOUND;

		int work_score = eval_to_tt(best_score, qsdepth);

		if (best_score > start_alpha && m.has_value())
			tti.store(hash, flag, 0, work_score, m.value());
		else if (tt_move.has_value())
			tti.store(hash, flag, 0, work_score, tt_move.value());
		else
			tti.store(hash, flag, 0, work_score);
	}

	return best_score;
}

void update_history(const search_pars_t & sp, const int index, const int bonus)
{
	constexpr const int max_history = 1023;
	constexpr const int min_history = -max_history;
	int  clamped_bonus = std::clamp(bonus, min_history, max_history);
	int  final_value   = clamped_bonus - sp.history[index] * abs(clamped_bonus) / max_history;

	assert(sp.history[index] + final_value <=  32767);  // sp.history is 16 bit
	assert(sp.history[index] + final_value >= -32768);

	sp.history[index] += final_value;
}

int search(int depth, int16_t alpha, const int16_t beta, const int null_move_depth, const int16_t max_depth, libchess::Move *const m, search_pars_t & sp)
{
	if (sp.stop->flag)
		return 0;

	if (depth == 0)
		return qs(alpha, beta, max_depth, sp);

	const int csd = max_depth - depth;
#if defined(ESP32)
	if (csd > sp.md) {
		if (check_min_stack_size(0, sp))
			return 0;
		sp.md = csd;
	}
#endif

	sp.cs.data.nodes++;

	bool is_root_position = max_depth == depth;
	if (!is_root_position && (sp.pos.is_repeat() || is_insufficient_material_draw(sp.pos))) {
		sp.cs.data.n_draws++;
		return 0;
	}

	const int  start_alpha = alpha;
	const bool is_pv       = alpha != beta -1;

	// TT //
	std::optional<libchess::Move> tt_move { };
	uint64_t       hash        = sp.pos.hash();
	std::optional<tt_entry> te = tti.lookup(hash);
	sp.cs.data.tt_query++;

        if (te.has_value()) {  // TT hit?
		sp.cs.data.tt_hit++;
		if (te.value().m)  // move stored in TT?
			tt_move = libchess::Move(te.value().m);

		if (te.value().depth >= depth && !is_pv) {
			int score      = te.value().score;
			int work_score = eval_from_tt(score, csd);
			auto flag      = te.value().flags;
                        bool use       = flag == EXACT ||
                                        (flag == LOWERBOUND && work_score >= beta) ||
                                        (flag == UPPERBOUND && work_score <= alpha);

			if (use) {
				if (tt_move.has_value()) {
					if (is_root_position) {
						if (sp.pos.is_legal_move(tt_move.value())) {
							*m = tt_move.value();  // move in TT is valid
							return work_score;
						}

						sp.cs.data.tt_invalid++; // move stored in TT is not valid - TT-collision
						// do NOT return
					}
					else {
						*m = tt_move.value();  // not used directly, only for move ordening
						return work_score;
					}
				}
				else if (!is_root_position) {
					return work_score;
				}
			}
		}
	}
	else if (is_pv && depth >= 4) {  // IIR, Internal Iterative Reductions
		depth--;
	}
	////////

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
	if (with_syzygy && !is_root_position) {
		// check piece count
		unsigned counts = sp.pos.occupancy_bb().popcount();

		// syzygy count?
		if (counts <= TB_LARGEST) {
			sp.cs.data.syzygy_queries++;
			std::optional<int> syzygy_score = probe_fathom_nonroot(sp.pos);

			if (syzygy_score.has_value()) {
				sp.cs.data.syzygy_query_hits++;
				sp.cs.data.tt_store++;

				int score      = syzygy_score.value();
				int work_score = eval_to_tt(score, csd);
				tti.store(hash, EXACT, depth, work_score, libchess::Move(0));
				return score;
			}
		}
	}
#endif
	bool in_check = sp.pos.in_check();

	if (!is_root_position && !in_check && depth <= 7 && beta <= 9800) {
		sp.cs.data.n_static_eval++;
		int staticeval = nnue_evaluate(sp.nnue_eval, sp.pos);

		// static null pruning (reverse futility pruning)
		if (staticeval - depth * 121 > beta) {
			sp.cs.data.n_static_eval_hit++;
			return (beta + staticeval) / 2;
		}
	}

	///// null move
	int nm_reduce_depth = depth > 6 ? 4 : 3;
	if (depth >= 2 && !in_check && !is_root_position && null_move_depth < 2) {
		sp.cs.data.n_null_move++;

		sp.pos.make_null_move();
		libchess::Move ignore { };
		int nmscore = -search(std::max(0, depth - nm_reduce_depth), -beta, -beta + 1, null_move_depth + 1, max_depth, &ignore, sp);
		sp.pos.unmake_move();

                if (nmscore >= beta) {
			libchess::Move ignore2 { };
			int verification = search(std::max(0, depth - nm_reduce_depth), beta - 1, beta, null_move_depth, max_depth, &ignore2, sp);
			if (verification >= beta) {
				sp.cs.data.n_null_move_hit++;
				return abs(nmscore) >= 9800 ? beta : nmscore;
			}
                }
	}
	///////////////
	
	int                best_score = -32767;
	libchess::MoveList move_list  = sp.pos.pseudo_legal_move_list();

	sort_movelist_compare smc(sp);

	if (tt_move.has_value())
		smc.add_first_move(tt_move.value());
	if (m->value() && sp.pos.is_capture_move(*m))
		smc.add_first_move(*m);

	sort_movelist(move_list, smc);

	int     n_played   = 0;
	int     lmr_start  = !in_check && depth >= 2 ? 4 : 999;

	std::optional<libchess::Move> beta_cutoff_move;
	libchess::Move new_move { 0 };
	for(auto move : move_list) {
		if (sp.pos.is_legal_generated_move(move) == false)
			continue;

		sp.cur_move = move.value();

                bool is_lmr = false;
                int  score  = -10000;

		auto undo_actions = make_move(sp.nnue_eval, sp.pos, move);
                if (n_played == 0)
                        score = -search(depth - 1, -beta, -alpha, null_move_depth, max_depth, &new_move, sp);
                else {
                        int new_depth = depth - 1;

                        if (n_played >= lmr_start && !sp.pos.is_capture_move(move) && !sp.pos.is_promotion_move(move)) {
                                is_lmr = true;
				sp.cs.data.n_lmr++;

				if (alpha == beta -1) {
#if defined(ESP32)
					constexpr double lmr_mul  = 0.5;
					constexpr double lmr_base = 1.0;
					int reduction = log(depth) * log(n_played + 1) * lmr_mul + lmr_base;
#else
					int reduction = lmr_reductions[std::min(N_LMR_DEPTH - 1, int(depth))][std::min(N_LMR_MOVES - 1, n_played)];
#endif
					new_depth = std::max(depth - reduction, 0);
				}
				else if (n_played >= lmr_start + 2)
					new_depth = (depth - 1) * 2 / 3;
				else {
					new_depth = depth - 2;
				}
			}

                        score = -search(new_depth, -alpha - 1, -alpha, null_move_depth, max_depth, &new_move, sp);

                        if (is_lmr && score > alpha)
                                score = -search(depth -1, -alpha - 1, -alpha, null_move_depth, max_depth, &new_move, sp);

                        if (score > alpha && score < beta)
                                score = -search(depth - 1, -beta, -alpha, null_move_depth, max_depth, &new_move, sp);
                }
		unmake_move(sp.nnue_eval, sp.pos, undo_actions);

		n_played++;

		if (score > best_score) {
			best_score = score;
			*m         = move;

			if (score > alpha) {
				if (score >= beta) {
					if (!sp.pos.is_capture_move(move))
						beta_cutoff_move = move;
					sp.cs.data.n_lmr_hit += is_lmr;
					break;
				}

				alpha = score;
			}
		}
	}

	// https://www.chessprogramming.org/History_Heuristic#History_Bonuses
	if (beta_cutoff_move.has_value()) {
		int bonus = depth * depth;
		for(auto move : move_list) {
			if (sp.pos.is_capture_move(move))
				continue;
			auto piece_type_from = sp.pos.piece_type_on(move.from_square());
			int  index           = history_index(sp.pos.side_to_move(), piece_type_from.value(), move.to_square());
			if (move == beta_cutoff_move.value()) {
				update_history(sp, index, bonus);
				break;
			}
			update_history(sp, index, -bonus);
		}

		sp.cs.data.n_moves_cutoff += n_played;
		sp.cs.data.nmc_nodes++;
	}

	if (n_played == 0) {
		if (in_check)
			best_score = -10000 + csd;
		else
			best_score = 0;
	}

	if (sp.stop->flag == false) {
		sp.cs.data.tt_store++;

		tt_entry_flag flag = EXACT;
		if (best_score <= start_alpha)
			flag = UPPERBOUND;
		else if (best_score >= beta)
			flag = LOWERBOUND;

		int work_score = eval_to_tt(best_score, csd);

		if (best_score > start_alpha && m->value())
			tti.store(hash, flag, depth, work_score, *m);
		else if (tt_move.has_value())
			tti.store(hash, flag, depth, work_score, tt_move.value());
		else
			tti.store(hash, flag, depth, work_score);
	}

	return best_score;
}

void timer(const int think_time, end_t *const ei)
{
	if (think_time > 0) {
		auto end_time = std::chrono::high_resolution_clock::now() += std::chrono::milliseconds{think_time};

		std::unique_lock<std::mutex> lk(ei->cv_lock);
		while(!ei->flag) {
			if (ei->cv.wait_until(lk, end_time) == std::cv_status::timeout)
				break;
		}
	}

	set_flag(ei);

#if !defined(__ANDROID__)
	my_trace("# time is up; set stop flag\n");
#endif
}

double calculate_EBF(const std::vector<uint64_t> & node_counts)
{
        size_t n = node_counts.size();
        return n >= 3 ? sqrt(double(node_counts.at(n - 1)) / double(node_counts.at(n - 3))) : -1;
}

std::string gen_pv_str_from_tt(const libchess::Position & p, const libchess::Move & m)
{
	std::vector<libchess::Move> pv = get_pv_from_tt(p, m);
	std::string pv_str;
	for(auto & move : pv) {
		if (pv_str.empty() == false)
			pv_str += " ";
		pv_str += move.to_str();
	}
	return pv_str;
}

void emit_result(const libchess::Position & pos, const libchess::Move & best_move, const int best_score, const uint64_t thought_ms, const std::vector<uint64_t> & node_counts, const int max_depth, const std::pair<uint64_t, uint64_t> & nodes)
{
	std::string pv_str     = gen_pv_str_from_tt(pos, best_move);
	double      ebf        = calculate_EBF(node_counts);
	std::string ebf_str    = ebf >= 0 ? std::to_string(ebf) : "";
	if (ebf_str.empty() == false)
		ebf_str = "ebf " + ebf_str + " ";

	uint64_t use_thought_ms = std::max(uint64_t(1), thought_ms);  // prevent div. by 0
	std::string score_str;
	if (abs(best_score) > 9800) {
		int mate_moves = (10000 - abs(best_score) + 1) / 2 * (best_score < 0 ? -1 : 1);
		score_str = "score mate " + std::to_string(mate_moves);
	}
	else {
		score_str = "score cp " + std::to_string(best_score);
	}

	printf("info depth %d %s nodes %" PRIu64 " %stime %" PRIu64 " nps %" PRIu64 " tbhits %" PRIu64 " hashfull %d pv %s\n",
			max_depth, score_str.c_str(),
			nodes.first, ebf_str.c_str(), thought_ms, uint64_t(nodes.first * 1000 / use_thought_ms),
			nodes.second, tti.get_per_mille_filled(), pv_str.c_str());
}

std::pair<libchess::Move, int> search_it(const int search_time, const bool is_absolute_time, search_pars_t *const sp, const int ultimate_max_depth, std::optional<uint64_t> max_n_nodes, const bool output)
{
	uint64_t t_offset = esp_timer_get_time();

#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
	std::thread *think_timeout_timer { nullptr };
#endif

	if (sp->thread_nr == 0) {
		if (search_time > 0) {
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
			think_timeout_timer = new std::thread([search_time, sp] {
					set_thread_name("searchtotimer");
					timer(search_time, sp->stop);
				});
#else
			esp_timer_start_once(think_timeout_timer, search_time * 1000ll);
#endif
		}
	}

	int16_t best_score = 0;

	auto move_list = sp->pos.legal_move_list();
	libchess::Move best_move { *move_list.begin() };

	if (move_list.size() > 1) {
		int16_t alpha     = -32767;
		int16_t beta      =  32767;

		int16_t add_alpha = 75;
		int16_t add_beta  = 75;

		int     max_depth = 1;

		libchess::Move cur_move { 0 };

		int alpha_repeat = 0;
		int beta_repeat  = 0;

		std::vector<uint64_t> node_counts;
		uint64_t previous_node_count = 0;

		while(ultimate_max_depth == -1 || max_depth <= ultimate_max_depth) {
#if defined(ESP32)
			sp->md = 0;
#endif
			if (max_depth >= 4)
				cur_move = sp->best_moves[max_depth - 3];
			int score = search(max_depth, alpha, beta, 0, max_depth, &cur_move, *sp);

			auto counts = simple_search_statistics();
			if (sp->stop->flag) {
				if (sp->thread_nr == 0 && output) {
#if !defined(__ANDROID__)
					my_trace("info string stop flag set\n");
#endif
					uint64_t thought_ms = (esp_timer_get_time() - t_offset) / 1000;
					emit_result(sp->pos, best_move, best_score, thought_ms, node_counts, max_depth, counts);
				}
				break;
			}

			uint64_t cur_n_nodes = counts.first;
			node_counts.push_back(cur_n_nodes - previous_node_count);
			previous_node_count  = cur_n_nodes;

			if (score <= alpha) {
				if (alpha_repeat >= 3)
					alpha = -10000;
				else {
					beta = (alpha + beta) / 2;
					alpha = score - add_alpha;
					if (alpha < -10000)
						alpha = -10000;
					add_alpha += add_alpha / 15 + 1;

					alpha_repeat++;
				}
			}
			else if (score >= beta) {
				if (beta_repeat >= 3)
					beta = 10000;
				else {
					alpha = (alpha + beta) / 2;
					beta = score + add_beta;
					if (beta > 10000)
						beta = 10000;
					add_beta += add_beta / 15 + 1;

					beta_repeat++;
				}
			}
			else {
				if (alpha != -32767) {
					sp->cs.data.alpha_distance += abs(score - alpha);
					sp->cs.data.n_alpha_distances++;
				}
				if (beta != 32767) {
					sp->cs.data.beta_distance  += abs(beta - score);
					sp->cs.data.n_beta_distances++;
				}

				alpha_repeat = beta_repeat = 0;

				alpha = score - add_alpha;
				if (alpha < -10000)
					alpha = -10000;

				beta = score + add_beta;
				if (beta > 10000)
					beta = 10000;

				best_move  = cur_move;
				best_score = score;

				uint64_t thought_ms = (esp_timer_get_time() - t_offset) / 1000;

				if (sp->thread_nr == 0 && output)
					emit_result(sp->pos, best_move, best_score, thought_ms, node_counts, max_depth, counts);

				if ((thought_ms > uint64_t(search_time / 2) && search_time > 0 && is_absolute_time == false) ||
				    (thought_ms >= search_time && is_absolute_time == true)) {
#if !defined(__ANDROID__)
					if (output)
						my_trace("info string time %u is up %" PRIu64 "\n", search_time, thought_ms);
#endif
					break;
				}

				add_alpha = 75;
				add_beta  = 75;

				if (max_depth == 127)
					break;

				if (max_n_nodes.has_value() && cur_n_nodes >= max_n_nodes.value()) {
					if (output)
						my_trace("info string node limit reached with %zu nodes\n", size_t(cur_n_nodes));
					break;
				}

				sp->best_moves[max_depth] = best_move;

				max_depth++;
			}
		}
	}
	else {
#if !defined(__ANDROID__)
		if (output)
			my_trace("info string only 1 move possible (%s for %s)\n", best_move.to_str().c_str(), sp->pos.fen().c_str());
#endif
		best_score = nnue_evaluate(sp->nnue_eval, sp->pos);
	}

	if (sp->thread_nr == 0) {
#if defined(linux) || defined(_WIN32) || defined(__ANDROID__) || defined(__APPLE__)
		set_flag(sp->stop);

		if (think_timeout_timer) {
			think_timeout_timer->join();
			delete think_timeout_timer;
		}
#else
		esp_timer_stop(think_timeout_timer);

		my_trace("# heap free: %u, max block size: %u\n", esp_get_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

		vTaskGetRunTimeStats();
#endif
	}

	return { best_move, best_score };
}
