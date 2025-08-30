#include <libchess/Position.h>


int game_phase(const libchess::Position & pos);
// white, black
std::pair<int, int> count_mobility(const libchess::Position & pos);
std::pair<int, int> development   (const libchess::Position & pos);
