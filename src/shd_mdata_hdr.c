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
 * @file shd_mdata_hdr.c
 *
 * @brief shared memory metadata header management.
 *
 */

#include <string.h>		/* for memcpy function */
#include <errno.h>
#include "shd_private.h"
#include "shd_mdata_hdr.h"

int shd_mdata_hdr_write(void *mdata_hdr_start,
				const void *src,
				size_t mdata_size)
{
	int ret = memcmp(mdata_hdr_start, src, mdata_size);

	if (ret != 0) {
		ULOGI("Writing a new metadata header into memory section");
		memcpy(mdata_hdr_start, src, mdata_size);
		ret = -1;
	} else {
		ULOGI("New metadata header matches the one already present in "
				"shared memory");
		ret = 0;
	}
	return ret;
}

int shd_mdata_hdr_read(void *mdata_hdr_start, void *dst, size_t dst_size)
{
	if (dst == NULL)
		return -EINVAL;

	memcpy(dst, mdata_hdr_start, dst_size);

	return 0;
}
