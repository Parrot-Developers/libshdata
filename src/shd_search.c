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
 * @file shd_search.c
 *
 * @brief shared memory search methods
 *
 */

#include <stdbool.h>

#include "libshdata.h"
#include "shd_sample.h"
#include "shd_utils.h"
#include "shd_private.h"
#include "shd_search.h"

unsigned int shd_search_get_max_depth(const struct shd_data_section_desc *desc,
					const struct search_ctx *ctx)
{
	unsigned int depth;

	/* If the top sample has been written at least twice, it means that the
	 * whole section has been written at least once, and the whole
	 * section is eligible for research */
	if (ctx->nb_writes_top > 0)
		return desc->nb_samples;

	/* Else, we run through the whole buffer and stop as soon as a sample
	 * is invalid */
	for (depth = 0; depth < desc->nb_samples; depth++) {
		int idx = index_n_before(ctx->t_index, depth, desc->nb_samples);
		struct shd_sample *curr = shd_data_get_sample_ptr(desc, idx);

		if (!shd_sync_is_sample_valid(&curr->sync))
			break;
	}

	return depth;
}

static bool search_reference_sample_naive(
				const struct shd_data_section_desc *desc,
				const struct timespec *date,
				const struct search_ctx *ctx,
				int *m_index,
				uint32_t *s_searched)
{
	bool found_ref = false;
	struct shd_sample *curr;
	int max_depth = shd_search_get_max_depth(desc, ctx);
	int searched = 0;

	*m_index = ctx->t_index;

	while (searched < max_depth && !found_ref) {
		curr = shd_data_get_sample_ptr(desc, *m_index);
		if (shd_sample_timestamp_cmp(curr, *date) < 0)
			found_ref = true;
		else
			index_decrement(m_index, desc->nb_samples);
		searched++;
	}

	*s_searched = searched;
	return found_ref;
}

static bool search_reference_sample_binary(
				const struct shd_data_section_desc *desc,
				const struct timespec *date,
				const struct search_ctx *ctx,
				int *m_index,
				uint32_t *s_searched)
{
	int imin = 0, imax = 0, imid = 0, max_depth = 0;
	struct shd_sample *curr = NULL;
	int res = 0;

	/* imin, imax, imid are relative index in window */
	imin = 0;
	max_depth = shd_search_get_max_depth(desc, ctx);
	imax = max_depth - 1;

	*s_searched = 0;

	while (imin < imax) {
		imid = (imin + imax) / 2;

		/* Get real index of sample */
		*m_index = index_n_before(ctx->t_index,
					  max_depth - 1 - imid,
					  desc->nb_samples);
		curr = shd_data_get_sample_ptr(desc, *m_index);
		(*s_searched)++;

		/* reduce the search */
		res = shd_sample_timestamp_cmp(curr, *date);
		if (res < 0)
			imin = imid + 1;
		else
			imax = imid;
	}

	/* imin is the closest value, recompute only if we don't have it */
	if (imin != imid) {
		*m_index = index_n_before(ctx->t_index,
					  max_depth - 1 - imin,
					  desc->nb_samples);
		curr = shd_data_get_sample_ptr(desc, *m_index);
		res = shd_sample_timestamp_cmp(curr, *date);
	}

	if (res >= 0) {
		/* Need to get previous */
		if (imin > 0) {
			index_decrement(m_index, desc->nb_samples);
			return true;
		} else {
			/* No match */
			return false;
		}
	} else {
		/* Found it */
		return true;
	}
}

/*
 * @brief Search reference sample in the section
 *
 * @details The reference sample is the first sample whose timestamp is set
 * before the date passed as parameter, the list of samples being browsed in
 * reverse order, from the last published sample
 *
 * @param[in] desc : description of the section
 * @param[in] date : reference date
 * @param[in] ctx : context of the search
 * @param[out] m_index : index of the reference sample, if found
 * @param[out] s_searched : number of samples which were browsed in the section
 * during the search
 *
 * @return true if a reference sample was found
 *         false otherwise (meaning the timestamps of all the samples in the
 * section are set after "date")
 */
static bool search_reference_sample(const struct shd_data_section_desc *desc,
				const struct timespec *date,
				const struct search_ctx *ctx,
				int *m_index,
				uint32_t *s_searched,
				enum shd_ref_sample_search_hint hint)
{
	switch (hint) {
	case SHD_WINDOW_REF_SEARCH_NAIVE:
		return search_reference_sample_naive(desc, date,
						ctx, m_index, s_searched);
		break;
	case SHD_WINDOW_REF_SEARCH_BINARY:
		return search_reference_sample_binary(desc, date,
						ctx, m_index, s_searched);
		break;
	default:
		return search_reference_sample_naive(desc, date,
						ctx, m_index, s_searched);
		break;
	}
}

char *shd_search_method_to_str(enum shd_search_method_t method)
{
	char *str[] = {
		[SHD_LATEST] = "SHD_LATEST",
		[SHD_OLDEST] = "SHD_OLDEST",
		[SHD_CLOSEST] = "SHD_CLOSEST",
		[SHD_FIRST_AFTER] = "SHD_FIRST_AFTER",
		[SHD_FIRST_BEFORE] = "SHD_FIRST_BEFORE",
	};
	if (method <= SHD_FIRST_BEFORE)
		return str[method];
	else
		return "Unknown";
}

int shd_search_oldest(const struct shd_data_section_desc *desc,
				 const struct search_ctx *ctx)
{
	int ret_index = -1;
	unsigned int max_depth = shd_search_get_max_depth(desc, ctx);

	if (max_depth == desc->nb_samples)
		ret_index = index_n_after(ctx->t_index, 2, desc->nb_samples);
	else
		ret_index = index_n_before(ctx->t_index,
				 max_depth - 1,
				 desc->nb_samples);

	return ret_index;
}

int shd_search_first_match_after(const struct shd_data_section_desc *desc,
				 const struct timespec *date,
				 const struct search_ctx *ctx,
				 enum shd_ref_sample_search_hint hint)
{
	int m_index = -1, ret_index = -1;
	bool found_ref = false;
	uint32_t s_searched;

	if (search_reference_sample(desc, date,
					ctx, &m_index,
					&s_searched, hint)) {
		/* If the reference index is set to the top index and the
		 * function returned true, it means that all the samples are
		 * in the past of the search date : the only case where there
		 * is something to do is if the reference index is different */
		if (m_index != ctx->t_index) {
			ret_index = index_next(m_index, desc->nb_samples);
			found_ref = true;
		}
	} else {
		/* If no reference sample has been found, it means that all
		 * the samples are set after the search date, so we return
		 * the oldest sample */
		ret_index = index_next(ctx->t_index, desc->nb_samples);
		found_ref = true;
	}

	ULOGD("%s found after going through %d samples",
			(found_ref == true) ? "Some matches" : "No match",
			s_searched);

	return ret_index;
}

int shd_search_first_match_before(const struct shd_data_section_desc *desc,
				  const struct timespec *date,
				  const struct search_ctx *ctx,
				  enum shd_ref_sample_search_hint hint)
{
	int m_index = -1, ret_index = -1;
	bool found_ref = false;
	uint32_t s_searched;

	if (search_reference_sample(desc, date,
					ctx, &m_index,
					&s_searched, hint)) {
		ret_index = m_index;
		found_ref = true;
	}

	ULOGD("%s found after going through %d samples",
			(found_ref == true) ? "Some matches" : "No match",
			s_searched);

	return ret_index;
}

int shd_search_closest_match(const struct shd_data_section_desc *desc,
			     const struct timespec *date,
			     const struct search_ctx *ctx,
			     enum shd_ref_sample_search_hint hint)
{
	int b_index = -1;
	int ret_index = -1;
	uint32_t s_searched;

	if (!search_reference_sample(desc, date,
					ctx, &b_index,
					&s_searched, hint)) {
		/* All the samples are set in the future of the searched date,
		 * and so the closest sample is in fact the oldest one */
		ret_index = index_next(ctx->t_index, desc->nb_samples);
	} else if (b_index == ctx->t_index) {
		/* All the samples are set in the past of the searched date,
		* and so the closest sample is in fact the latest one */
		ret_index = ctx->t_index;
	} else {
		/* b_index has been set to the date of the sample right before
		 * the search date */
		int a_index = index_next(b_index, desc->nb_samples);

		struct shd_sample *b_s = shd_data_get_sample_ptr(desc, b_index);
		struct shd_sample *a_s = shd_data_get_sample_ptr(desc, a_index);

		/* The closest sample is either the first after of the first
		 * before the search date */
		int ret = shd_sample_closest_timestamp(b_s, a_s, *date);

		if (ret < 0)
			ret_index = a_index;
		else
			ret_index = b_index;
	}

	return ret_index;
}

struct search_ctx shd_search_start(const struct shd_sync_hdr *hdr,
				const struct shd_data_section_desc *desc)
{
	struct search_ctx ret;
	ret.t_index = shd_sync_get_last_write_index(hdr);
	struct shd_sample *top = shd_data_get_sample_ptr(desc, ret.t_index);

	ret.nb_writes_top = shd_sync_get_nb_writes(&top->sync);

	return ret;
}

bool shd_search_end(const struct shd_sync_hdr *hdr,
		const struct search_ctx *ctx,
		int w_start_idx,
		const struct shd_data_section_desc *desc)
{
	/* Index of the most recent sample, after the search */
	int t_index_new = shd_sync_get_last_write_index(hdr);
	/* Number of samples between the most recent sample at the start of the
	 * search and the start of the window */
	int margin = interval_between(index_next(ctx->t_index,
						desc->nb_samples),
					w_start_idx,
					desc->nb_samples);
	/* Number of samples that were added to the section during the start
	 * of the search and now */
	int nb_new_samples = interval_between(ctx->t_index,
						t_index_new,
						desc->nb_samples);
	struct shd_sample *w_start = shd_data_get_sample_ptr(desc, w_start_idx);
	/* Number of writes on the start of the window */
	int nb_writes_start = shd_sync_get_nb_writes(&w_start->sync);

	/*
	 * The window has been overwritten if :
	 *   - There has been new samples in the section, AND
	 *   - The start of the window is located before the new most recent
	 *   sample, AND
	 *   - The buffer slot that contains the sample at the start of the
	 *   window has been written more times than the buffer slot that
	 *   contained the most recent sample at the start of the search (for
	 *   a given buffer slot, the nb_writes is monotonously increasing
	 *   from 0 from the moment the section is created, and is updated
	 *   right at the beginning of sample write)
	 */
	return nb_new_samples > 0
			&& margin < nb_new_samples
			&& nb_writes_start >= ctx->nb_writes_top;
}
