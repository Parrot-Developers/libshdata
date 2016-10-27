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
 * @file shd_window.c
 *
 * @brief shared memory section window management.
 *
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>			/* For memory allocation functions */
#include <errno.h>
#include <time.h>

#include "shd_data.h"
#include "shd_utils.h"
#include "shd_private.h"
#include "shd_sample.h"
#include "libshdata.h"
#include "shd_window.h"

#include "shd_search.h"

int shd_window_set(struct shd_window *window,
			const struct shd_sync_hdr *hdr,
			const struct shd_sample_search *search,
			const struct shd_data_section_desc *desc,
			enum shd_ref_sample_search_hint hint)
{
	int ref_idx = -1;
	int ret = -1;
	struct search_ctx ctx = shd_search_start(hdr, desc);

	SHD_HOOK(HOOK_WINDOW_SEARCH_START);

	ULOGD("Setting reading window using method %s for date %ld_%ld",
			shd_search_method_to_str(search->method),
			search->date.tv_sec,
			search->date.tv_nsec);

	shd_window_reset(window);

	switch (search->method) {
	case SHD_LATEST:
		ref_idx = ctx.t_index;
		break;
	case SHD_CLOSEST:
		ref_idx = shd_search_closest_match(desc, &search->date,
						&ctx, hint);
		break;
	case SHD_FIRST_AFTER:
		ref_idx = shd_search_first_match_after(desc, &search->date,
							&ctx, hint);
		break;
	case SHD_FIRST_BEFORE:
		ref_idx = shd_search_first_match_before(desc, &search->date,
							&ctx, hint);
		break;
	default:
		ULOGW("Invalid sample search method");
		ret = -EINVAL;
		goto exit;
		break;
	}

	SHD_HOOK(HOOK_WINDOW_SEARCH_OVER);

	if (ref_idx < 0) {
		ret = -ENOENT;
		goto exit;
	} else {
		/* Number of samples which have been produced after wr_index */
		int nb_more_recent_samples = interval_between(ref_idx,
							ctx.t_index,
							desc->nb_samples);

		/* Number of samples which have been produced between t_index+1
		 * and wr_index. t_index is the most recent sample. t_index+1
		 * is the oldest. */
		int nb_older_samples = interval_between(
				index_next(ctx.t_index, desc->nb_samples),
				ref_idx,
				desc->nb_samples);

		int w_start_idx = index_n_before(ref_idx,
						min(nb_older_samples,
						search->nb_values_before_date),
						desc->nb_samples);

		if (shd_search_end(hdr, &ctx, w_start_idx, desc)) {
			ULOGW("Samples window set during search has been "
				"overwritten");
			ret = -EFAULT;
			goto exit;
		} else {
			window->ref_idx = ref_idx;
			window->end_idx = index_n_after(
					ref_idx,
					min(nb_more_recent_samples,
						search->nb_values_after_date),
					desc->nb_samples);
			window->start_idx = index_n_before(
					ref_idx,
					min(nb_older_samples,
						search->nb_values_before_date),
					desc->nb_samples);
			window->nb_matches = 1 + interval_between(
							window->start_idx,
							window->end_idx,
							desc->nb_samples);
		}
	}

	ULOGD("Search ended with : nb_matches = %d, w_start = %d, "
			"w_ref = %d, w_end = %d",
			window->nb_matches,
			window->start_idx,
			window->ref_idx,
			window->end_idx);
	if (window->ref_idx >= 0) {
		struct shd_sample *r_samp = shd_data_get_sample_ptr(desc,
							window->ref_idx);
		ULOGD("Reference sample has timestamp : %ld_%ld",
				r_samp->metadata.ts.tv_sec,
				r_samp->metadata.ts.tv_nsec);
	}

	ret = window->nb_matches;
exit:
	return ret;
}

int shd_window_read(struct shd_window *window,
			struct shd_data_section_desc *desc,
			void *dst,
			size_t data_size,
			ptrdiff_t s_offset)
{
	int s_index; /* index in data section */
	int d_index; /* index in destination buffer*/
	struct shd_sample *curr = NULL;

	/*
	 * Iterate over the whole window of matching samples
	 * Due to the fact that we use a circular buffer, the source index
	 * cannot just be simply incremented but needs to be computed using a
	 * function that computes a modular addition.
	 */
	for (s_index = window->start_idx, d_index = 0;
		d_index < window->nb_matches;
		s_index = index_next(s_index, desc->nb_samples),
			d_index++) {
		char *curr_dst;
		curr = shd_data_get_sample_ptr(desc, s_index);
		curr_dst = (char *) dst + data_size * d_index;
		shd_sample_read(curr, s_offset, curr_dst, data_size);
	}

	return d_index;
}

struct shd_window *shd_window_new(void)
{
	struct shd_window *window;

	window = calloc(1, sizeof(*window));
	if (window == NULL)
		return NULL;

	shd_window_reset(window);

	return window;
}

int shd_window_reset(struct shd_window *window)
{
	if (window == NULL)
		return -EINVAL;
	if (window->nb_matches < 0)
		return -EPERM;

	window->start_idx = -1;
	window->ref_idx = -1;
	window->end_idx = -1;
	window->nb_matches = -1;

	return 0;
}

int shd_window_destroy(struct shd_window *window)
{
	free(window);
	return 0;
}
