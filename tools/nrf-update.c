// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (C) 2022 Leica Geosystems AG
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <nrfu.h>

static void print_help(void)
{
	printf("nrf-update\n");
	printf("\n");
	printf("Update Firmware on a nRF5 device (running in bootloader) via DFU over serial port.\n");
	printf("\n");
	printf("Reqired arguments:\n");
	printf("  -d <device>\t\tserial device\n");
	printf("  -i <init-packet>\tinit-packet (*.dat) file\n");
	printf("  -f <firmware>\t\tfirmware (*.bin) file\n");
	printf("\n");
	printf("Optional arguments:\n");
	printf("  -l <log-level>\t1-4 (1 means quite, 4 highest verbosity, default is 2)\n");
	printf("  -h\t\t\tdisplay this message and exit\n");
	printf("\n");
}

int main(int argc, char **argv)
{
	int c;
	char *device = NULL, *init_packet = NULL, *firmware = NULL;
	int log_input = -1;
	enum nrfu_log_level log_level;

	while ((c = getopt(argc, argv, "hd:i:f:l:")) != -1) {
		switch (c) {
		case 'd':
			device = optarg;
			break;
		case 'i':
			init_packet = optarg;
			break;
		case 'f':
			firmware = optarg;
			break;
		case 'l':
			log_input = atoi(optarg);
			break;
		case 'h':
			print_help();
			return 0;
		case '?':
		default:
			return -1;
		}
	}

	switch (log_input) {
	case 1:
		log_level = NRFU_LOG_LEVEL_SILENT;
		break;
	case 3:
		log_level = NRFU_LOG_LEVEL_INFO;
		break;
	case 4:
		log_level = NRFU_LOG_LEVEL_DEBUG;
		break;
	case 2:
	default:
		log_level = NRFU_LOG_LEVEL_ERROR;
		break;
	}

	if (!device) {
		print_help();
		fprintf(stderr, "No device provided\n");
		return -1;
	}

	if (!init_packet) {
		print_help();
		fprintf(stderr, "No *.dat file provided\n");
		return -1;
	}

	if (!firmware) {
		print_help();
		fprintf(stderr, "No *.bin file provided\n");
		return -1;
	}

	if (nrfu_update(device, init_packet, firmware, log_level) < 0) {
		fprintf(stderr, "Update failed!\n");
		return -1;
	}

	return 0;
}
