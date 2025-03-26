typedef enum { T_ANSI, T_VT100, T_ASCII } terminal_t;

extern terminal_t t;

#include <libchess/Position.h>

std::string myformat(const char *const fmt, ...);
void my_printf(const char *const fmt, ...);
void to_uart(const char *const buffer, int buffer_len);
void perft(libchess::Position &pos, int depth);
void run_tui();
