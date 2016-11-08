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
 *   * Neither the name of the Parrot Company nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT COMPANY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file dev_lookup_x1.c
 *
 * @brief section lookup function for X1 target
 *
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <futils/futils.h>
#include "shd_section.h"
#include "backend/shd_shm.h"
#include "backend/shd_dev_mem.h"
#include <liblk_shdata.h>

static const struct {
	const char *name;
	struct shd_dev_mem_backend_param param;
} bpmp_sections[] = {
	{
		.name = "imu_flight",
		.param = {
			.offset = LIBLK_SHD_IMU_FLIGHT_ADDR,
		}
	}, {
		.name = "imu_cam",
		.param = {
			.offset = LIBLK_SHD_IMU_CAM_ADDR,
		}
	},{
		.name = "cam_h",
		.param = {
			.offset = LIBLK_SHD_CAM_H_ADDR,
		}
	},{
		.name = "cam_v",
		.param = {
			.offset = LIBLK_SHD_CAM_V_ADDR,
		}
	},{
		.name = "cppm",
		.param = {
			.offset = LIBLK_SHD_CPPM_ADDR,
		}
	},{
		.name = "ulog",
		.param = {
			.offset = LIBLK_SHD_ULOG_ADDR,
		}
	},
};

static int bpmp_add_and_fetch(int *ptr, int value)
{
	__sync_synchronize();
	*ptr += value;
	__sync_synchronize();
	return *ptr;
}

static struct shd_sync_primitives bpmp_sync_primitives = {
	.add_and_fetch = bpmp_add_and_fetch,
};

int shd_section_lookup(const char *blob_name,
			struct shd_section_properties *properties)
{
	size_t i;

	/* check if the section is stored in the memory shared with the bmbp */
	for (i = 0; i < SIZEOF_ARRAY(bpmp_sections); i++) {
		if (!strcmp(blob_name, bpmp_sections[i].name)) {
			properties->backend = &shd_dev_mem_backend;
			properties->backend_param = &bpmp_sections[i].param;
			properties->primitives = bpmp_sync_primitives;

			return 0;
		}
	}

	/*  on other cases use default backend (shm backend) */
	properties->backend = &shd_shm_backend;
	properties->backend_param = NULL;
	shd_sync_primitives_set_builtin(&properties->primitives);

	return 0;
}
