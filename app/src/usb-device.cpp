#if defined(linux) || defined(__ANDROID__)
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "main.h"


bool send_disp_cmd(const int fd, const std::string & cmd)
{
	const char *p   = cmd.c_str();
	size_t      len = cmd.size();

	while(len > 0) {
		int rc = write(fd, p, len);

		if (rc <= 0) {
			printf("# cannot write to usb display: %s\n", strerror(errno));
			return false;
		}

		p   += rc;
		len -= rc;
	}

	return true;
}

std::string __attribute__((format (printf, 1, 2) )) myformat(const char *const fmt, ...)
{
	char *buffer = NULL;
	va_list ap;

	va_start(ap, fmt);
	int len = vasprintf(&buffer, fmt, ap);
	va_end(ap);

	std::string result(buffer, len);
	free(buffer);

	return result;
}

void usb_disp(const std::string & device)
{
	int fd = open(device.c_str(), O_RDWR);
	if (fd == -1) {
		printf("# cannot open usb display %s\n", device.c_str());

		return;
	}

	termios tty { 0 };

	if (tcgetattr(fd, &tty) == -1) {
		printf("# cannot tcgetattr %s\n", device.c_str());
		close(fd);

		return;
	}

	tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
	tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
	tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size
	tty.c_cflag |= CS8; // 8 bits per byte (most common)
	tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
	tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

	tty.c_lflag &= ~ICANON;
	tty.c_lflag &= ~ECHO; // Disable echo
	tty.c_lflag &= ~ECHOE; // Disable erasure
	tty.c_lflag &= ~ECHONL; // Disable new-line echo
	tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
	tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

	tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
	tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
			       // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
			       // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

	tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
	tty.c_cc[VMIN] = 0;

	cfsetispeed(&tty, B1000000);
	cfsetospeed(&tty, B1000000);

	// Save tty settings, also checking for error
	if (tcsetattr(fd, TCSANOW, &tty) == -1) {
		printf("# cannot tcsetattr %s\n", device.c_str());
		close(fd);

		return;
	}

	pollfd fds[] { { fd, POLLIN, 0  } };

	for(;;) {
		if (!send_disp_cmd(fd, myformat("depth %d\n", sp1.md)))
			break;

		if (!send_disp_cmd(fd, myformat("move %s\n", sp1.move)))
			break;

		if (!send_disp_cmd(fd, myformat("score %d\n", abs(sp1.score))))
			break;

		if (!send_disp_cmd(fd, myformat("bitmap 0 %" PRIx64 "\n", wboard)))
			break;

		if (!send_disp_cmd(fd, myformat("bitmap 8 %" PRIx64 "\n", bboard)))
			break;

		for(;;) {
			int rc = poll(fds, 1, 0);
			if (rc != 1)
				break;

			char buffer[4096];
			if (read(fd, buffer, sizeof buffer) <= 0) {
				printf("# Read error (-u device)\n");
				break;
			}
		}

		usleep(101000);
	}

	close(fd);
}
#endif
