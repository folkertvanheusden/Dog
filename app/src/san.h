#include <optional>
#include <string>
#include <libchess/Position.h>


std::optional<libchess::Move> SAN_to_move  (std::string san_move,     const libchess::Position & p);
std::optional<libchess::Move> validate_move(const libchess::Move & m, const libchess::Position & p);
std::string                   move_to_san  (const libchess::Position & p, const libchess::Move & m);
