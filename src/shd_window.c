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

static char *method_to_str(enum shd_search_method_t method)
{
	char *str[] = {
		[SHD_LATEST] = "SHD_LATEST",
		[SHD_CLOSEST] = "SHD_CLOSEST",
		[SHD_FIRST_AFTER] = "SHD_FIRST_AFTER",
		[SHD_FIRST_BEFORE] = "SHD_FIRST_BEFORE",
	};
	if (method <= SHD_FIRST_BEFORE)
		return str[method];
	else
		return "Unknown";
}

static bool search_reference_sample_naive(
				const struct shd_data_section_desc *desc,
				struct timespec date,
				int t_index,
				int *m_index,
				uint32_t *s_searched)
{
	bool found_ref = false;
	struct shd_sample *curr;

	*m_index = t_index;
	*s_searched = 0;

	while (*s_searched <= desc->nb_samples && !found_ref) {
		curr = shd_data_get_sample_ptr(desc, *m_index);
		if (shd_sample_timestamp_cmp(curr, date) < 0)
			found_ref = true;
		else
			index_decrement(m_index, desc->nb_samples);
		(*s_searched)++;
	}
	return found_ref;
}

static bool search_reference_sample_binary(
				const struct shd_data_section_desc *desc,
				struct timespec date,
				int t_index,
				int *m_index,
				uint32_t *s_searched)
{
	int imin = 0, imax = 0, imid = 0;
	struct shd_sample *curr = NULL;
	int res = 0;

	/* imin, imax, imid are relative index in window */
	imin = 0;
	imax = desc->nb_samples - 1;

	*s_searched = 0;

	while (imin < imax) {
		imid = (imin + imax) / 2;

		/* Get real index of sample */
		*m_index = index_n_before(t_index, desc->nb_samples - 1 - imid,
				desc->nb_samples);
		curr = shd_data_get_sample_ptr(desc, *m_index);
		(*s_searched)++;

		/* reduce the search */
		res = shd_sample_timestamp_cmp(curr, date);
		if (res < 0)
			imin = imid + 1;
		else
			imax = imid;
	}

	/* imin is the closest value, recompute only if we don't have it */
	if (imin != imid) {
		*m_index = index_n_before(t_index, desc->nb_samples - 1 - imin,
				desc->nb_samples);
		curr = shd_data_get_sample_ptr(desc, *m_index);
		res = shd_sample_timestamp_cmp(curr, date);
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
 * @param[in] t_index : index of the last published sample
 * @param[out] m_index : index of the reference sample, if found
 * @param[out] s_searched : number of samples which were browsed in the section
 * during the search
 *
 * @return true if a reference sample was found
 *         false otherwise (meaning the timestamps of all the samples in the
 * section are set after "date")
 */
static bool search_reference_sample(const struct shd_data_section_desc *desc,
				struct timespec date,
				int t_index,
				int *m_index,
				uint32_t *s_searched,
				enum shd_ref_sample_search_hint hint)
{
	switch (hint) {
	case SHD_WINDOW_REF_SEARCH_NAIVE:
		return search_reference_sample_naive(desc, date,
						t_index, m_index, s_searched);
		break;
	case SHD_WINDOW_REF_SEARCH_BINARY:
		return search_reference_sample_binary(desc, date,
						t_index, m_index, s_searched);
		break;
	default:
		return search_reference_sample_naive(desc, date,
						t_index, m_index, s_searched);
		break;
	}
}

static int search_first_match_after(const struct shd_data_section_desc *desc,
				const struct shd_sample_search *search,
				int t_index,
				enum shd_ref_sample_search_hint hint)
{
	int m_index = -1, ret_index = -1;
	bool found_ref = false;
	uint32_t s_searched;

	if (search_reference_sample(desc, search->date,
					t_index, &m_index,
					&s_searched, hint)) {
		/* If the reference index is set to the top index and the
		 * function returned true, it means that all the samples are
		 * in the past of the search date : the only case where there
		 * is something to do is if the reference index is different */
		if (m_index != t_index) {
			ret_index = index_next(m_index, desc->nb_samples);
			found_ref = true;
		}
	} else {
		/* If no reference sample has been found, it means that all
		 * the samples are set after the search date, so we return
		 * the oldest sample */
		ret_index = index_next(t_index, desc->nb_samples);
		found_ref = true;
	}

	ULOGD("%s found after going through %d samples",
			(found_ref == true) ? "Some matches" : "No match",
			s_searched);

	return ret_index;
}

static int search_first_match_before(const struct shd_data_section_desc *desc,
				const struct shd_sample_search *search,
				int t_index,
				enum shd_ref_sample_search_hint hint)
{
	int m_index = -1, ret_index = -1;
	bool found_ref = false;
	uint32_t s_searched;

	if (search_reference_sample(desc, search->date,
					t_index, &m_index,
					&s_searched, hint)) {
		ret_index = m_index;
		found_ref = true;
	}

	ULOGD("%s found after going through %d samples",
			(found_ref == true) ? "Some matches" : "No match",
			s_searched);

	return ret_index;
}

static int search_closest_match(const struct shd_data_section_desc *desc,
				const struct shd_sample_search *search,
				int t_index,
				enum shd_ref_sample_search_hint hint)
{
	int b_index = -1;
	int ret_index = -1;
	uint32_t s_searched;

	if (!search_reference_sample(desc, search->date,
					t_index, &b_index,
					&s_searched, hint)) {
		/* All the samples are set in the future of the searched date,
		 * and so the closest sample is in fact the oldest one */
		ret_index = index_next(t_index, desc->nb_samples);
	} else if (b_index == t_index) {
		/* All the samples are set in the past of the searched date,
		* and so the closest sample is in fact the latest one */
		ret_index = t_index;
	} else {
		/* b_index has been set to the date of the sample right before
		 * the search date */
		int a_index = index_next(b_index, desc->nb_samples);

		struct shd_sample *b_s = shd_data_get_sample_ptr(desc, b_index);
		struct shd_sample *a_s = shd_data_get_sample_ptr(desc, a_index);

		/* The closest sample is either the first after of the first
		 * before the search date */
		int ret = shd_sample_closest_timestamp(b_s, a_s, search->date);

		if (ret < 0)
			ret_index = a_index;
		else
			ret_index = b_index;
	}

	return ret_index;
}


int shd_window_set(struct shd_window *window,
			const struct shd_sample_search *search,
			const struct shd_data_section_desc *desc,
			int t_index,
			enum shd_ref_sample_search_hint hint)
{
	int ref_idx = -1;
	int ret = -1;

	SHD_HOOK(HOOK_WINDOW_SEARCH_START);

	ULOGD("Setting reading window using method %s for date %ld_%ld",
			method_to_str(search->method),
			search->date.tv_sec,
			search->date.tv_nsec);

	shd_window_reset(window);

	switch (search->method) {
	case SHD_LATEST:
		ref_idx = t_index;
		break;
	case SHD_CLOSEST:
		ref_idx = search_closest_match(desc, search,
							t_index, hint);
		break;
	case SHD_FIRST_AFTER:
		ref_idx = search_first_match_after(desc, search,
							t_index, hint);
		break;
	case SHD_FIRST_BEFORE:
		ref_idx = search_first_match_before(desc, search,
							t_index, hint);
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
							t_index,
							desc->nb_samples);

		/* Number of samples which have been produced between t_index+1
		 * and wr_index. t_index is the most recent sample. t_index+1
		 * is the oldest. */
		int nb_older_samples = interval_between(
				index_next(t_index, desc->nb_samples),
				ref_idx,
				desc->nb_samples);

		window->ref_idx = ref_idx;
		window->end_idx = index_n_after(ref_idx,
					min(nb_more_recent_samples,
						search->nb_values_after_date),
					desc->nb_samples);
		window->start_idx = index_n_before(ref_idx,
					min(nb_older_samples,
						search->nb_values_before_date),
					desc->nb_samples);
		window->nb_matches = interval_between(window->start_idx,
							window->end_idx,
							desc->nb_samples) + 1;
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
