#pragma once
#include <cstdio>
#include <cstring>
#include <streambuf>

#if defined(ESP32)
#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif


// http://www.josuttis.com/libbook/io/inbuf1.hpp.html
class inbuf : public std::streambuf {
protected:
	/* data buffer:
	 * - at most, four characters in putback area plus
	 * - at most, six characters in ordinary read buffer
	 */
	static const int bufferSize = 10;    // size of the data buffer
	char buffer[bufferSize];             // data buffer
	bool local_echo { false };

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

	void set_local_echo(const bool state) {
		local_echo = state;
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
			if (c >= 0) {
				if (local_echo)
					printf("%c", c);
				break;
			}

#if !defined(linux) && !defined(_WIN32) && !defined(__ANDROID__)
			vTaskDelay(1);
#endif
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
