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
 * @file shd_ctx.h
 *
 * @brief shared memory data context management.
 *
 */

#ifndef _SHD_CTX_H_
#define _SHD_CTX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "shd_section.h"
#include "libshdata.h"

struct shd_ctx {
	/* shared memory section identifier */
	struct shd_section_id id;
	/* full name of the shared memory section */
	char *blob_name;
	/* synchronization-related context */
	struct shd_sync_ctx *sync_ctx;
	/* structure of the shared memory data section */
	struct shd_data_section_desc *desc;
	/* M'maped pointers to the shared memory section */
	struct shd_section *sect_mmap;
	/* current window of matching samples */
	struct shd_window *window;
	/* Favored method of search, setup by environment variable */
	enum shd_ref_sample_search_hint hint;
	/* Pointer to the library-allocated metadata */
	struct shd_sample_metadata *metadata;
};

/*
 * @brief Allocate and create a new shared memory context
 *
 * @param[in] id : file descriptor to the open shared memory section that
 * should be associated to that context
 * @param[in] blob_name : name of the blob within the shared memory section
 *
 * @return : an allocated shared memory context,
 *           NULL in case of error
 */
struct shd_ctx *shd_ctx_new(struct shd_section_id *id, const char *blob_name);

/*
 * @brief M'map shared memory section into context
 *
 * @param[in] ctx : context to m'map
 * @param[in] hdr_info : pointer to the header structure that contains all info
 * regarding the shared memory section
 *
 * @return : 0 in case of success,
 *           -EINVAL if ctx is NULL
 *           -EFAULT if a fault is detected while m'maping
 */
int shd_ctx_mmap(struct shd_ctx *ctx,
			const struct shd_hdr_user_info *hdr_info);

/*
 * @brief Destroys a shared memory context
 *
 * @param[in] ctx : context to destroy
 *
 * @return : 0 in case of success
 */
int shd_ctx_destroy(struct shd_ctx *ctx);


#ifdef __cplusplus
}
#endif

#endif /* _SHD_CTX_H_ */
