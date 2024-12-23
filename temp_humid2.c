/* ---------------------------------------------------------------------
 *                           temp_humid2.c
 * ---------------------------------------------------------------------
 * 
 * Copyright (c) 2024 Ivaylo Haratcherev
 * All Rights Reserved
 * 
 * 
 * Author           :  Ivaylo Haratcherev
 * Acknowledgements :  Using some code from i2c-tools and lm-sensors
 * Created			:  3/2/2014
 * Last revision	:  7/12/2024
 *
 * DESCRIPTION: Measure air temperature and humidity with SHT30 sensor
 *              on the i2c-1 bus (P1-03 and P1-05) of the Raspberry pi
 * --------------------------------------------------------------------*/


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <locale.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include "i2cbusses.h"
#include "common.h"

#define I2CBUS				1

#define TOUT_10_MS			10
#define TOUT_20_MS			20
#define BUSY_WAIT_RETRIES	20

#define SHT30_I2CADDR_DEFAULT 0x44   ///< AHT default i2c address
#define SHT30_CMD_MEASURE 0x2C       ///< Measurement command
#define SHT30_CMD_MEASURE1 0x24       ///< Measurement command


//#define DEBUG

static void help(void)
{
	fprintf(stderr,
		"temp_humid2 v1 by Ivaylo\n"
		"Usage: room_temp [[-b] -r | -h]\n"
		"  Gets air temperature in deg C and humidity in %%\n"
		"  -b   Bare format (displays temperature only)\n"
		"  -h   Print this help\n");
	exit(1);
}


extern char degstr[]; /* store the correct string to print degrees */

static int8_t getStatus(int file) {
  int8_t ret = i2c_smbus_read_byte(file);

  return ret;
}

static int busy_wait_limited(int file, uint8_t loop_delay_ms, uint8_t max_retries) {
  uint8_t retries = 0;
  while (getStatus(file) < 0 ) {
	usleep(loop_delay_ms * 1000);
#if defined(DEBUG)		
		printf("Busy wait...%d\n", retries);
#endif
	retries++;
	if (retries > max_retries) {
	  return -1;
	}
  }
  return 0;
}

int main(int argc, char *argv[])
{
	char *end;
	int file;
	char filename[20];
	int flags = 0;
	int bare_fmt = 0;

	/* handle (optional) flags first */
	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch (argv[1+flags][1]) {
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
	 || set_slave_addr(file, SHT30_I2CADDR_DEFAULT, 0))
		exit(1);

	//uint8_t data_meas[1] = {0x06}; // i2c_smbus_write_i2c_block_data(file, SHT30_CMD_MEASURE, 1, data_meas)
	if (i2c_smbus_write_byte_data(file, SHT30_CMD_MEASURE1, 0x00) < 0) {
		fprintf(stderr, "Error: send measure cmd failed\n");
		exit(2);
	}

	// if(busy_wait_limited(file, TOUT_20_MS, BUSY_WAIT_RETRIES) < 0) {
	// 	fprintf(stderr, "Error: trigger busy timeout\n");
	// 	exit(2);
	// }

	usleep(20 * 1000);

	uint8_t data[6] = {0};

   	if (i2c_smbus_read_i2c_block_data(file, 0x00, 6, data) < 0) {
	 	fprintf(stderr, "Error: reading values failed\n");
	 	exit(2);
	}
#if defined(DEBUG)
	for (uint16_t i = 0; i < sizeof(data); i++) {
		printf("0x%02x ", data[i]);
	}
	printf("\n");
#endif
	close(file);

	double temperature = -45 + (175 * (float)(data[0] * 256 + data[1]) / 65535.0);
	double humidity = 100 * (float)(data[3] * 256 + data[4]) / 65535.0;


	setlocale(LC_CTYPE, "");
	set_degstr();
	if (bare_fmt)
		printf("%.2f\n", temperature);
	else 
	{
		printf("Temp=%.2f%s\n", temperature, degstr);
		printf("Humi=%.1f%%\n", humidity);
	}

	exit(0);
}
