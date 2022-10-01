#include <atomic>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <streambuf>

#include <driver/uart.h>
#include <esp32/rom/uart.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//#include <esp_int_wdt.h>
#include <esp_task_wdt.h>

#include <libchess/Position.h>
#include <libchess/UCIService.h>


std::atomic_bool stop { false };

libchess::Position position { libchess::constants::STARTPOS_FEN };

auto position_handler = [&position](const libchess::UCIPositionParameters & position_parameters) {
	position = libchess::Position { position_parameters.fen() };

	if (!position_parameters.move_list())
		return;

	for (auto & move_str : position_parameters.move_list()->move_list())
		position.make_move(*libchess::Move::from(move_str));
};


// http://www.josuttis.com/libbook/io/inbuf1.hpp.html
class inbuf : public std::streambuf {
  protected:
    /* data buffer:
     * - at most, four characters in putback area plus
     * - at most, six characters in ordinary read buffer
     */
    static const int bufferSize = 10;    // size of the data buffer
    char buffer[bufferSize];             // data buffer

  public:
    /* constructor
     * - initialize empty data buffer
     * - no putback area
     * => force underflow()
     */
    inbuf() {
        setg (buffer+4,     // beginning of putback area
              buffer+4,     // read position
              buffer+4);    // end position
    }

  protected:
    // insert new characters into the buffer
    virtual int_type underflow () {

        // is read position before end of buffer?
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }

        /* process size of putback area
         * - use number of characters read
         * - but at most four
         */
        int numPutback;
        numPutback = gptr() - eback();
        if (numPutback > 4) {
            numPutback = 4;
        }

        /* copy up to four characters previously read into
         * the putback buffer (area of first four characters)
         */
        std::memmove (buffer+(4-numPutback), gptr()-numPutback,
                      numPutback);

        // read new characters
	int c = 0;

	for(;;) {
		c = fgetc(stdin);

		if (c >= 0)
			break;

		vTaskDelay(1);
	}

	buffer[4] = c;

	int num = 1;

        // reset buffer pointers
        setg (buffer+(4-numPutback),   // beginning of putback area
              buffer+4,                // read position
              buffer+4+num);           // end of buffer

        // return next character
        return traits_type::to_int_type(*gptr());
    }
};

inbuf i;
std::istream is(&i);
libchess::UCIService uci_service{"esp32chesstest", "Folkert van Heusden", std::cout, is};

auto stop_handler = [&stop]() { stop = true; };

void main_task(void *)
{
	esp_task_wdt_delete(nullptr);

	std::ios_base::sync_with_stdio(true);
	std::cout.setf(std::ios::unitbuf);

	auto go_handler = [&position, &stop](const libchess::UCIGoParameters & go_parameters) {
		stop = false;

//		auto best_move = search::best_move_search(position);
//		if (best_move) {
//			libchess::UCIService::bestmove(best_move->to_str());
//		} else {
			libchess::UCIService::bestmove("0000");
//		}
	};

	uci_service.register_position_handler(position_handler);
	uci_service.register_go_handler(go_handler);
	uci_service.register_stop_handler(stop_handler);

	while (true) {
		std::string line;
		std::getline(is, line);

		if (line == "uci") {
			uci_service.run();
			break;
		}
	}

	printf("TASK TERMINATED\n");
}

extern "C" void app_main()
{
	TaskHandle_t main_task_handle;
	xTaskCreate(main_task, "chess", 131072, NULL, 10, &main_task_handle);

	printf("\n\n\n# HELLO, THIS IS DOG\n");

	for(;;) {
		vTaskDelay(1000);
	}
}
