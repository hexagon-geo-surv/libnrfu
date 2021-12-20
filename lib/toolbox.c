// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 Leica Geosystems AG
 */

#include <stdint.h>

#include "toolbox.h"

uint32_t uint32_decode(const uint8_t *data)
{
	return ((((uint32_t)((uint8_t *)data)[0]) << 0)  |
		(((uint32_t)((uint8_t *)data)[1]) << 8)  |
		(((uint32_t)((uint8_t *)data)[2]) << 16) |
		(((uint32_t)((uint8_t *)data)[3]) << 24));
}

uint16_t uint16_decode(const uint8_t *data)
{
	return ((((uint16_t)((uint8_t *)data)[0])) |
		(((uint16_t)((uint8_t *)data)[1]) << 8));
}

uint8_t uint16_encode(uint16_t value, uint8_t *data)
{
	data[0] = (uint8_t) ((value & 0x00FF) >> 0);
	data[1] = (uint8_t) ((value & 0xFF00) >> 8);
	return sizeof(uint16_t);
}

uint8_t uint32_encode(uint32_t value, uint8_t *data)
{
	data[0] = (uint8_t) ((value & 0x000000FF) >> 0);
	data[1] = (uint8_t) ((value & 0x0000FF00) >> 8);
	data[2] = (uint8_t) ((value & 0x00FF0000) >> 16);
	data[3] = (uint8_t) ((value & 0xFF000000) >> 24);
	return sizeof(uint32_t);
}

uint32_t crc32_compute(const uint8_t *data, uint32_t size, uint32_t crc)
{
	uint32_t ret;
	int j;

	ret = (crc == 0) ? 0xFFFFFFFF : ~(crc);
	for (uint32_t i = 0; i < size; i++) {
		ret = ret ^ data[i];
		for (j = 8; j > 0; j--)
			ret = (ret >> 1) ^ (0xEDB88320U & ((ret & 1) ? 0xFFFFFFFF : 0));
	}
	return ~ret;
}
