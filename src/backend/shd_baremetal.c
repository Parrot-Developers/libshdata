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
 * @file shd_baremetal.c
 *
 * @brief baremetal backend
 *
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "shd_hdr.h"
#include "shd_baremetal.h"

struct shd_baremetal_priv {
	void *ptr;
};

static int create_internal(const void *raw_param, void **priv)
{
	struct shd_baremetal_priv *self;
	const struct shd_baremetal_backend_param *param = raw_param;

	self = calloc(1, sizeof(*self));
	if (!self)
		return -ENOMEM;

	self->ptr = (void *) param->address;

	*priv = self;

	return 0;
}

static int shd_baremetal_create(const char *blob_name,
			size_t size,
			const void *raw_param,
			void **priv,
			bool *first_creation)
{
	int ret;

	ret = create_internal(raw_param, priv);
	if (ret < 0)
		return ret;

	*first_creation = false;

	return 0;
}

static int shd_baremetal_open(const char *blob_name,
			const void *raw_param,
			void **priv)
{
	return create_internal(raw_param, priv);
}

static int shd_baremetal_close(void *priv)
{
	struct shd_baremetal_priv *self = priv;

	free(self);

	return 0;
}

static int shd_baremetal_read_header(struct shd_hdr *hdr, void *priv)
{
	struct shd_baremetal_priv *self = priv;

	memcpy(hdr, self->ptr, sizeof(*hdr));

	return 0;
}

static int shd_baremetal_get_section(size_t size,
				void **out_ptr, void *priv)
{
	struct shd_baremetal_priv *self = priv;

	*out_ptr = self->ptr;

	return 0;
}

static int shd_baremetal_resize(void *priv)
{
	return 0;
}

static int shd_baremetal_lock(void *priv)
{
	return 0;
}

static int shd_baremetal_unlock(void *priv)
{
	return 0;
}

const struct shd_section_backend shd_baremetal_backend = {
	.create = shd_baremetal_create,
	.open = shd_baremetal_open,
	.close = shd_baremetal_close,
	.hdr_read = shd_baremetal_read_header,
	.get_section_start = shd_baremetal_get_section,
	.section_resize = shd_baremetal_resize,
	.section_lock = shd_baremetal_lock,
	.section_unlock = shd_baremetal_unlock
};
