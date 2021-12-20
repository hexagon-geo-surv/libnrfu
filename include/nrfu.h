/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2022 Leica Geosystems AG
 */
#ifndef NRFU_H_
#define NRFU_H_

enum nrfu_log_level {
	NRFU_LOG_LEVEL_SILENT = 0,
	NRFU_LOG_LEVEL_ERROR = 1,
	NRFU_LOG_LEVEL_INFO = 2,
	NRFU_LOG_LEVEL_DEBUG = 3,
};

int nrfu_update(const char *devname, const char *init_packet, const char *firmware, enum nrfu_log_level log_level);

#endif /* NRFU_H_ */
