/* ---------------------------------------------------------------------
 *                           room_temp.c
 * ---------------------------------------------------------------------
 * 
 * Copyright (c) 2013 Ivaylo Haratcherev
 * All Rights Reserved
 * 
 * 
 * Author           :  Ivaylo Haratcherev
 * Acknowledgements :  Using some code from i2c-tools and lm-sensors
 * Created			:  3/2/2014
 * Last revision	:  3/2/2014
 *
 * DESCRIPTION: Measure ambient room air temperature with MCP9801 sensor
 *              to the i2c-1 bus (P1-03 and P1-05) of the raspberry pi
 * --------------------------------------------------------------------*/


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include "i2cbusses.h"
#include "common.h"


#define I2CBUS				1
#define CHIP_ADDRESS		0x4f
#define TEMP_REGISTER_ADDR	0
#define CFG_REGISTER_ADDR	1
#define CFG_VALUE			0x60
#define CONV_TIMEOUT_MS		330

static void help(void)
{
	fprintf(stderr,
		"room_temp v1 by Ivaylo\n"
		"Usage: room_temp [[-b] -r | -h]\n"
		"  Gets room temperature in deg C\n"
		"  -r   Print raw reg value as well\n"
		"  -b   Bare format\n"
		"  -h   Print this help\n");
	exit(1);
}


extern char degstr[]; /* store the correct string to print degrees */

int main(int argc, char *argv[])
{
	char *end;
	int res, file;
	int daddress;
	char filename[20];
	int flags = 0;
	int raw_fmt = 0, bare_fmt = 0;

	/* handle (optional) flags first */
	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch (argv[1+flags][1]) {
		case 'r': 
			raw_fmt = 1; 
			break;
		case 'b': 
			bare_fmt = 1; 
			break;
		case 'h': 
			help();
			exit(0);
		default:
			fprintf(stderr, "Error: Unsupported option "
				"\"%s\"!\n", argv[1+flags]);
			help();
			exit(1);
		}
		flags++;
	}


	file = open_i2c_dev(I2CBUS, filename, sizeof(filename), 0);
	if (file < 0
	 || set_slave_addr(file, CHIP_ADDRESS, 0))
		exit(1);

	res = i2c_smbus_read_byte_data(file, CFG_REGISTER_ADDR);
	if (res < 0) {
		fprintf(stderr, "Error: Config reg read failed\n");
		exit(2);
	}
	// fix config if not the right one (12 bit resolution)
	if (CFG_VALUE != res) {
		printf("Wrong config 0x%02x. Setting it to 0x%02x\n", res, CFG_VALUE);
		i2c_smbus_write_byte_data(file, CFG_REGISTER_ADDR, CFG_VALUE);
		// the chip needs some time before larger-resolution conversion
		usleep(CONV_TIMEOUT_MS*1000);
	}
	res = i2c_smbus_read_word_data(file, TEMP_REGISTER_ADDR);

	close(file);

	if (res < 0) {
		fprintf(stderr, "Error: Temperature reg read failed\n");
		exit(2);
	}

	setlocale(LC_CTYPE, "");
	set_degstr();
	if (raw_fmt)
		printf("Raw=0x%04x\n", res);
	if (bare_fmt)
		printf("%.2f\n", (res&0xff)+(double)(res>>12)/16);
	else
		printf("Temp=%.1f%s\n", (res&0xff)+(double)(res>>12)/16, degstr);

	exit(0);
}
