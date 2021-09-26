/*******************************************************************************
* Copyright (c) 2020, STMicroelectronics - All Rights Reserved
*
* This file is part of the VL53L5CX Ultra Lite Driver and is dual licensed,
* either 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0081
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, the VL53L5CX Ultra Lite Driver may be distributed under the
* terms of 'BSD 3-clause "New" or "Revised" License', in which case the
* following provisions apply instead of the ones mentioned above :
*
********************************************************************************
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* mdification, are permitted provided that the following conditions are met:
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*dev
*******************************************************************************/

#include <fcntl.h> // open()
#include <unistd.h> // close()
#include <time.h> // clock_gettime()

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <sys/ioctl.h>

#include "platform.h"
#include "types.h"

#define VL53L5CX_ERROR_GPIO_SET_FAIL	-1
#define VL53L5CX_COMMS_ERROR		-2
#define VL53L5CX_ERROR_TIME_OUT		-3

#define SUPPRESS_UNUSED_WARNING(x) \
	((void) (x))

#define VL53L5CX_COMMS_CHUNK_SIZE  1024

#define ST_TOF_IOCTL_TRANSFER           _IOWR('a',0x1, void*)

#if defined(I2C0)
#define I2CBUS "/dev/i2c-0"
#else
#define I2CBUS "/dev/i2c-1"
#endif

static uint8_t i2c_buffer[VL53L5CX_COMMS_CHUNK_SIZE];

int32_t vl53l5cx_comms_init(VL53L5CX_Platform * p_platform){
	p_platform->fd = open(I2CBUS, O_RDONLY);
	if (p_platform->fd == -1) {
		return VL53L5CX_COMMS_ERROR;
	}

	if (ioctl(p_platform->fd, I2C_SLAVE, 0x29) <0) {
		return VL53L5CX_COMMS_ERROR;
	}

	return 0;
}

int32_t vl53l5cx_comms_close(VL53L5CX_Platform * p_platform)
{
	close(p_platform->fd);
	return 0;
}

int32_t write_read_multi(
		int fd,
		uint16_t reg_address,
		uint8_t *pdata,
		uint32_t count,
		int write_not_read)
{
	struct i2c_rdwr_ioctl_data packets;
	struct i2c_msg messages[2];

	uint32_t data_size = 0;
	uint32_t position = 0;

	if (write_not_read) {
		do {
			data_size = (count - position) > (VL53L5CX_COMMS_CHUNK_SIZE-2) ? (VL53L5CX_COMMS_CHUNK_SIZE-2) : (count - position);

			memcpy(&i2c_buffer[2], &pdata[position], data_size);

			i2c_buffer[0] = (reg_address + position) >> 8;
			i2c_buffer[1] = (reg_address + position) & 0xFF;

			messages[0].addr = 0x29;
			messages[0].flags = 0; //I2C_M_WR;
			messages[0].len = data_size + 2;
			messages[0].buf = i2c_buffer;

			packets.msgs = messages;
			packets.nmsgs = 1;

			if (ioctl(fd, I2C_RDWR, &packets) < 0)
				return VL53L5CX_COMMS_ERROR;
			position +=  data_size;

		} while (position < count);
	}

	else {
		do {
			data_size = (count - position) > VL53L5CX_COMMS_CHUNK_SIZE ? VL53L5CX_COMMS_CHUNK_SIZE : (count - position);

			i2c_buffer[0] = (reg_address + position) >> 8;
			i2c_buffer[1] = (reg_address + position) & 0xFF;

			messages[0].addr = 0x29;
			messages[0].flags = 0; //I2C_M_WR;
			messages[0].len = 2;
			messages[0].buf = i2c_buffer;

			messages[1].addr = 0x29;
			messages[1].flags = I2C_M_RD;
			messages[1].len = data_size;
			messages[1].buf = pdata + position;

			packets.msgs = messages;
			packets.nmsgs = 2;

			if (ioctl(fd, I2C_RDWR, &packets) < 0)
				return VL53L5CX_COMMS_ERROR;

			position += data_size;

		} while (position < count);
	}
	return 0;
}

int32_t write_multi(
		int fd,
		uint16_t reg_address,
		uint8_t *pdata,
		uint32_t count)
{
	return(write_read_multi(fd, reg_address, pdata, count, 1));
}

int32_t read_multi(
		int fd,
		uint16_t reg_address,
		uint8_t *pdata,
		uint32_t count)
{
	return(write_read_multi(fd, reg_address, pdata, count, 0));
}

uint8_t RdByte(
		VL53L5CX_Platform * p_platform,
		uint16_t reg_address,
		uint8_t *p_value)
{
	return(read_multi(p_platform->fd, reg_address, p_value, 1));
}

uint8_t WrByte(
		VL53L5CX_Platform * p_platform,
		uint16_t reg_address,
		uint8_t value)
{
	return(write_multi(p_platform->fd, reg_address, &value, 1));
}

uint8_t RdMulti(
		VL53L5CX_Platform * p_platform,
		uint16_t reg_address,
		uint8_t *p_values,
		uint32_t size)
{
	return(read_multi(p_platform->fd, reg_address, p_values, size));
}

uint8_t WrMulti(
		VL53L5CX_Platform * p_platform,
		uint16_t reg_address,
		uint8_t *p_values,
		uint32_t size)
{
	return(write_multi(p_platform->fd, reg_address, p_values, size));
}

void SwapBuffer(
		uint8_t 		*buffer,
		uint16_t 	 	 size)
{
	uint32_t i, tmp;
	
	/* Example of possible implementation using <string.h> */
	for(i = 0; i < size; i = i + 4) 
	{
		tmp = (
		  buffer[i]<<24)
		|(buffer[i+1]<<16)
		|(buffer[i+2]<<8)
		|(buffer[i+3]);
		
		memcpy(&(buffer[i]), &tmp, 4);
	}
}	

uint8_t WaitMs(
		VL53L5CX_Platform * p_platform,
		uint32_t time_ms)
{
	(void) p_platform;
	usleep(time_ms*1000);
	return 0;
}