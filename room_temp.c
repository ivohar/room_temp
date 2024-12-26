/* ---------------------------------------------------------------------
 *                           room_temp.c
 * ---------------------------------------------------------------------
 * 
 * Copyright (c) 2013-2024 Ivaylo Haratcherev
 * All Rights Reserved
 * 
 * 
 * Author           :  Ivaylo Haratcherev
 * Acknowledgements :  Using some code from i2c-tools and examples 
 *                     regarding AHT10 and SHT30 sensors
 * Created          :  3/2/2014
 * Last revision    :  26/12/2024
 *
 * DESCRIPTION: Measure ambient air temperature with MCP9801 sensor
 *              or temperature and humidity with AHT10 or SHT30 sensor
 *              on the i2c-1 bus (P1-03 and P1-05) of the Raspberry pi
 * NOTE:        The AHT10 sensor is a low-cost sensor that does not seem
 *              to be very accurate in terms of humidity measurement.
 * --------------------------------------------------------------------*/


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <langinfo.h>
#include <iconv.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>


#define I2CBUS_FILE         "/dev/i2c-1"

#define MCP9801_ADDR            0x4f
#define MCP9801_TEMPER_REG      0
#define MCP9801_CFG_REG	        1
#define MCP9801_CFG_VALUE       0x60
#define MCP9801_CONV_TOUT_MS    330

#define TOUT_10_MS          10
#define TOUT_20_MS          20
#define BUSY_WAIT_RETRIES   20

#define AHTX0_ADDR_DEFAULT      0x38    ///< AHT default i2c address
#define AHTX0_ADDR_ALTERNATE    0x39    ///< AHT alternate i2c address
#define AHTX0_CMD_CALIBRATE     0xE1    ///< Calibration command
#define AHTX0_CMD_TRIGGER       0xAC    ///< Trigger reading command
#define AHTX0_CMD_SOFTRESET     0xBA    ///< Soft reset command
#define AHTX0_STATUS_BUSY       0x80    ///< Status bit for busy
#define AHTX0_STATUS_CALIBRATED 0x08    ///< Status bit for calibrated

#define SHT30_ADDR_DEFAULT      0x44    ///< SHT default i2c address
#define SHT30_CMD_MEAS_HREP_CSTRETCH_MSB 0x2C   ///< Measurement High Repeatability with Clock Stretch Enabled
#define SHT30_CMD_MEAS_HREP_CSTRETCH_LSB 0x06   ///< --
#define SHT30_CMD_MEAS_HREP_MSB 0x24    ///< Measurement High Repeatability with Clock Stretch Disabled
#define SHT30_CMD_MEAS_HREP_LSB 0x00    ///< --

//#define DEBUG
//#define DEBUG_GET_STATUS
//#define AHT10_SOFTRESET
//#define AHT10_CALIBRATE_EXIT_ON_FAIL

// pointer to the function that reads the sensor
// returns val>0 on success, -1 on error
// val represents sensor capabilities:
//		 1 if only temperature, 2 if humidity, 3 if both
typedef int8_t(*readsensor_fn)(int file, float * temp, float * humi);

int8_t read_mcp9801(int file, float * temp, float * humi)
{
	int res;

	res = i2c_smbus_read_byte_data(file, MCP9801_CFG_REG);
	if (res < 0) {
		fprintf(stderr, "Error: Config reg read failed\n");
		return -1;
	}
	// fix config if not the right one (12 bit resolution)
	if (MCP9801_CFG_VALUE != res) {
#ifdef DEBUG
		printf("Wrong config 0x%02x. Setting it to 0x%02x\n", res, MCP9801_CFG_VALUE);
#endif
		i2c_smbus_write_byte_data(file, MCP9801_CFG_REG, MCP9801_CFG_VALUE);
		// the chip needs some time before larger-resolution conversion
		usleep(MCP9801_CONV_TOUT_MS*1000);
	}
	res = i2c_smbus_read_word_data(file, MCP9801_TEMPER_REG);

	if (res < 0) {
		fprintf(stderr, "Error: Temperature reg read failed\n");
		return -1;
	}

	*temp = (res&0xff)+(double)(res>>12)/16;
	return 1;
}

static uint8_t getStatus(int file) {
  int8_t ret = i2c_smbus_read_byte(file);
#if defined(DEBUG_GET_STATUS)  
  printf("status:0x%02x ", ret & 0xff);
#endif
  if (ret < 0) {
    return 0xFF;
  }
  return (uint8_t)ret;
}

static int busy_wait_limited(int file, uint8_t loop_delay_ms, uint8_t max_retries) {
  uint8_t retries = 0;
  while (getStatus(file) & AHTX0_STATUS_BUSY) {
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

int8_t read_aht10(int file, float * temp, float * humi)
{

#if defined(AHT10_SOFTRESET)
	if (i2c_smbus_write_byte(file, AHTX0_CMD_SOFTRESET) < 0) {
		fprintf(stderr, "Error: reset failed\n");
		return -1;
	}
	usleep(TOUT_20_MS*1000);

	if (busy_wait_limited(file, TOUT_10_MS, BUSY_WAIT_RETRIES) < 0) {
		fprintf(stderr, "Error: reset busy timeout\n");
		return -1;
	}
#endif	

	uint8_t data_cal[2] = {0x08, 0x00};
	if (i2c_smbus_write_i2c_block_data(file, AHTX0_CMD_CALIBRATE, 2, data_cal) < 0) {
#if defined(AHT10_CALIBRATE_EXIT_ON_FAIL)
		fprintf(stderr, "Error: send calibrate cmd failed\n");
		return -1;
#endif
	}

	if (busy_wait_limited(file, TOUT_10_MS, BUSY_WAIT_RETRIES) < 0) {
		fprintf(stderr, "Error: calibrate busy timeout\n");
		return -1;
	}

	if (!(getStatus(file) & AHTX0_STATUS_CALIBRATED)) {
		fprintf(stderr, "Error: calibration failed\n");
		return -1;
	}	

	uint8_t data_trig[2] = {0x33, 0x00};
	if (i2c_smbus_write_i2c_block_data(file, AHTX0_CMD_TRIGGER, 2, data_trig) < 0) {
		fprintf(stderr, "Error: send trigger cmd failed\n");
		return -1;
	}

	if(busy_wait_limited(file, TOUT_20_MS, BUSY_WAIT_RETRIES) < 0) {
		fprintf(stderr, "Error: trigger busy timeout\n");
		return -1;
	}

	uint8_t data[6] = {0};

   	if (i2c_smbus_read_i2c_block_data(file, 0x00, 6, data) < 0) {
	 	fprintf(stderr, "Error: reading values failed\n");
	 	return -1;
	}
	uint32_t h = data[1];
	h <<= 8;
	h |= data[2];
	h <<= 4;
	h |= data[3] >> 4;
	*humi = ((float)h * 100) / 0x100000;

	uint32_t tdata = data[3] & 0x0F;
	tdata <<= 8;
	tdata |= data[4];
	tdata <<= 8;
	tdata |= data[5];
	*temp = ((float)tdata * 200 / 0x100000) - 50;	
	return 3;
}

int8_t read_sht30(int file, float * temp, float * humi)
{
	if (i2c_smbus_write_byte_data(file, SHT30_CMD_MEAS_HREP_MSB, SHT30_CMD_MEAS_HREP_LSB) < 0) {
		fprintf(stderr, "Error: send measure cmd failed\n");
		return -1;
	}

	usleep(TOUT_20_MS * 1000);

	uint8_t data[6] = {0};

   	if (i2c_smbus_read_i2c_block_data(file, 0x00, 6, data) < 0) {
	 	fprintf(stderr, "Error: reading values failed\n");
	 	return -1;
	}
	*temp = -45 + (175 * (float)(data[0] * 256 + data[1]) / 65535.0);
	*humi = 100 * (float)(data[3] * 256 + data[4]) / 65535.0;
	return 3;
}

static void help(void)
{
	fprintf(stderr,
		"room_temp by Ivaylo\n"
		"  Gets air temperature in deg C and humidity in %% (for sensors that support it)\n"
		"  Can read from MCP9801 (default), AHT10 or SHT30\n"
		"Usage: room_temp <options>\n"
		"Options:\n"
		"  -2   Use AHT10 sensor\n"
		"  -3   Use SHT30 sensor\n"
		"  -b   Bare format, temperature only (if not supported, considered as -r)\n"
		"  -r   Bare format, humidity only (if not supported, considered as -b)\n"
		"  -h   Print this help\n"
		"Options -2 and -3 are mutually exclusive\n"
		"If both are given, the last one is used\n");
	exit(1);
}

char degstr[5]; /* store the correct string to print degrees */

void set_degstr(void)
{
	const char *deg_default_text = "'C";

	/* Size hardcoded for better performance.
	   Don't forget to count the trailing \0! */
	size_t deg_latin1_size = 3;
	char *deg_latin1_text = "\260C";
	size_t nconv;
	size_t degstr_size = sizeof(degstr);
	char *degstr_ptr = degstr;

	iconv_t cd = iconv_open(nl_langinfo(CODESET), "ISO-8859-1");
	if (cd != (iconv_t) -1) {
		nconv = iconv(cd, &deg_latin1_text, &deg_latin1_size,
			      &degstr_ptr, &degstr_size);
		iconv_close(cd);

		if (nconv != (size_t) -1)
			return;
	}

	/* There was an error during the conversion, use the default text */
	strcpy(degstr, deg_default_text);
}

int main(int argc, char *argv[])
{
	int res, file, chip_addr = MCP9801_ADDR;
	int flags = 0;
	uint8_t bare_fmt = 0;
	float temp, humi;
	readsensor_fn readsensorfn = read_mcp9801;

	/* handle (optional) flags first */
	while (1+flags < argc && argv[1+flags][0] == '-') {
		switch (argv[1+flags][1]) {
		case '2': 
			chip_addr = AHTX0_ADDR_DEFAULT;
			readsensorfn = read_aht10; 
			break;
		case '3': 
			chip_addr = SHT30_ADDR_DEFAULT;
			readsensorfn = read_sht30; 
			break;
		case 'r': 
			bare_fmt |= 2; 
			break;
		case 'b': 
			bare_fmt |= 1; 
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


	file = open(I2CBUS_FILE, O_RDWR);
	if (file < 0)
	{
		fprintf(stderr, "Error: Could not open file `%s': %s\n", I2CBUS_FILE, strerror(errno));
		if (errno == EACCES)
			fprintf(stderr, "Run as root?\n");
		exit(1);
	}
	if (ioctl(file, I2C_SLAVE, chip_addr) < 0) {
		fprintf(stderr,
			"Error: Could not set address to 0x%02x: %s\n",
			chip_addr, strerror(errno));
		exit(1);
	}

	res = readsensorfn(file, &temp, &humi);

	close(file);

	if (res <= 0) {
		fprintf(stderr, "Sensor read failed - exiting...\n");
		exit(2);
	}

	if (res < 3 && bare_fmt > 0)
		bare_fmt = res;

	if (bare_fmt & 0x01)
		printf("%.2f\n", temp);
	if (bare_fmt & 0x02)
		printf("%.1f\n", humi);
	if (bare_fmt == 0)
	{
		setlocale(LC_CTYPE, "");
		set_degstr();
		if (res & 0x01)
			printf("Temp=%.2f%s\n", temp, degstr);
		if (res & 0x02)
			printf("Humi=%.1f%%\n", humi);
	}

	exit(0);
}
