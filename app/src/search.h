#include <cstdint>
#include <optional>
#include <libchess/Position.h>

#include "main.h"


class sort_movelist_compare
{
private:
        const libchess::Position  & p;
	const search_pars_t       & sp;
        std::vector<libchess::Move> first_moves;
        std::optional<libchess::Square> previous_move_target;

public:
        sort_movelist_compare(const libchess::Position & p, const search_pars_t & sp);
        void add_first_move(const libchess::Move move);
        int  move_evaluater(const libchess::Move move) const;
};

void sort_movelist(libchess::MoveList & move_list, sort_movelist_compare & smc);

void init_lmr();
bool is_insufficient_material_draw(const libchess::Position & pos);
int qs(libchess::Position & pos, int alpha, int beta, int qsdepth, search_pars_t & sp, const int thread_nr);
std::pair<libchess::Move, int> search_it(libchess::Position & pos, const int search_time, const bool is_absolute_time, search_pars_t & sp, const int ultimate_max_depth, const int thread_nr, std::optional<uint64_t> max_n_nodes, chess_stats & cs);
