#include <cstdint>
#include <optional>
#include <libchess/Position.h>

#include "main.h"


class sort_movelist_compare
{
private:
	const search_pars_t         & sp;
	int                           n_first_moves { 0 };
	std::array<libchess::Move, 2> first_moves;

public:
        sort_movelist_compare(const search_pars_t & sp);

        void add_first_move(const libchess::Move move);
        int  move_evaluater(const libchess::Move move) const;
};

void sort_movelist(libchess::MoveList & move_list, sort_movelist_compare & smc);

bool is_insufficient_material_draw(const libchess::Position & pos);
int qs(int alpha, int beta, int qsdepth, search_pars_t & sp);
std::tuple<libchess::Move, int, int> search_it(const int search_time, const bool is_absolute_time, search_pars_t *const sp, const int ultimate_max_depth, std::optional<uint64_t> max_n_nodes, const bool output);
std::optional<libchess::Move> str_to_move(const libchess::Position & p, const std::string & m);
