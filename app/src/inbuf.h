#pragma once
#include <cstdio>
#include <cstring>
#include <streambuf>

#if defined(ESP32)
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

const uart_port_t uart_num = UART_NUM_2;
#endif


// http://www.josuttis.com/libbook/io/inbuf1.hpp.html
constexpr int bufferSize { 10 };    // size of the data buffer
class inbuf : public std::streambuf {
protected:
	/* data buffer:
	 * - at most, four characters in putback area plus
	 * - at most, six characters in ordinary read buffer
	 */
	char buffer[bufferSize];            // data buffer

public:
	/* constructor
	 * - initialize empty data buffer
	 * - no putback area
	 * => force underflow()
	 */
	inbuf() {
		setg(buffer+4,     // beginning of putback area
			buffer+4,     // read position
			buffer+4);    // end position
	}

protected:
	// insert new characters into the buffer
	virtual int_type underflow () {
		// is read position before end of buffer?
		if (gptr() < egptr())
			return traits_type::to_int_type(*gptr());

		/* process size of putback area
		 * - use number of characters read
		 * - but at most four
		 */
		int numPutback = gptr() - eback();
		if (numPutback > 4)
			numPutback = 4;

		/* copy up to four characters previously read into
		 * the putback buffer (area of first four characters)
		 */
		std::memmove(buffer+(4-numPutback), gptr()-numPutback, numPutback);

		// read new characters
		int c = 0;

		for(;;) {
			c = fgetc(stdin);
			if (c >= 0)
				break;

#if defined(WEMOS32) || defined(ESP32_S3_QTPY) || defined(ESP32_S3_XIAO)
			size_t length = 0;
			ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_num, (size_t*)&length));
			if (length) {
				char buffer = 0;
				if (uart_read_bytes(uart_num, &buffer, 1, 100)) {
					if (buffer == 13)
						c = 10;
					else
						c = buffer;
					break;
				}
			}
			vTaskDelay(1);
#endif
		}

		buffer[4] = c;

		int num = 1;

		// reset buffer pointers
		setg(buffer+(4-numPutback),   // beginning of putback area
				buffer+4,                // read position
				buffer+4+num);           // end of buffer

		// return next character
		return traits_type::to_int_type(*gptr());
	}
};
