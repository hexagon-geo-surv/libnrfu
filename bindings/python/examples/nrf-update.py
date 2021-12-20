#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022 Leica Geosystems AG

import nrfu
import argparse
import sys
import os


def update_firmware(dev, update_file):
    def extract_files():
        from zipfile import ZipFile

        with ZipFile(update_file, 'r') as zip_object:
            firmware_file = ""
            init_packet_file = ""
            files = zip_object.namelist()
            for file_name in files:
                if file_name.endswith('.bin'):
                    zip_object.extract(file_name, '/tmp')
                    firmware_file = "/tmp/%s" % (file_name)
                if file_name.endswith('.dat'):
                    zip_object.extract(file_name, '/tmp')
                    init_packet_file = "/tmp/%s" % (file_name)
        return firmware_file, init_packet_file

    def cleanup(firmware_file, init_packet_file):
        os.remove(init_packet_file)
        os.remove(firmware_file)

    firmware_file, init_packet_file = extract_files()

    print("Starting update...")
    nrfu.update(dev, init_packet_file, firmware_file, log_level=nrfu.LOG_LEVEL_ERROR)
    print("Done!")

    cleanup(firmware_file, init_packet_file)


def parse_args(args):
    parser = argparse.ArgumentParser(
        description="Update Firmware on a nRF5 device \
        (running in bootloader) via DFU over serial port.")
    parser.add_argument(
        dest="device",
        help="serial device",
        type=str)
    parser.add_argument(
        dest="package",
        help="update package",
        type=str)
    return parser.parse_args(args)


def main():
    args = parse_args(sys.argv[1:])
    update_firmware(args.device, args.package)


if __name__ == "__main__":
    main()
