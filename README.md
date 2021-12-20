# libnrfu
libnrfu is a library that supports serial Device Firmware Updates (DFU) for Nordic Semiconductor's nRF5 devices.

## Building

Requirements:

  * Meson 0.49 or newer
  * Ninja 1.8.2 or newer
  * Python3

To initialize the builder with python bindings (install to `/usr/lib/python3.8`):

    meson -Dwith-pymod=true -Dpython_site_dir=/usr/lib/python3.8 build

To initialize the builder without python bindings:

    meson build

To build the project from then on:

    ninja -C build

To install the project:

    ninja -C build install

All build files will be put in the `build` subdir.

## Tools

The command-line tool `nrf-update` which utilizes the library function is provided.

## Bindings

Bindings for python3 are provided and can be enabled by passing `with-pymod` option.
The installation path for the python module can be passed with `python_site_dir` (default is `libdir`).
An example of how to use the module in python is also provided.
