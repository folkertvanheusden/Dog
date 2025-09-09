typedef enum { T_ANSI, T_VT100, T_ASCII } terminal_t;

extern terminal_t t;

#if defined(ESP32)
#include <driver/uart.h>
#if defined(ESP32_S3_WAVESHARE)
const uart_port_t uart_num = UART_NUM_0;
#else
const uart_port_t uart_num = UART_NUM_1;
#endif
#endif

#include <libchess/Position.h>

void store_cursor_position();
void restore_cursor_position();
std::string myformat(const char *const fmt, ...);
void my_printf(const char *const fmt, ...);
void to_uart(const char *const buffer, int buffer_len);
void perft(libchess::Position &pos, int depth);
void run_tui(const bool wait);
