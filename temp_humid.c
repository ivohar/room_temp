/* ---------------------------------------------------------------------
 *                           temp_humid.c
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
 * DESCRIPTION: Measure air temperature and humidity with AHT10 sensor
 *              to the i2c-1 bus (P1-03 and P1-05) of the raspberry pi
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

#define TEMP_REGISTER_ADDR	0
#define CFG_REGISTER_ADDR	1
#define CFG_VALUE			0x60
#define CONV_TIMEOUT_MS		330
#define TIMEOUT1_MS			10
#define TIMEOUT2_MS			20

#define AHTX0_I2CADDR_DEFAULT 0x38   ///< AHT default i2c address
#define AHTX0_I2CADDR_ALTERNATE 0x39 ///< AHT alternate i2c address
#define AHTX0_CMD_CALIBRATE 0xE1     ///< Calibration command
#define AHTX0_CMD_TRIGGER 0xAC       ///< Trigger reading command
#define AHTX0_CMD_SOFTRESET 0xBA     ///< Soft reset command
#define AHTX0_STATUS_BUSY 0x80       ///< Status bit for busy
#define AHTX0_STATUS_CALIBRATED 0x08 ///< Status bit for calibrated

//#define DEBUG
//#define DEBUG_GET_STATUS
//#define AHT10_SOFTRESET

static void help(void)
{
	fprintf(stderr,
		"temp_humid v1 by Ivaylo\n"
		"Usage: room_temp [[-b] -r | -h]\n"
		"  Gets air temperature in deg C and humidity in %%\n"
		"  -b   Bare format (displays temperature only)\n"
		"  -h   Print this help\n");
	exit(1);
}


extern char degstr[]; /* store the correct string to print degrees */

uint8_t getStatus(int file) {
  int8_t ret = i2c_smbus_read_byte(file);
#if defined(DEBUG_GET_STATUS)  
  printf("status:0x%02x ", ret & 0xff);
#endif
  if (ret < 0) {
    return 0xFF;
  }
  return (uint8_t)ret;
}

int main(int argc, char *argv[])
{
	char *end;
	int file;
	int daddress;
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
	 || set_slave_addr(file, AHTX0_I2CADDR_DEFAULT, 0))
		exit(1);

#if defined(AHT10_SOFTRESET)
	if (i2c_smbus_write_byte(file, AHTX0_CMD_SOFTRESET) < 0) {
			fprintf(stderr, "Error: reset failed\n");
			exit(2);
	}
	usleep(TIMEOUT2_MS*1000);
#endif

	while (getStatus(file) & AHTX0_STATUS_BUSY) {
		printf("\nWait after soft reset\n");
		usleep(TIMEOUT1_MS*1000);
	}		

	uint8_t data_cal[2] = {0x08, 0x00};
	i2c_smbus_write_i2c_block_data(file, AHTX0_CMD_CALIBRATE, 2, data_cal);

	while (getStatus(file) & AHTX0_STATUS_BUSY) {
		printf("\nWait after calibrate\n");
		usleep(TIMEOUT1_MS*1000);
	}
	if (!(getStatus(file) & AHTX0_STATUS_CALIBRATED)) {
			fprintf(stderr, "Error: calibration failed\n");
			exit(2);
	}	

	uint8_t data_trig[2] = {0x33, 0x00};
	i2c_smbus_write_i2c_block_data(file, AHTX0_CMD_TRIGGER, 2, data_trig);

	while (getStatus(file) & AHTX0_STATUS_BUSY) {
#if defined(DEBUG)		
		printf("\nWait after trigger\n");
#endif
		usleep(TIMEOUT2_MS*1000);
	}

	uint8_t data[6] = {0};
	int res;

   	res = i2c_smbus_read_i2c_block_data(file, 0x00, 6, data);
#if defined(DEBUG)	
	printf("res: 0x%x\n",res);
#endif
	if (res < 0)
	{
	 	fprintf(stderr, "Error: reading values failed\n");
	 	exit(2);
	}
#if defined(DEBUG)
	for (uint16_t i = 0; i < sizeof(data); i++) {
		printf("0x%02x ", data[i]);
	}
	printf("\n");
#endif

	uint32_t h = data[1];
	h <<= 8;
	h |= data[2];
	h <<= 4;
	h |= data[3] >> 4;
	double humidity = ((float)h * 100) / 0x100000;

	uint32_t tdata = data[3] & 0x0F;
	tdata <<= 8;
	tdata |= data[4];
	tdata <<= 8;
	tdata |= data[5];
	double temperature = ((float)tdata * 200 / 0x100000) - 50;

	close(file);

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
