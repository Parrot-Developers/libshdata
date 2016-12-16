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
 * @file shd_ctx.c
 *
 * @brief shared memory data context implementation
 *
 */

#include <errno.h>		/* For error codes */
#include <stddef.h>		/* For NULL pointer */
#include <stdlib.h>		/* For memory allocation functions */
#include <string.h>		/* for strerror */
#include <unistd.h>		/* for close */

#include "shd_private.h"
#include "shd_section.h"
#include "shd_ctx.h"
#include "shd_window.h"
#include "shd_sync.h"
#include "libshdata.h"

struct shd_ctx *shd_ctx_new(struct shd_section_id *id, const char *blob_name)
{
	struct shd_ctx *ctx;
	const char *env_search_method;

	/* Allocate context structure */
	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		goto error;

	ctx->blob_name = strdup(blob_name);
	ctx->id = *id;
	ctx->sync_ctx = shd_sync_ctx_new(id);
	if (ctx->sync_ctx == NULL)
		goto error;

	ctx->window = shd_window_new();
	if (ctx->window == NULL)
		goto error;

	/* Set internal search method according to environment variable, if
	 * existing and valid ; else default to naive search method */
	env_search_method = getenv("LIBSHDATA_CONFIG_INTERNAL_SEARCH_METHOD");
	if (env_search_method == NULL) {
		ctx->hint = SHD_WINDOW_REF_SEARCH_NAIVE;
	} else {
		if (!strcmp(env_search_method, "NAIVE"))
			ctx->hint = SHD_WINDOW_REF_SEARCH_NAIVE;
		else if (!strcmp(env_search_method, "BINARY"))
			ctx->hint = SHD_WINDOW_REF_SEARCH_BINARY;
		else if (!strcmp(env_search_method, "DATE"))
			ctx->hint = SHD_WINDOW_REF_SEARCH_DATE;
		else
			ctx->hint = SHD_WINDOW_REF_SEARCH_NAIVE;
	}

	return ctx;

error:
	if (ctx != NULL) {
		shd_sync_ctx_destroy(ctx->sync_ctx);
		shd_window_destroy(ctx->window);
		shd_ctx_destroy(ctx);
	}
	return NULL;
}

int shd_ctx_mmap(struct shd_ctx *ctx,
			const struct shd_hdr_user_info *hdr_info)
{
	if (ctx == NULL)
		return -EINVAL;

	ctx->sect_mmap = shd_section_mapping_new(&ctx->id, hdr_info);
	if (ctx->sect_mmap == NULL)
		return -EFAULT;
	ctx->desc = shd_data_section_desc_new(ctx, hdr_info);
	if (ctx->desc == NULL)
		return -EFAULT;

	return 0;
}

int shd_ctx_destroy(struct shd_ctx *ctx)
{
	if (ctx != NULL) {
		shd_section_free(&ctx->id);
		shd_sync_ctx_destroy(ctx->sync_ctx);
		shd_section_mapping_destroy(ctx->sect_mmap);
		free(ctx->blob_name);
		free(ctx->desc);
		shd_window_destroy(ctx->window);
		free(ctx);
	}

	return 0;
}
