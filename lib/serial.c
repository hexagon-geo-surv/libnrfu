// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 Leica Geosystems AG
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/file.h>

#include "serial.h"

int serial_init(const char *devname)
{
	int fd;
	const speed_t speed = B115200;
	struct termios options;

	fd = open(devname, O_RDWR | O_NOCTTY);
	if (fd < 0)	{
		fprintf(stderr, "Failed to open %s: %s\n", devname, strerror(errno));
		goto err_exit;
	}

	if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
		fprintf(stderr, "Device %s already in use\n", devname);
		goto err_exit;
	}

	tcgetattr(fd, &options);

	if (cfsetispeed(&options, speed))
		goto err_exit;
	if (cfsetospeed(&options, speed))
		goto err_exit;

	/* 8N1 */
	options.c_cflag &= ~(PARENB | PARODD | CMSPAR | CSTOPB | CSIZE);
	options.c_cflag |= (CLOCAL | CREAD | CS8 | CRTSCTS);
	options.c_iflag = 0;
	options.c_oflag = 0;
	options.c_lflag = 0;
	options.c_cc[VMIN] = 1;
	options.c_cc[VTIME] = 0;

	if (tcflush(fd, TCIFLUSH) != 0)
		goto err_exit;

	if (tcsetattr(fd, TCSANOW, &options))
		goto err_exit;

	return fd;

err_exit:
	if (fd >= 0)
		close(fd);

	return -1;
}

int serial_send(int tty_fd, uint8_t *data, size_t data_length)
{
	if (write(tty_fd, data, data_length) < 0) {
		fprintf(stderr, "Failed to write: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

size_t serial_receive(int tty_fd, uint8_t *data, size_t max_length, uint8_t stop_byte)
{
	size_t n = 0;
	fd_set fds;
	struct timeval tv;

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(tty_fd, &fds);

	for (;;) {
		int v = 0;

		if (select(tty_fd + 1, &fds, NULL, NULL, &tv) <= 0) {
			fprintf(stderr, "%s: timeout!\n", __func__);
			break;
		}

		v = read(tty_fd, &data[n], 1);
		if (v < 0)
			return v;

		if (data[n] == stop_byte)
			break;

		if (v > 0)
			n += v;

		if (n >= max_length)
			break;
	}

	return n;
}
