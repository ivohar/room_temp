## Introduction

An utility to measure ambient air temperature with an MCP9801 or 
air temperature and humidity with an AHT10 or an SHT30 sensor, on a Raspberry Pi.
All those are I2C sensors.

## SW requirements / dependancies
* i2c-tools
* libi2c-dev
* linux-libc-dev

## HW requirements and connection
Apart from a Raspberry Pi (obviously) you need either an MCP9801 or an AHT10 or an SHT30 sensor.
The sensors are connected to the i2c-1 bus (P1-03 and P1-05) of the device. 

## Installation and usage

To build the software, use:

	make

To install it, you can use a command like:

    sudo cp room_temp /opt/vc/bin/

For usage, type:

    /opt/vc/bin/room_temp -h

