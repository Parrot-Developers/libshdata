/**
 * Copyright (c) 2016 Parrot S.A.
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
 * @file shd_shm.c
 *
 * @brief shd backend.
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>		/* For ftruncate */
#include <limits.h>		/* For NAME_MAX macro */
#include <sys/file.h>		/* for flock */
#include <sys/mman.h>		/* For shm and PROT flags */
#include <futils/futils.h>
#include <futils/fdutils.h>
#include "shd_hdr.h"
#include "shd_section.h"
#include "shd_private.h"
#include "backend/shd_shm.h"

#define SHD_SECTION_PREFIX		"/shd_"

struct shd_shm_priv {
	int fd;
	struct shd_section_addr addr;
	int writable;
	size_t creation_size;
};

static int shd_shm_open_internal(struct shd_shm_priv *self,
		const char *blob_name,
		int flags,
		int mode)
{
	char *path;
	int ret;

	ret = asprintf(&path, "%s%s", SHD_SECTION_PREFIX, blob_name);
	if (ret < 0)
		return -ENOMEM;

	self->fd = shm_open(path, flags, mode);
	if (self->fd == -1) {
		ret = -errno;
		goto error;
	}

	return 0;

error:
	free(path);

	return ret;
}

static int shd_shm_open_other_root(struct shd_shm_priv *self,
		const struct shd_shm_backend_param *param,
		const char *blob_name,
		int flags,
		int mode)
{
	char *path;
	int ret;

	ret = asprintf(&path, "%s%s%s",
			param->root, SHD_SECTION_PREFIX, blob_name);
	if (ret < 0)
		return -ENOMEM;

	self->fd = open(path, flags, mode);
	if (self->fd == -1) {
		ret = -errno;
		goto clear_path;
	}

	ret = fd_set_close_on_exec(self->fd);
	if (ret < 0)
		goto close_fd;

	free(path);

	return 0;

close_fd:
	close(self->fd);
clear_path:
	free(path);

	return ret;
}

static int open_internal(const char *blob_name,
			const struct shd_shm_backend_param *param,
			int flags,
			struct shd_shm_priv **priv)
{
	struct shd_shm_priv *self;
	int mode = 0666;
	int ret;

	self = calloc(1, sizeof(*self));
	if (!self)
		return -ENOMEM;

	if (!param || !param->root) {
		ret = shd_shm_open_internal(self, blob_name, flags, mode);
		if (ret < 0)
			goto error;
	} else {
		ret = shd_shm_open_other_root(self, param, blob_name,
				flags, mode);
		if (ret < 0)
			goto error;
	}

	*priv = self;

	return 0;

error:
	free(self);

	return ret;
}

static int shd_shm_create(const char *blob_name,
		size_t size,
		const void *raw_param,
		void **priv,
		bool *first_creation)
{
	const struct shd_shm_backend_param *param = raw_param;
	struct shd_shm_priv *self;
	int ret;

	ret = open_internal(blob_name, param, O_CREAT | O_EXCL | O_RDWR, &self);
	if (ret == 0) {
		*first_creation = true;
	} else if (ret == -EEXIST) {
		ret = open_internal(blob_name, param, O_EXCL | O_RDWR, &self);
		if (ret < 0)
			return ret;

		*first_creation = false;
	} else {
		return ret;
	}

	self->writable = 1;
	self->creation_size = size;

	*priv = self;

	return 0;
}

static int shd_shm_open(const char *blob_name,
			const void *raw_param,
			void **priv)
{
	const struct shd_shm_backend_param *param = raw_param;
	struct shd_shm_priv *self;
	int ret;

	ret = open_internal(blob_name, param, O_EXCL | O_RDONLY, &self);
	if (ret < 0)
		return ret;

	self->writable = 0;

	*priv = self;

	return 0;
}

static int shd_shm_close(void *priv)
{
	struct shd_shm_priv *self = priv;

	if (self->addr.ptr)
		munmap(self->addr.ptr, self->addr.size);

	close(self->fd);
	free(self);

	return 0;
}

static int shd_shm_read_header(struct shd_hdr *hdr, void *priv)
{
	struct shd_shm_priv *self = priv;
	void *ptr;
	int ret;

	if (lseek(self->fd, 0, SEEK_END) <= 0) {
		ULOGW("Can not m'map in zero-sized file");
		return -ENOMEM;
	}

	ptr = mmap(0,
		   sizeof(hdr),
		   PROT_READ,
		   MAP_SHARED,
		   self->fd,
		   0);
	if (ptr == MAP_FAILED) {
		ret = -errno;
		ULOGW("Could not allocate memory : %m");
		return ret;
	}

	memcpy(hdr, ptr, sizeof(*hdr));
	munmap(ptr, sizeof(*hdr));

	return 0;
}

static int shd_shm_get_section(size_t size,
				void **out_ptr, void *priv)
{
	struct shd_shm_priv *self = priv;
	void *ptr = NULL;
	int ret;

	ptr = mmap(0,
		   size,
		   self->writable ? PROT_WRITE : PROT_READ,
		   MAP_SHARED,
		   self->fd,
		   0);
	if (ptr == MAP_FAILED) {
		ret = -errno;
		ULOGW("Could not allocate memory : %m");
		return ret;
	}

	self->addr.ptr = ptr;
	self->addr.size = size;

	*out_ptr = ptr;

	return 0;
}

static int shd_shm_resize(void *priv)
{
	struct shd_shm_priv *self = priv;

	return ftruncate(self->fd, self->creation_size);
}

static int shd_shm_lock(void *priv)
{
	struct shd_shm_priv *self = priv;

	return flock(self->fd, LOCK_EX | LOCK_NB);
}

static int shd_shm_unlock(void *priv)
{
	struct shd_shm_priv *self = priv;

	return flock(self->fd, LOCK_UN);
}

const struct shd_section_backend shd_shm_backend = {
	.create = shd_shm_create,
	.open = shd_shm_open,
	.close = shd_shm_close,
	.hdr_read = shd_shm_read_header,
	.get_section_start = shd_shm_get_section,
	.section_resize = shd_shm_resize,
	.section_lock = shd_shm_lock,
	.section_unlock = shd_shm_unlock
};
