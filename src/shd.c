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
 * @file shd.c
 *
 * @brief shared memory data low level.
 *
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>		/* For prot constants */

#define SHD_ADVANCED_WRITE_API
#define SHD_ADVANCED_READ_API
#include "libshdata.h"
#include "shd_section.h"
#include "shd_ctx.h"
#include "shd_hdr.h"
#include "shd_data.h"
#include "shd_mdata_hdr.h"
#include "shd_private.h"

#if defined(BUILD_LIBULOG)
ULOG_DECLARE_TAG(libshdata);
#endif

struct shd_ctx *shd_create(const char *blob_name, const char *shd_root,
			   const struct shd_hdr_user_info *hdr_info,
			   const void *blob_metadata_hdr)
{
	struct shd_ctx *ctx = NULL;
	int ret = -1;
	int rev_nb;
	bool first_creation = true;
	struct shd_section_id id;

	if (blob_name == NULL
		|| hdr_info == NULL
		|| blob_metadata_hdr == NULL) {
		ULOGE("Invalid arguments for shared memory section creation");
		goto error;
	}

	/* Try to open shared memory section as if it were new. If it doesn't
	 * work, re-try to open it in non-exclusive mode. */
	ret = shd_section_new(blob_name, shd_root, &id);
	if (ret == -EEXIST) {
		ret = shd_section_open(blob_name, shd_root, &id);
		if (ret >= 0)
			first_creation = false;
	}

	if (ret < 0) {
		ULOGE("Could not add new shared memory section \"%s\" : %s",
				blob_name,
				strerror(-ret));
		goto error;
	}

	SHD_HOOK(HOOK_SECTION_CREATED_NOT_RESIZED);

	/* Try to lock the shared memory section to prevent it from being open
	 * at the same time by another process */
	ret = shd_section_lock(&id);
	if (ret < 0) {
		ULOGE("Could not get lock on shared memory section \"%s\" : %s",
				blob_name,
				strerror(-ret));
		if (ret == -EWOULDBLOCK)
			ULOGE("Section is already locked by another process");
		goto error;
	}

	SHD_HOOK(HOOK_SECTION_CREATED_LOCK_TAKEN);
	SHD_HOOK(HOOK_SECTION_CREATED_BEFORE_TRUNCATE);

	/* Resize the memory section accordingly */
	if (shd_section_resize(&id, shd_section_get_total_size(hdr_info)) < 0) {
		ULOGE("Could not resize shared memory section \"%s\": %s",
				blob_name,
				strerror(errno));
		goto error;
	}

	/* Create associated context */
	ctx = shd_ctx_new(&id, blob_name);

	ret = shd_ctx_mmap(ctx, hdr_info, PROT_READ | PROT_WRITE);
	if (ret < 0) {
		ULOGE("Could not RW-map the shared memory section \"%s\" : %s",
				blob_name,
				strerror(-ret));
		goto error;
	}

	shd_sync_invalidate_section(ctx->sync_ctx,
					ctx->sect_mmap->sync_top,
					first_creation);

	/* Write section and metadata headers */
	shd_hdr_write(ctx->sect_mmap->section_top, hdr_info);
	shd_mdata_hdr_write(ctx->sect_mmap->metadata_blob_top,
				blob_metadata_hdr,
				hdr_info->blob_metadata_hdr_size);
	shd_data_clear_section(ctx->desc);

	rev_nb = shd_sync_update_global_revision_nb(ctx->sync_ctx,
						ctx->sect_mmap->sync_top);
	if (rev_nb < 0) {
		ULOGE("Revision number update went very wrong");
		goto error;
	}

	SHD_HOOK(HOOK_SECTION_CREATED_BEFORE_UNLOCK);

	/* Unlock the shared memory section */
	ret = shd_section_unlock(&id);
	if (ret < 0) {
		ULOGE("Could not unlock shared memory section \"%s\" : %s",
				blob_name,
				strerror(ret));
		goto error;
	}

	ULOGI("Memory section \"%s\" successfully %s with revision number : %d",
			blob_name,
			first_creation ? "created" : "reopen for writing",
			rev_nb);

	return ctx;

error:
	if (ctx != NULL)
		shd_ctx_destroy(ctx);
	if (ret >= 0)
		shd_section_free(&id);
	return NULL;
}

struct shd_ctx *shd_open(const char *blob_name, const char *shd_root,
		struct shd_revision **rev)
{
	int rev_nb = -1;
	int ret = -1;
	struct shd_ctx *ctx = NULL;
	struct shd_section_id id;

	/* Init rev pointer to NULL so that a free on this pointer in case of
	* error doesn't hurt */
	*rev = NULL;

	if (blob_name == NULL) {
		ULOGE("Invalid argument for shared memory section opening");
		goto error;
	}

	SHD_HOOK(HOOK_SECTION_OPEN_START);

	ret = shd_section_get(blob_name, shd_root, &id);
	if (ret < 0) {
		if (ret == -ENOENT)
			ULOGD("Could not get shared memory section "
			      "\"%s\" : %s",
			      blob_name, strerror(-ret));
		else
			ULOGW("Could not get shared memory section "
			      "\"%s\" : %s",
			      blob_name, strerror(-ret));

		goto error;
	}

	ctx = shd_ctx_new(&id, blob_name);

	*rev = calloc(1, sizeof(struct shd_revision));
	if (*rev == NULL)
		goto error;

	ret = shd_ctx_mmap(ctx, NULL, PROT_READ);
	if (ret < 0) {
		ULOGE("Could not RO-map the shared memory section \"%s\" : %s",
				blob_name,
				strerror(-ret));
		goto error;
	}

	SHD_HOOK(HOOK_SECTION_OPEN_MMAP_DONE);

	rev_nb = shd_sync_update_local_revision_nb(ctx->sync_ctx,
					ctx->sect_mmap->sync_top);
	if (rev_nb == -EAGAIN) {
		ULOGW("Section \"%s\" is being updated by a producer",
				blob_name);
		/* @todo should set up a timer and try again, or something */
		goto error;
	}

	ULOGI("Memory section \"%s\" successfully open "
			"with revision number : %d", blob_name, rev_nb);

	(*rev)->nb_creations = rev_nb;
	return ctx;

error:
	free(*rev);
	if (ctx != NULL)
		shd_ctx_destroy(ctx);
	return NULL;
}

int shd_close(struct shd_ctx *ctx, struct shd_revision *rev)
{
	if (ctx == NULL)
		return -EINVAL;

	ULOGI("Trying to close memory section \"%s\"", ctx->blob_name);

	free(rev);
	shd_ctx_destroy(ctx);

	ULOGI("Memory section close successful");

	return 0;
}

int shd_new_sample(struct shd_ctx *ctx,
			const struct shd_sample_metadata *metadata)
{
	int ret;

	if (ctx == NULL || metadata == NULL)
		return -EINVAL;

	ret = shd_data_reserve_write(ctx);
	if (ret < 0)
		return ret;

	return shd_data_write_metadata(ctx, metadata);
}

int shd_write_quantity(struct shd_ctx *ctx,
			const struct shd_quantity *quantity,
			const void *src)
{
	if (ctx == NULL || quantity == NULL || src == NULL)
		return -EINVAL;

	return shd_data_write_quantity(ctx, quantity, src);
}

int shd_commit_sample(struct shd_ctx *ctx)
{
	if (ctx == NULL)
		return -EINVAL;

	return shd_data_end_write(ctx);
}

int shd_write_new_blob(struct shd_ctx *ctx,
			const void *src,
			const size_t size,
			const struct shd_sample_metadata *metadata)
{
	int ret;
	/* Setup a fake quantity which covers the whole blob */
	struct shd_quantity fake_quantity = {
		.quantity_offset = 0,
		.quantity_size = size
	};

	if (ctx == NULL || src == NULL || metadata == NULL)
		return -EINVAL;

	SHD_HOOK(HOOK_SAMPLE_WRITE_START);

	ret = shd_new_sample(ctx, metadata);
	if (ret < 0)
		return ret;

	ret = shd_write_quantity(ctx, &fake_quantity, src);
	if (ret < 0)
		return ret;

	SHD_HOOK(HOOK_SAMPLE_WRITE_BEFORE_COMMIT);

	ret = shd_commit_sample(ctx);
	if (ret < 0)
		return ret;

	SHD_HOOK(HOOK_SAMPLE_WRITE_AFTER_COMMIT);

	return 0;
}

int shd_select_samples(struct shd_ctx *ctx,
			const struct shd_sample_search *search,
			struct shd_sample_metadata **metadata,
			struct shd_search_result *result)
{
	int ret = -1;

	if (ctx == NULL || search == NULL
			|| metadata == NULL || result == NULL) {
		ret = -EINVAL;
		goto exit;
	}

	ret = shd_data_find(ctx, search);
	if (ret < 0)
		goto exit;

	result->nb_matches = ret;

	ret = shd_data_read_metadata(ctx, metadata);
	if (ret < 0)
		goto exit;

	result->r_sample_idx = ret;

	ret = 0;

exit:
	return ret;
}

int shd_read_quantity(struct shd_ctx *ctx,
			const struct shd_quantity *quantity,
			void *dst,
			size_t dst_size)
{
	int ret = -1;

	if (ctx == NULL || dst == NULL) {
		ret = -EINVAL;
		goto exit;
	}

	if (quantity == NULL)
		ret = shd_data_read_blob(ctx, dst, dst_size);
	else
		ret = shd_data_read_quantity(ctx, quantity, dst, dst_size);

exit:
	return ret;
}

int shd_read_from_sample(struct shd_ctx *ctx,
				int n_quantities,
				const struct shd_sample_search *search,
				const struct shd_quantity quantity[],
				struct shd_quantity_sample qty_samples[])
{
	int ret = -ENOSYS;

	if (ctx == NULL || search == NULL
			|| qty_samples == NULL
			|| n_quantities < 0
			|| (n_quantities > 0 && quantity == NULL)
			|| search->nb_values_after_date > 0
			|| search->nb_values_before_date > 0) {
		ret = -EINVAL;
		goto exit;
	}

	ret = shd_data_find(ctx, search);
	if (ret < 0)
		goto exit;

	if (n_quantities > 0) {
		ret = shd_data_read_quantity_sample(ctx, n_quantities,
						quantity, qty_samples);
	} else {
		struct shd_quantity fake_qty[1] = {
			{ 0, ctx->desc->blob_size }
		};

		ret = shd_data_read_quantity_sample(ctx, 1, fake_qty,
				qty_samples);
	}

exit:
	if (ret < 0) {
		/* There has been an error, but some of them are expected to
		 * happen in the nominal cases so they are only recorded for
		 * debug */
		if (ret == -EAGAIN)
			ULOGD("%s: No sample has been produced yet",
					ctx ? ctx->blob_name : "??");
		else if (ret == -ENOENT)
			ULOGD("%s: No sample was found to match search",
					ctx ? ctx->blob_name : "??");
		else
			ULOGW("%s: %s read failed with error : %s",
				ctx ? ctx->blob_name : "??",
				n_quantities > 0 ? "Quantity sample" : "Blob",
				strerror(-ret));
	} else {
		/* There are still some potential error cases here */
		if (n_quantities > 0 && ret < n_quantities)
			ULOGW("%s: Could not read all quantities",
					ctx ? ctx->blob_name : "??");
		else if (n_quantities == 0 && ret == 0)
			ULOGW("%s: Could not read blob",
				ctx ? ctx->blob_name : "??");
		else
			ULOGD("%s: Read %s successfully",
				ctx ? ctx->blob_name : "??",
				n_quantities > 0 ? "all quantities" : "blob");
	}

	return ret;
}

int shd_end_read(struct shd_ctx *ctx, struct shd_revision *rev)
{
	int ret = -ENOSYS;

	if (ctx == NULL || rev == NULL) {
		ret = -EINVAL;
		goto error;
	}

	ret = shd_data_check_validity(ctx, rev);
	if (ret < 0) {
		/* If data validity check indicates that section revision
		 * number has changed, end read but return -ENODEV anyway */
		if (ret == -ENODEV)
			(void)shd_data_end_read(ctx);
		goto error;
	}

	ret = shd_data_end_read(ctx);
	if (ret < 0)
		goto error;

	return ret;

error:
	ULOGW("%s: Read session ended with error : %s",
			ctx ? ctx->blob_name : "??", strerror(-ret));
	return ret;
}

int shd_read_section_hdr(struct shd_ctx *ctx,
				struct shd_hdr_user_info *hdr_info,
				struct shd_revision *rev)
{
	int ret = -ENOSYS;

	if (ctx == NULL || hdr_info == NULL || rev == NULL) {
		ret = -EINVAL;
		goto exit;
	}

	ret = shd_sync_check_revision_nb(rev, ctx->sect_mmap->sync_top);
	if (ret < 0)
		goto exit;

	ret = shd_hdr_read(&ctx->id, ctx->sect_mmap->section_top, hdr_info);

exit:
	if (ret < 0)
		ULOGW("%s: Section header read ended with error : %s",
				ctx ? ctx->blob_name : "??", strerror(-ret));
	return ret;
}

int shd_read_blob_metadata_hdr(struct shd_ctx *ctx,
				void *dst,
				size_t size,
				struct shd_revision *rev)
{
	int ret = -ENOSYS;

	if (ctx == NULL || dst == NULL || rev == NULL) {
		ret = -EINVAL;
		goto exit;
	}

	if (size != shd_hdr_get_mdata_size(ctx->sect_mmap->header_top)) {
		ret = -ENOMEM;
		goto exit;
	}

	ret = shd_sync_check_revision_nb(rev, ctx->sect_mmap->sync_top);
	if (ret < 0)
		goto exit;

	ret = shd_mdata_hdr_read(ctx->sect_mmap->metadata_blob_top,
					dst,
					size);

exit:
	if (ret < 0)
		ULOGW("%s: Blob metadata header read ended with error : %s",
				ctx ? ctx->blob_name : "??", strerror(-ret));
	return ret;
}
