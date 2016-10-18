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
 * @file shd_window.h
 *
 * @brief shared memory section window management. A window is a set of
 * consecutive samples that match a given search.
 *
 */

#ifndef SHD_WINDOW_H_
#define SHD_WINDOW_H_

/*
 * Structure describing a sample window
 */
struct shd_window {
	/* Index of window's start sample */
	int start_idx;
	/* Index of window's reference sample */
	int ref_idx;
	/* Index of window's end sample */
	int end_idx;
	/* Total number of samples in the window */
	int nb_matches;
};

#include "libshdata.h"
#include "shd_sync.h"

/*
 * @brief Copy some data from a window of samples
 *
 * @param[in] window : pointer to a structure describing the window of samples
 * to read
 * @param[in] desc : pointer to a structure describing the data section
 * @param[out] dst : pointer to destination buffer
 * @param[in] data_size : size of the data to copy
 * @param[in] s_offset : offset of the data to copy, from the start of the
 * sample
 *
 * @return : number of samples that were read
 */
int shd_window_read(struct shd_window *window,
			struct shd_data_section_desc *desc,
			void *dst,
			size_t data_size,
			ptrdiff_t s_offset);

/*
 * @brief Define the current window
 *
 * @param[out] window : updated window of matching samples
 * @param[in] hdr : pointer to section synchronization header
 * @param[in] search : pointer to a structure describing the search that the
 * window should match
 * @param[in] desc : pointer to a structure describing the data section
 * @param[in] hint :  hint for sample search method to use
 *
 * @return number of matching samples in case of success,
 *         -EINVAL in case of invalid parameter
 *         -EFAULT if result window was overwritten during search,
 *         -ENOENT if no result was found
 */
int shd_window_set(struct shd_window *window,
			const struct shd_sync_hdr *hdr,
			const struct shd_sample_search *search,
			const struct shd_data_section_desc *desc,
			enum shd_ref_sample_search_hint hint);

/*
 * @brief Allocate and create a new window structure
 *
 * @return : an allocated window in case of success,
 *           NULL in case of error
 */
struct shd_window *shd_window_new(void);

/*
 * @brief Reset a given window
 *
 * @param[in] window : pointer to the window to reset
 *
 * @return : 0 in case of success,
 *           -EINVAL if argument is NULL
 *           -EPERM if window is already empty (most likely due to an
 * out-of-sequence call)
 */
int shd_window_reset(struct shd_window *window);

/*
 * @brief Destroy a window structure
 *
 * @param[in] window : pointer to the window to destroy
 *
 * @return : 0 in case of success
 */
int shd_window_destroy(struct shd_window *window);

#endif /* SHD_WINDOW_H_ */
