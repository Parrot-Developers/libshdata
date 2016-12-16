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
 * @file shd_section.c
 *
 * @brief shared memory section management.
 *
 */

#define _GNU_SOURCE
#include <errno.h>			/* For error codes */
#include <sys/stat.h>		/* For mode constants */
#include <fcntl.h>		/* For O_* constants */
#include <string.h>		/* String operations */
#include <stddef.h>		/* NULL pointer */
#include <stdlib.h>		/* For memory allocation functions */
#include <sys/file.h>		/* for flock */
#include <unistd.h>		/* For ftruncate */
#include "shd_private.h"
#include "shd_section.h"
#include "shd_hdr.h"
#include "shd_data.h"
#include "shd_utils.h"

struct shd_section_mapping {
	ptrdiff_t metadata_offset;
	size_t metadata_size;
	ptrdiff_t data_offset;
	size_t data_size;
	size_t total_size;
	ptrdiff_t hdr_offset;
	ptrdiff_t sync_offset;
};

static void get_offsets(const struct shd_hdr_user_info *hdr_info,
			struct shd_section_mapping *offsets)
{
	offsets->metadata_offset = ALIGN_UP(sizeof(struct shd_hdr));
	offsets->hdr_offset = offsetof(struct shd_hdr, user_info);
	offsets->sync_offset = offsetof(struct shd_hdr, sync_info);
	offsets->metadata_size = hdr_info->blob_metadata_hdr_size;
	offsets->data_offset = ALIGN_UP(offsets->metadata_offset +
						offsets->metadata_size);
	offsets->data_size = shd_data_get_total_size(hdr_info->blob_size,
						hdr_info->max_nb_samples);
	offsets->total_size = offsets->data_offset + offsets->data_size;
}

static struct shd_section *get_mmap(char *ptr,
					struct shd_section_mapping *offsets)
{
	struct shd_section *map;

	map = calloc(1, sizeof(*map));
	if (map == NULL)
		return NULL;

	map->section_top = ptr;
	map->header_top = ptr + offsets->hdr_offset;
	map->sync_top = ptr + offsets->sync_offset;
	map->metadata_blob_top = ptr + offsets->metadata_offset;
	map->data_top = ptr + offsets->data_offset;
	map->total_size = offsets->total_size;

	return map;
}

static int init_section_infos(const char *blob_name,
				struct shd_section_id *id,
				struct shd_section_properties *properties)
{
	int ret;

	if (blob_name == NULL || strchr(blob_name, '/') != NULL)
		return -EINVAL;

	ret = shd_section_lookup(blob_name, properties);
	if (ret < 0) {
		ULOGE("blob '%s' not found", blob_name);
		return -ENOENT;
	}

	id->backend = *properties->backend;
	id->primitives = properties->primitives;

	return 0;
}

int shd_section_create(const char *blob_name, size_t size,
			struct shd_section_id *id, bool *first_creation)
{
	struct shd_section_properties properties;
	int ret;

	ret = init_section_infos(blob_name, id, &properties);
	if (ret < 0)
		return ret;

	return (*id->backend.create) (blob_name, size, properties.backend_param,
			&id->instance, first_creation);
}

int shd_section_open(const char *blob_name,
			struct shd_section_id *id)
{
	struct shd_section_properties properties;
	int ret;

	ret = init_section_infos(blob_name, id, &properties);
	if (ret < 0)
		return ret;

	return (*id->backend.open) (blob_name, properties.backend_param,
			&id->instance);
}

int shd_section_lock(const struct shd_section_id *id)
{
	return (*id->backend.section_lock) (id->instance);
}

int shd_section_unlock(const struct shd_section_id *id)
{
	return (*id->backend.section_unlock) (id->instance);
}

int shd_section_resize(const struct shd_section_id *id)
{
	return (*id->backend.section_resize) (id->instance);
}

int shd_section_free(const struct shd_section_id *id)
{
	return (*id->backend.close) (id->instance);
}

struct shd_section *shd_section_mapping_new(const struct shd_section_id *id,
			const struct shd_hdr_user_info *hdr_info)
{
	struct shd_section *map = NULL;
	struct shd_hdr_user_info src_hdr_user;
	struct shd_section_mapping offsets;
	void *ptr = NULL;
	int ret;

	/* Either we can get the information from hdr_info itself, or we have
	 * to get it from the header of the shared memory section */
	if (hdr_info != NULL) {
		src_hdr_user = *hdr_info;
	} else {
		ret = shd_hdr_read(id, NULL, &src_hdr_user);
		if (ret < 0)
			goto error;
	}

	get_offsets(&src_hdr_user, &offsets);

	ret = (*id->backend.get_section_start) (offsets.total_size,
						&ptr, id->instance);
	if (ret < 0) {
		ULOGW("Could not allocate memory : %s", strerror(-ret));
		return NULL;
	}

	map = get_mmap(ptr, &offsets);

	return map;

error:
	return NULL;
}

int shd_section_mapping_destroy(struct shd_section *map)
{
	free(map);

	return 0;
}

size_t shd_section_get_total_size(const struct shd_hdr_user_info *hdr_info)
{
	if (hdr_info == NULL)
		return -1;

	return ALIGN_UP(sizeof(struct shd_hdr))
			+ ALIGN_UP(hdr_info->blob_metadata_hdr_size)
			+ shd_data_get_total_size(hdr_info->blob_size,
					hdr_info->max_nb_samples);
}

