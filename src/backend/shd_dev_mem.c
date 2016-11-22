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
 * @file shd_dev_mem.c
 *
 * @brief /dev/mem
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>		/* For ftruncate */
#include <sys/file.h>		/* for flock */
#include <sys/mman.h>		/* For shm and PROT flags */
#include <futils/fdutils.h>
#include "shd_hdr.h"
#include "shd_section.h"
#include "shd_private.h"
#include "backend/shd_dev_mem.h"

struct shd_dev_mem_priv {
	int fd;
	uintptr_t offset;
	struct shd_section_addr addr;
	int writable;
	size_t creation_size;
};

static int open_internal(const struct shd_dev_mem_backend_param *param,
			int flags,
			struct shd_dev_mem_priv **priv)
{
	struct shd_dev_mem_priv *self;
	int ret;

	self = calloc(1, sizeof(*self));
	if (!self)
		return -ENOMEM;

	/* Open the base */
	self->fd = open("/dev/mem", flags, 0666);
	if (self->fd == -1) {
		ret = -errno;
		goto clear;
	}

	ret = fd_set_close_on_exec(self->fd);
	if (ret < 0)
		goto close_fd;

	self->offset = param->offset;

	*priv = self;

	return 0;

close_fd:
	close(self->fd);
clear:
	free(self);

	return ret;
}

static int shd_dev_mem_create(const char *blob_name,
		size_t size,
		const void *raw_param,
		void **priv,
		bool *first_creation)
{
	const struct shd_dev_mem_backend_param *param = raw_param;
	struct shd_dev_mem_priv *self;
	int flags = O_EXCL | O_RDWR | param->open_flags;
	int ret;

	ret = open_internal(param, flags, &self);
	if (ret < 0)
		return ret;

	self->writable = 1;
	self->creation_size = size;

	*priv = self;
	*first_creation = false;

	return 0;
}

static int shd_dev_mem_open(const char *blob_name,
			const void *raw_param,
			void **priv)
{
	const struct shd_dev_mem_backend_param *param = raw_param;
	struct shd_dev_mem_priv *self;
	int flags = O_EXCL | O_RDONLY | param->open_flags;
	int ret;

	ret = open_internal(param, flags, &self);
	if (ret < 0)
		return ret;

	self->writable = 0;

	*priv = self;

	return 0;
}

static int shd_dev_mem_close(void *priv)
{
	struct shd_dev_mem_priv *self = priv;

	if (self->addr.ptr)
		munmap(self->addr.ptr, self->addr.size);

	close(self->fd);
	free(self);

	return 0;
}

static int shd_dev_mem_read_header(struct shd_hdr *hdr, void *priv)
{
	struct shd_dev_mem_priv *self = priv;
	void *ptr;

	ptr = mmap(0, sizeof(*hdr),
			PROT_READ,
			MAP_SHARED,
			self->fd,
			self->offset);
	if (ptr == MAP_FAILED)
		return -ENOMEM;

	memcpy(hdr, ptr, sizeof(*hdr));
	munmap(ptr, sizeof(*hdr));

	return 0;
}

static int shd_dev_mem_get_section(size_t size,
				void **out_ptr, void *priv)
{
	struct shd_dev_mem_priv *self = priv;
	void *ptr;

	ptr = mmap(0, size,
			self->writable ? PROT_WRITE : PROT_READ,
			MAP_SHARED,
			self->fd,
			self->offset);
	if (ptr == MAP_FAILED)
		return -ENOMEM;

	self->addr.ptr = ptr;
	self->addr.size = size;

	*out_ptr = ptr;

	return 0;
}

static int shd_dev_mem_resize(void *priv)
{
	return 0;
}

static int shd_dev_mem_lock(void *priv)
{
	return 0;
}

static int shd_dev_mem_unlock(void *priv)
{
	return 0;
}

const struct shd_section_backend shd_dev_mem_backend = {
	.create = shd_dev_mem_create,
	.open = shd_dev_mem_open,
	.close = shd_dev_mem_close,
	.hdr_read = shd_dev_mem_read_header,
	.get_section_start = shd_dev_mem_get_section,
	.section_resize = shd_dev_mem_resize,
	.section_lock = shd_dev_mem_lock,
	.section_unlock = shd_dev_mem_unlock
};
