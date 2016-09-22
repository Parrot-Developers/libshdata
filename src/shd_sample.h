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
 * @file shd_sample.h
 *
 * @brief shared memory sample management
 *
 */

#ifndef SHD_SAMPLE_H_
#define SHD_SAMPLE_H_

#include <stddef.h>
#include "shd_sync.h"
#include "libshdata.h"

/* @todo this structure should certainly not need to be exposed in this
 * header */
struct shd_sample {
	struct shd_sample_metadata metadata;
	struct shd_sync_sample sync;
	void *blob;
};

/*
 * @brief Get the total size of a sample
 *
 * @param[in] blob_size : size of the blob contained in the sample
 *
 * @return : size of the sample
 */
size_t shd_sample_get_size(size_t blob_size);

/*
 * @brief Read some data from a given sample
 *
 * @param[in] sample : pointer to the sample to read from
 * @param[in] offset : offset within the sample at which data should be read
 * @param[in] dst : destination buffer for the data
 * @param[in] length : length of the data to copy
 *
 * @return : 0 in any case
 */
int shd_sample_read(struct shd_sample *sample,
				ptrdiff_t offset,
				void *dst,
				size_t length);

/*
 * @brief Write some data into a given sample
 *
 * @param[in] sample : pointer to the sample to read from
 * @param[in] offset : offset within the sample at which data should be written
 * @param[in] src : data to copy
 * @param[in] length : length of the data to copy
 *
 * @return : 0 in any case
 */
int shd_sample_write(struct shd_sample *sample,
				ptrdiff_t offset,
				const void *src,
				size_t length);

/*
 * @brief Compare a sample timestamp against a given date
 *
 * @param[in] sample : pointer to the sample to read
 * @param[in] date : date against which to compare the sample
 *
 * @return 0 if sample timestamp equals date
 *         -1 if sample timestamp is before date
 *         1 if sample timestamp is after date
 */
int shd_sample_timestamp_cmp(struct shd_sample *sample,
				struct timespec date);

/*
 * @brief Out of 2 samples, return which one is closer to a given date
 *
 * @param[in] s1 : pointer to the first sample
 * @param[in] s2 : pointer to the second sample
 * @param[in] date : reference date
 *
 * @return 0 if s1 and s2 are equally close from date
 *         1 if s1 is closer to date than s2
 *         -1 if s2 is closer to date than s1
 */
int shd_sample_closest_timestamp(struct shd_sample *s1,
					struct shd_sample *s2,
					struct timespec date);

#endif /* SHD_SAMPLE_H_ */
