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
 * @file shd_ctx.h
 *
 * @brief shared memory section header management.
 *
 */

#ifndef _SHD_HDR_H_
#define _SHD_HDR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "shd_sync.h"
#include "shd_section.h"
#include "libshdata.h"

struct shd_hdr {
	/* Magic number */
	uint64_t magic_number;
	/* Library version major */
	uint32_t lib_version_maj;
	/* Library version minor */
	uint32_t lib_version_min;
	/* user-defined info */
	struct shd_hdr_user_info user_info;
	/* sync-related info */
	struct shd_sync_hdr sync_info;
};

/*
 * @brief Write section header into shared memory
 *
 * Segmentation fault will occur if the process has no write access to the
 * address pointed by hdr_start
 *
 * @param[in] hdr_start : pointer to the start of the shared memory header
 * @param[in] hdr : pointer to the memory structure describing the header
 *
 * @return : 0 if the header that was already present in the memory section
 * was already filled with the new header value,
 *           -1 otherwise
 */
int shd_hdr_write(void *hdr_start, const struct shd_hdr_user_info *hdr);

/*
 * @brief Copy user info from header of shared memory section
 *
 * If the section is already m'mapped into memory, hdr_start argument must be
 * non null and point to the top of the shared memory section.
 * Else, hdr_start argument is discarded and a file descriptor to the open
 * shared memory section must be provided.
 *
 * @param[in] id : identifier of the open shared memory section
 * @param[in] hdr_start : pointer to the start of the shared memory header
 * @param[out] hdr_user : pointer to a user-allocated destination buffer for
 * the user info
 *
 * @return : 0 in case of success,
 *           -EINVAL if the arguments are invalid
 *           -ENOMEM if the header could not be mapped
 *           -EFAULT if the shared memory section was created using another
 * library version, or the mapped memory section is not a shared memory section
 */
int shd_hdr_read(const struct shd_section_id *id,
			void *hdr_start,
			struct shd_hdr_user_info *hdr_user);

/*
 * @brief Get size of metadata header
 *
 * @param[in] hdr_start : pointer to the start of the shared memory header
 *
 * @return : size of metadata header in case of success
 */
size_t shd_hdr_get_mdata_size(void *hdr_start);

#ifdef __cplusplus
}
#endif

#endif /* _SHD_HDR_H_ */
