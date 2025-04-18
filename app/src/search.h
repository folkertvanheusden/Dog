#include <cstdint>
#include <optional>
#include <libchess/Position.h>

#include "main.h"


class sort_movelist_compare
{
private:
	const search_pars_t & sp;
        std::vector<libchess::Move> first_moves;
        std::optional<libchess::Square> previous_move_target;

public:
        sort_movelist_compare(const search_pars_t & sp);
        void add_first_move(const libchess::Move move);
        int  move_evaluater(const libchess::Move move) const;
};

void sort_movelist(libchess::MoveList & move_list, sort_movelist_compare & smc);

void init_lmr();
bool is_insufficient_material_draw(const libchess::Position & pos);
int qs(int alpha, int beta, int qsdepth, search_pars_t & sp);
std::pair<libchess::Move, int> search_it(const int search_time, const bool is_absolute_time, search_pars_t *const sp, const int ultimate_max_depth, std::optional<uint64_t> max_n_nodes, const bool output);
std::optional<libchess::Move> str_to_move(const libchess::Position & p, const std::string & m);
