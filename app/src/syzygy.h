#include <optional>
#include <string>
#include <libchess/Position.h>

std::optional<std::pair<libchess::Move, int> > probe_fathom_root(const libchess::Position & lpos);
std::optional<int>                             probe_fathom_nonroot(const libchess::Position & lpos);

void fathom_init(const std::string & path);
