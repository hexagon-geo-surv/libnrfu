/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2022 Leica Geosystems AG
 */
#ifndef SERIAL_H_
#define SERIAL_H_

int serial_init(const char *devname);
int serial_send(int tty_fd, uint8_t *data, size_t data_length);
size_t serial_receive(int tty_fd, uint8_t *data, size_t max_length, uint8_t stop_byte);

#endif /* SERIAL_H_ */
