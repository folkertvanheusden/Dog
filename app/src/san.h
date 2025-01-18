#include <optional>
#include <string>
#include <libchess/Position.h>


std::optional<libchess::Move> SAN_to_move(std::string san_move, const libchess::Position & pos);
