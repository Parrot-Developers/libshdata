/**
 * Copyright (c) 2015 Parrot S.A.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the <organization> nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file dev_mem_lookup_x1.c
 *
 * @brief Dev-mem lookup table for X1 target
 *
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <dev_mem_lookup.h>
#include <liblk_shdata.h>

#define BLOB_NAME_MAX_SIZE 255

#define	LK_DEVICE_COUNT 6

const char *lk_device[LK_DEVICE_COUNT] = {
	"imu_flight",
	"imu_cam",
	"cam_h",
	"cam_v",
	"cppm",
	"ulog"
};

uint32_t lk_addr[LK_DEVICE_COUNT] = {
	LIBLK_SHD_IMU_FLIGHT_ADDR,
	LIBLK_SHD_IMU_CAM_ADDR,
	LIBLK_SHD_CAM_H_ADDR,
	LIBLK_SHD_CAM_V_ADDR,
	LIBLK_SHD_CPPM_ADDR,
	LIBLK_SHD_ULOG_ADDR
};

int dev_mem_lookup(const char *blob_name, intptr_t *phys_addr)
{
	int i;
	for(i = 0; i < LK_DEVICE_COUNT; i++) {
		if (!strncmp(blob_name, lk_device[i], BLOB_NAME_MAX_SIZE)) {
			*phys_addr = lk_addr[i];
			return 0;
		}
	}
	return -ENOENT;
}
