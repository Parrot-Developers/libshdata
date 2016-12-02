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
 * @file shd_data.c
 *
 * @brief shared memory data section management.
 *
 */

#define _GNU_SOURCE

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>		/* for syscall */
#include <string.h>			/* for memcpy function */
#include <stdlib.h>			/* For memory allocation functions */
#include <errno.h>
#include <time.h>
#include "shd_ctx.h"
#include "shd_sync.h"
#include "shd_data.h"
#include "shd_utils.h"
#include "shd_private.h"
#include "shd_sample.h"
#include "shd_window.h"
#include "shd_section.h"
#include "libshdata.h"

struct shd_sample *
shd_data_get_sample_ptr(const struct shd_data_section_desc *desc,
				int index)
{
	return (struct shd_sample *) ((char *)desc->data_section_start
			+ index * shd_sample_get_size(desc->blob_size));
}

int shd_data_clear_section(const struct shd_data_section_desc *desc)
{
	unsigned int i;
	memset(desc->data_section_start,
		0,
		shd_data_get_total_size(desc->blob_size, desc->nb_samples));
	for (i = 0; i < desc->nb_samples; i++) {
		struct shd_sample *samp = shd_data_get_sample_ptr(desc, i);
		shd_sync_invalidate_sample(&samp->sync);
	}

	return 0;
}

int shd_data_reserve_write(struct shd_ctx *ctx)
{
	int ret;
	int index;
	struct shd_sample *curr_sample;

	ret = shd_sync_start_write_session(ctx->sync_ctx,
						ctx->sect_mmap->sync_top);

	if (ret < 0)
		return ret;

	index = shd_sync_get_next_write_index(ctx->sect_mmap->sync_top,
						ctx->desc);
	curr_sample = shd_data_get_sample_ptr(ctx->desc,
						index);

	ret = shd_sync_start_sample_write(ctx->sync_ctx,
						ctx->sect_mmap->sync_top,
						&curr_sample->sync,
						ctx->desc);

	if (ret < 0)
		return ret;

	return 0;
}

int shd_data_write_metadata(struct shd_ctx *ctx,
				const struct shd_sample_metadata *metadata)
{
	struct shd_sample *curr_sample;
	int index = shd_sync_get_local_write_index(ctx->sync_ctx);
	if (index == -1)
		return -EPERM;

	curr_sample = shd_data_get_sample_ptr(ctx->desc,
						index);

	return shd_sample_write(curr_sample,
				0,
				metadata,
				sizeof(*metadata));
}

int shd_data_write_quantity(struct shd_ctx *ctx,
				const struct shd_quantity *quantity,
				const void *src)
{
	struct shd_sample *curr_sample;
	ptrdiff_t blob_offset;
	int index = shd_sync_get_local_write_index(ctx->sync_ctx);
	if (index == -1)
		return -EPERM;


	curr_sample = shd_data_get_sample_ptr(ctx->desc,
						index);

	blob_offset = offsetof(struct shd_sample, blob);

	return shd_sample_write(curr_sample,
				blob_offset + quantity->quantity_offset,
				src,
				quantity->quantity_size);
}

int shd_data_end_write(struct shd_ctx *ctx)
{
	int index = shd_sync_get_local_write_index(ctx->sync_ctx);
	if (index == -1)
		return -EPERM;

	return shd_sync_end_write_session(ctx->sync_ctx,
						ctx->sect_mmap->sync_top);
}

int shd_data_find(struct shd_ctx *ctx,
			const struct shd_sample_search *search)
{
	int ret;
	struct shd_sample *wstart;
	int t_index;

	t_index = shd_sync_get_last_write_index(ctx->sect_mmap->sync_top);
	if (t_index == -1)
		return -EAGAIN;
	/* @todo EAGAIN should also be returned if there hasn't been enough
	 * writes to the shared memory section yet
	 */

	if ((search->nb_values_after_date + search->nb_values_before_date + 1)
			> ctx->desc->nb_samples)
		return -EINVAL;

	ret = shd_window_set(ctx->window, ctx->sect_mmap->sync_top,
				search, ctx->desc,
				ctx->hint);

	if (ret < 0) {
		return ret;
	} else {
		wstart = shd_data_get_sample_ptr(ctx->desc,
							ctx->window->start_idx);
		shd_sync_start_read_session(ctx->sync_ctx, &wstart->sync);
		return ctx->window->nb_matches;
	}
}

int shd_data_read_metadata(struct shd_ctx *ctx,
				struct shd_sample_metadata **metadata)
{
	int ret = -1;

	if (ctx->window == NULL) {
		ret = -EPERM;
		goto exit;
	}

	*metadata = calloc(ctx->window->nb_matches,
				sizeof(struct shd_sample_metadata));
	if (*metadata == NULL) {
		ret = -ENOMEM;
		goto exit;
	}

	shd_window_read(ctx->window, ctx->desc, *metadata,
			sizeof(struct shd_sample_metadata),
			offsetof(struct shd_sample, metadata));

	ret = interval_between(ctx->window->start_idx,
				ctx->window->ref_idx,
				ctx->desc->nb_samples);

	ctx->metadata = *metadata;

exit:
	return ret;
}

int shd_data_read_blob(struct shd_ctx *ctx, void *dst, size_t dst_size)
{
	int ret = -ENOSYS;
	size_t req_size;

	if (ctx->window->nb_matches < 0) {
		ret = -EPERM;
		goto exit;
	}

	req_size = ctx->desc->blob_size * ctx->window->nb_matches;
	if (dst_size < req_size) {
		ret = -EINVAL;
		goto exit;
	}

	ret = shd_window_read(ctx->window, ctx->desc, dst,
				ctx->desc->blob_size,
				offsetof(struct shd_sample, blob));

exit:
	return ret;
}

int shd_data_read_quantity(struct shd_ctx *ctx,
				const struct shd_quantity *quantity,
				void *dst,
				size_t dst_size)
{
	int ret = -ENOSYS;
	size_t req_size;

	if (ctx->window->nb_matches < 0) {
		ret = -EPERM;
		goto exit;
	}

	req_size = quantity->quantity_size * ctx->window->nb_matches;
	if (dst_size < req_size) {
		ret = -EINVAL;
		goto exit;
	}

	ret = shd_window_read(ctx->window, ctx->desc, dst,
				quantity->quantity_size,
				offsetof(struct shd_sample, blob)
					+ quantity->quantity_offset);
exit:
	return ret;
}

int shd_data_read_quantity_sample(struct shd_ctx *ctx,
				int n_quantities,
				const struct shd_quantity quantity[],
				struct shd_quantity_sample qty_samples[])
{
	int ret = 0;
	int q_idx = 0;
	int s_index;
	struct shd_sample *curr;

	if (ctx->window->nb_matches < 0) {
		ret = -EPERM;
		goto exit;
	}
	if (ctx->window->nb_matches != 1) {
		ret = -EINVAL;
		goto exit;
	}

	s_index = ctx->window->ref_idx;
	curr = shd_data_get_sample_ptr(ctx->desc, s_index);

	for (q_idx = 0; q_idx < n_quantities; q_idx++) {
		/* Check that the destination buffer is large enough */
		if (quantity[q_idx].quantity_size
				<= qty_samples[q_idx].size) {
			/* Copy quantity data */
			shd_sample_read(curr,
					quantity[q_idx].quantity_offset
						+ offsetof(struct shd_sample,
								blob),
					qty_samples[q_idx].ptr,
					qty_samples[q_idx].size);
			/* Copy quantity metadata */
			shd_sample_read(curr,
					offsetof(struct shd_sample, metadata),
					&qty_samples[q_idx].meta,
					sizeof(struct shd_sample_metadata));

			ret++;
		}
	}

exit:
	return ret;
}

int shd_data_check_validity(struct shd_ctx *ctx, struct shd_revision *rev)
{
	struct shd_sample *w_start;
	int ret;

	if (ctx->window->start_idx >= 0)
		w_start = shd_data_get_sample_ptr(ctx->desc,
							ctx->window->start_idx);
	else
		return -EPERM;

	ret = shd_sync_end_read_session(ctx->sync_ctx, &w_start->sync);

	if (ret < 0)
		return ret;

	ret = shd_sync_check_revision_nb(rev,
					ctx->sect_mmap->sync_top);

	return ret;
}

int shd_data_end_read(struct shd_ctx *ctx)
{
	int ret;

	ret = shd_window_reset(ctx->window);
	if (ret < 0) {
		ret = -EPERM;
		goto exit;
	}

exit:
	free(ctx->metadata);
	return 0;
}

size_t shd_data_get_total_size(size_t blob_size,
				uint32_t max_nb_samples)
{
	return max_nb_samples * shd_sample_get_size(blob_size);
}

struct shd_data_section_desc *shd_data_section_desc_new(struct shd_ctx *ctx,
				const struct shd_hdr_user_info *hdr_info)
{
	struct shd_data_section_desc *desc;
	const struct shd_hdr_user_info *user_info;

	/* Allocate context structure */
	desc = calloc(1, sizeof(*desc));
	if (desc == NULL)
		return NULL;

	desc->data_section_start = ctx->sect_mmap->data_top;
	if (hdr_info == NULL)
		user_info = ctx->sect_mmap->header_top;
	else
		user_info = hdr_info;
	desc->blob_size = user_info->blob_size;
	desc->nb_samples = user_info->max_nb_samples;

	return desc;
}

