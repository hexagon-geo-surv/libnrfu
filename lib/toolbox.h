/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2022 Leica Geosystems AG
 */
#ifndef TOOLBOX_H_
#define TOOLBOX_H_

uint32_t uint32_decode(const uint8_t *data);
uint16_t uint16_decode(const uint8_t *data);
uint8_t uint16_encode(uint16_t value, uint8_t *data);
uint8_t uint32_encode(uint32_t value, uint8_t *data);
uint32_t crc32_compute(const uint8_t *data, uint32_t size, uint32_t crc);

#endif /* TOOLBOX_H_ */
