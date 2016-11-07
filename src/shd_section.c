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
#include <sys/mman.h>		/* For shm and PROT flags */
#include <sys/stat.h>		/* For mode constants */
#include <fcntl.h>		/* For O_* constants */
#include <string.h>		/* String operations */
#include <stddef.h>		/* NULL pointer */
#include <stdlib.h>		/* For memory allocation functions */
#include <limits.h>		/* For NAME_MAX macro */
#include <sys/mman.h>		/* For mmap */
#include <sys/file.h>		/* for flock */
#include <unistd.h>		/* For ftruncate */
#include <futils/fdutils.h>
#include "shd_private.h"
#include "shd_section.h"
#include "shd_hdr.h"
#include "shd_data.h"
#include "shd_utils.h"

#include "dev_mem_lookup.h"

#define SHD_SECTION_MODE		0666

struct shd_section_mapping {
	ptrdiff_t metadata_offset;
	size_t metadata_size;
	ptrdiff_t data_offset;
	size_t data_size;
	size_t total_size;
	ptrdiff_t hdr_offset;
	ptrdiff_t sync_offset;
};

enum shd_section_operation {
	SHD_OPERATION_CREATE,
	SHD_OPERATION_OPEN_WR,
	SHD_OPERATION_OPEN_RD
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

static int check_blob_name(const char *blob_name)
{
	if (blob_name == NULL || strchr(blob_name, '/') != NULL)
		return -EINVAL;
	if (strlen(blob_name) >= SHD_SECTION_BLOB_NAME_MAX_LEN)
		return -ENAMETOOLONG;
	return 0;
}

static int get_open_flags(enum shd_section_operation op)
{
	switch (op) {
	case SHD_OPERATION_CREATE:
		return O_CREAT | O_EXCL | O_RDWR;
	case SHD_OPERATION_OPEN_RD:
		return O_RDONLY;
	case SHD_OPERATION_OPEN_WR:
		return O_CREAT | O_RDWR;
	default:
		return -EINVAL;
	}
}

static int from_blob_to_section_shm_dev(const char *blob_name,
					const char *shd_root,
					struct shd_section_id *id,
					enum shd_section_operation op)
{
	char *shm_name = NULL;
	int mode = SHD_SECTION_MODE;
	int flags = get_open_flags(op);

	/* Build shm section name */
	int ret = asprintf(&shm_name, "%s%s",
				SHD_SECTION_PREFIX, blob_name);
	if (ret < 0)
		return -ENOMEM;

	id->type = SHD_SECTION_DEV_SHM;

	/* Create a new shm memory using shm_open */
	id->id.shm_fd = shm_open(shm_name, flags, mode);
	if (id->id.shm_fd == -1) {
		free(shm_name);
		return -errno;
	}

	free(shm_name);
	return 0;
}

static int from_blob_to_section_shm_other(const char *blob_name,
						const char *shd_root,
						struct shd_section_id *id,
						enum shd_section_operation op)
{
	char *file_path = NULL;
	int mode = SHD_SECTION_MODE;
	int flags = get_open_flags(op);

	/* Build file path from root dir path and blob name */
	int ret = asprintf(&file_path, "%s%s%s",
				shd_root, SHD_SECTION_PREFIX, blob_name);
	if (ret < 0)
		return -ENOMEM;

	id->type = SHD_SECTION_SHM_OTHER;

	/* Create a new shm memory object using a simple "open" */
	id->id.shm_fd = open(file_path, flags, mode);
	if (id->id.shm_fd == -1) {
		ret = -errno;
		goto exit;
	}

	ret = fd_set_close_on_exec(id->id.shm_fd);
	if (ret < 0)
		close(id->id.shm_fd);

exit:
	free(file_path);
	return ret;
}

static int from_blob_to_section_dev_mem(const char *blob_name,
					const char *shd_root,
					struct shd_section_id *id,
					enum shd_section_operation op)
{
	int mode = SHD_SECTION_MODE;
	int flags = get_open_flags(op);
	int ret;

	id->type = SHD_SECTION_DEV_MEM;

	/* Open the base "/dev/mem" */
	id->id.dev_mem.fd = open(shd_root, flags, mode);
	if (id->id.shm_fd == -1) {
		ret = -errno;
		goto error_no_close;
	}

	ret = fd_set_close_on_exec(id->id.shm_fd);
	if (ret < 0)
		goto error_close;

	ret = dev_mem_lookup(blob_name, &id->id.dev_mem.offset);
	if (ret < 0) {
		ULOGW("Lookup for blob \"%s\" ended with error : %s",
				blob_name, strerror(-ret));
		goto error_close;
	}

	return 0;

error_close:
	close(id->id.dev_mem.fd);
error_no_close:
	return ret;
}

static int from_blob_to_section(const char *blob_name,
				const char *shd_root,
				struct shd_section_id *id,
				enum shd_section_operation op)
{
	/* Test input string for validity */
	int check = check_blob_name(blob_name);
	if (check < 0)
		return check;

	if (shd_root == NULL)
		return from_blob_to_section_shm_dev(blob_name, shd_root,
							id, op);
	else if (!strncmp(shd_root, "/dev/mem", PATH_MAX))
		return from_blob_to_section_dev_mem(blob_name, shd_root,
							id, op);
	else
		return from_blob_to_section_shm_other(blob_name, shd_root,
							id, op);
}

int shd_section_new(const char *blob_name, const char *shd_root,
			struct shd_section_id *id)
{
	return from_blob_to_section(blob_name, shd_root,
					id, SHD_OPERATION_CREATE);
}

int shd_section_open(const char *blob_name, const char *shd_root,
			struct shd_section_id *id)
{
	return from_blob_to_section(blob_name, shd_root,
					id, SHD_OPERATION_OPEN_WR);
}

int shd_section_get(const char *blob_name, const char *shd_root,
			struct shd_section_id *id)
{
	return from_blob_to_section(blob_name, shd_root,
					id, SHD_OPERATION_OPEN_RD);
}

int shd_section_lock(const struct shd_section_id *id)
{
	int ret = -1;

	switch (id->type) {
	case SHD_SECTION_DEV_SHM:
	case SHD_SECTION_SHM_OTHER:
		ret = flock(id->id.shm_fd, LOCK_EX | LOCK_NB);
		break;
	case SHD_SECTION_DEV_MEM:
		ret = 0;
		break;
	default:
		break;
	}

	if (ret < 0)
		return -errno;
	else
		return 0;
}

int shd_section_unlock(const struct shd_section_id *id)
{
	int ret = -1;

	switch (id->type) {
	case SHD_SECTION_DEV_SHM:
	case SHD_SECTION_SHM_OTHER:
		ret = flock(id->id.shm_fd, LOCK_UN);
		break;
	case SHD_SECTION_DEV_MEM:
		ret = 0;
		break;
	default:
		break;
	}

	if (ret < 0)
		return -errno;
	else
		return 0;
}

int shd_section_resize(const struct shd_section_id *id, size_t size)
{
	int ret = -1;

	switch (id->type) {
	case SHD_SECTION_DEV_SHM:
	case SHD_SECTION_SHM_OTHER:
		/* @todo The size we get from the function should certainly be
		 * compared against PAGE_SIZE, which is the minimum size of a
		 * shared memory it seems */
		ret = ftruncate(id->id.shm_fd, size);
		break;
	case SHD_SECTION_DEV_MEM:
		ret = 0;
		break;
	default:
		break;
	}

	if (ret < 0)
		return -errno;
	else
		return 0;
}

int shd_section_free(const struct shd_section_id *id)
{
	switch (id->type) {
	case SHD_SECTION_DEV_SHM:
	case SHD_SECTION_SHM_OTHER:
		close(id->id.shm_fd);
		break;
	default:
		close(id->id.dev_mem.fd);
		break;
	}

	return 0;
}

struct shd_section *shd_section_mapping_new(const struct shd_section_id *id,
			const struct shd_hdr_user_info *hdr_info,
			int prot)
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

	switch (id->type) {
	case SHD_SECTION_DEV_SHM:
	case SHD_SECTION_SHM_OTHER:
		ptr = mmap(0, offsets.total_size,
					prot,
					MAP_SHARED,
					id->id.shm_fd,
					0);

		if (ptr == MAP_FAILED) {
			ULOGW("Could not allocate memory : %s",
					strerror(errno));
			goto error;
		}

		map = get_mmap(ptr, &offsets);
		break;
	case SHD_SECTION_DEV_MEM:
		ptr = mmap(0, offsets.total_size,
					prot,
					MAP_SHARED,
					id->id.dev_mem.fd,
					id->id.dev_mem.offset);

		if (ptr == MAP_FAILED) {
			ULOGW("Could not allocate memory : %s",
					strerror(errno));
			goto error;
		}

		map = get_mmap(ptr, &offsets);
		break;
	default:
		break;
	}

	return map;

error:
	return NULL;
}

int shd_section_mapping_destroy(struct shd_section *map)
{
	if (map) {
		munmap(map->section_top, map->total_size);
		free(map);
	}
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

