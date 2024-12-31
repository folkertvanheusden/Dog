#include <cstdint>
#include <optional>
#include <libchess/Position.h>

#include "main.h"


int qs(libchess::Position & pos, int alpha, int beta, int qsdepth, const search_pars_t & sp, const int thread_nr);
std::pair<libchess::Move, int> search_it(libchess::Position *const pos, const int search_time, const bool is_absolute_time, search_pars_t & sp, const int ultimate_max_depth, const int thread_nr, std::optional<uint64_t> max_n_nodes, chess_stats & cs);
