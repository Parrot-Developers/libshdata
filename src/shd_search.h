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
 * @file shd_search.h
 *
 * @brief shared memory search methods
 *
 */

#ifndef SHD_SEARCH_H_
#define SHD_SEARCH_H_

#include "shd_data.h"
#include "shd_sync.h"

/*
 * @brief Structure to hold the context of a search
 */
struct search_ctx {
	/* Index of the most recent sample in the buffer */
	int t_index;
	/* Number of writes in the buffer slot containing the most recent
	 * sample */
	int nb_writes_top;
};

char *shd_search_method_to_str(enum shd_search_method_t method);

/*
 * @brief Search for the sample whose timestamp is right after a given date
 *
 * @param[in] desc : description of the section
 * @param[in] date : reference date
 * @param[in] ctx : context of the search
 * @param[in] hint : method to use for this search
 *
 * @return index of the sample if a match is found,
 *         -1 else
 */
int shd_search_first_match_after(const struct shd_data_section_desc *desc,
				 const struct timespec *date,
				 const struct search_ctx *ctx,
				 enum shd_ref_sample_search_hint hint);

/*
 * @brief Search for the sample whose timestamp is right before a given date
 *
 * @param[in] desc : description of the section
 * @param[in] date : reference date
 * @param[in] ctx : context of the search
 * @param[in] hint : method to use for this search
 *
 * @return index of the sample if a match is found,
 *         -1 else
 */
int shd_search_first_match_before(const struct shd_data_section_desc *desc,
				  const struct timespec *date,
				  const struct search_ctx *ctx,
				  enum shd_ref_sample_search_hint hint);

/*
 * @brief Search for the sample whose timestamp is the closest to a given date
 *
 * @param[in] desc : description of the section
 * @param[in] date : reference date
 * @param[in] ctx : context of the search
 * @param[in] hint : method to use for this search
 *
 * @return index of the sample if a match is found,
 *         -1 else
 */
int shd_search_closest_match(const struct shd_data_section_desc *desc,
			     const struct timespec *date,
			     const struct search_ctx *ctx,
			     enum shd_ref_sample_search_hint hint);

/*
 * @brief Start a search session
 *
 * @param[in] hdr : pointer to the section's synchronization header
 * @param[in] desc : pointer to the section's description
 *
 * @return the context of the section at the time of call
 */
struct search_ctx shd_search_start(const struct shd_sync_hdr *hdr,
				const struct shd_data_section_desc *desc);

/*
 * @brief End a search session
 *
 * @param[in] hdr : pointer to the synchronization header
 * @param[in] ctx : pointer to the search context at the start of the search
 * @param[in] w_start_idx : index of the start of the window
 * @param[in] desc : pointer to the description of the section
 *
 * @return true if the window was overwritten during the search,
 *         false otherwise
 */
bool shd_search_end(const struct shd_sync_hdr *hdr,
		const struct search_ctx *ctx,
		int w_start_idx,
		const struct shd_data_section_desc *desc);

#endif /* SHD_SEARCH_H_ */
