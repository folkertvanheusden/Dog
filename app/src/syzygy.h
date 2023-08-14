#include <optional>
#include <string>
#include <libchess/Position.h>

std::optional<std::pair<libchess::Move, int> > probe_fathom(libchess::Position & lpos);

void fathom_init(const std::string & path);
