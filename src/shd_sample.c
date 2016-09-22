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
 * @file shd_sample.c
 *
 * @brief shared memory sample management
 *
 */

#include <stddef.h>
#include <string.h>			/* for memcpy function */
#include "futils/timetools.h"
#include "shd_utils.h"
#include "shd_private.h"
#include "shd_sample.h"

size_t shd_sample_get_size(size_t blob_size)
{
	return ALIGN_UP(offsetof(struct shd_sample, blob) + blob_size);
}

int shd_sample_read(struct shd_sample *sample,
				ptrdiff_t offset,
				void *dst,
				size_t length)
{
	memcpy(dst, (char *)sample + offset, length);
	return 0;
}

int shd_sample_write(struct shd_sample *sample,
				ptrdiff_t offset,
				const void *src,
				size_t length)
{
	memcpy((char *)sample + offset, src, length);
	return 0;
}

int shd_sample_timestamp_cmp(struct shd_sample *sample,
				struct timespec date)
{
	return time_timespec_cmp(&sample->metadata.ts, &date);
}

int shd_sample_closest_timestamp(struct shd_sample *before,
					struct shd_sample *after,
					struct timespec date)
{
	int ret;
	struct timespec duration_before;
	struct timespec duration_after;

	ret = time_timespec_diff(&before->metadata.ts,
					&date,
					&duration_before);
	if (ret < 0)
		goto exit;

	ret = time_timespec_diff(&date,
					&after->metadata.ts,
					&duration_after);
	if (ret < 0)
		goto exit;

	/* The comparison returns 1 if "duration_after" is bigger than
	 * "duration_before", meaning "before" is closer from date than "after"
	 */
	ret = time_timespec_cmp(&duration_after, &duration_before);

exit:
	return ret;
}

