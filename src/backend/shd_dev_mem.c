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
#include "dev_mem_lookup.h"

struct shd_dev_mem_priv {
	int fd;
	intptr_t offset;
	struct shd_section_addr addr;
};

static int get_prot_flags(enum shd_map_prot prot)
{
	switch (prot) {
	case SHD_MAP_PROT_READ:
		return PROT_READ;
	case SHD_MAP_PROT_WRITE:
		return PROT_WRITE;
	case SHD_MAP_PROT_READ_WRITE:
		return PROT_READ | PROT_WRITE;
	default:
		ULOGW("Invalid protection flag %d", prot);
		return 0;
	}
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

static int shd_dev_mem_open(const char *blob_name,
			enum shd_section_operation op,
			const void *param,
			void **priv)
{
	struct shd_dev_mem_priv *self;
	int flags = get_open_flags(op);
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

	ret = dev_mem_lookup(blob_name, &self->offset);
	if (ret < 0) {
		ULOGW("Lookup for blob \"%s\" ended with error : %s",
				blob_name, strerror(-ret));
		goto close_fd;
	}

	*priv = self;

	return 0;

close_fd:
	close(self->fd);
clear:
	free(self);

	return ret;
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

static int shd_dev_mem_get_section(size_t size, enum shd_map_prot prot,
				void **out_ptr, void *priv)
{
	struct shd_dev_mem_priv *self = priv;
	void *ptr;

	ptr = mmap(0, size,
			get_prot_flags(prot),
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

static int shd_dev_mem_resize(size_t size, void *priv)
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
	.type = SHD_SECTION_DEV_MEM,
	.open = shd_dev_mem_open,
	.close = shd_dev_mem_close,
	.hdr_read = shd_dev_mem_read_header,
	.get_section_start = shd_dev_mem_get_section,
	.section_resize = shd_dev_mem_resize,
	.section_lock = shd_dev_mem_lock,
	.section_unlock = shd_dev_mem_unlock
};
