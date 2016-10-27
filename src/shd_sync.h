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
 * @file shd_sync.h
 *
 * @brief shared memory synchronization data management
 *
 */

#ifndef _SHD_SYNC_H_
#define _SHD_SYNC_H_

#include <stdint.h>
#include <stdbool.h>

struct shd_revision {
	/*
	 * revision number of the memory section : an odd value indicates that
	 * the section is being rewritten, an even value indicates twice the
	 * number of times since the start of the system that the memory
	 * section has been created
	 * @todo update this value only if the metadata related to the memory
	 * section has changed (so that a failure of a producer does not
	 * necessarily make the consumers remap the section)
	 */
	int nb_creations;
};

/*
 * @brief Header-level synchronization data
 */
struct shd_sync_hdr {
	/* revision of the memory section */
	struct shd_revision revision;
	/* index of currently written buffer slot */
	int write_index;
	/* TID of the current writer : used for synchronization purposes */
	int wtid;
};

/*
 * @brief Synchronization primitives required by the library
 */
struct shd_sync_primitives {
	bool (*compare_and_swap) (int *ptr, int oldval, int newval);
	int (*add_and_fetch) (int *ptr, int value);
};

/*
 * @brief Context-level synchronization data
 */
struct shd_sync_ctx {
	/* revision of the memory section as seen by the context when
	 * the section was open */
	struct shd_revision revision;
	/* index of the reference buffer slot as last seen by the caller (-1
	 * means there is no operation in progress) */
	int index;
	/* index of the buffer slot that was last written (-1 if no write
	 * operation has occurred yet) */
	int prev_index;
	/* TID of the current writer (-1 means there is no operation in
	 * progress) */
	int wtid;
	/* number of writes to the reference memory slot as last seen by the
	 * caller */
	int nb_writes;
	/* Primitives to use on the section */
	struct shd_sync_primitives primitives;
};

/*
 * @brief Sample-level synchronization data
 */
struct shd_sync_sample {
	/* number of writes to the memory slot currently in use for the
	 * sample
	 */
	int nb_writes;
};

#include "shd_data.h"
#include "shd_section.h"

/*
 * @brief Init synchronization header
 *
 * @param[in,out] sync_hdr : pointer to the header to init
 *
 * @return : 0 in case of success,
 *           -errno in case of error
 */
int shd_sync_hdr_init(struct shd_sync_hdr *sync_hdr);

/*
 * @brief Allocate and create a new sync context
 *
 * @param[in] section_type : Type of section
 *
 * @return : an allocated sync context,
 *           NULL in case of error
 */
struct shd_sync_ctx *shd_sync_ctx_new(enum shd_section_type section_type);

/*
 * @brief Destroy a sync context
 *
 * @param[in,out] ctx : sync context to destroy
 *
 * @return : an allocated sync context,
 *           NULL in case of error
 */
int shd_sync_ctx_destroy(struct shd_sync_ctx *ctx);

/*
 * @brief Update current writer
 *
 * @param[in,out] ctx : current synchronization context
 * @param[in,out] hdr : pointer to the synchronization part of the section
 * header
 * @param[in] tid : thread ID of the writer
 *
 * @return : 0 in case of success,
 *           -EALREADY if an operation from the same thread is already in
 *           progress
 *           -EPERM if another thread is currently writing on this memory
 *           section,
 */

int shd_sync_update_writer(struct shd_sync_ctx *ctx,
				struct shd_sync_hdr *hdr,
				int tid);

/*
 * @brief Start write session on a new sample
 *
 * @param[in,out] ctx : current synchronization context
 * @param[in,out] hdr : pointer to the synchronization part of the section
 * header
 * @param[in,out] samp : pointer to the synchronization part of the sample
 * @param[in] desc : description of the data section
 *
 * @return : 0 in case of success,
 *           -EINVAL if arguments are invalid,
 *           -EALREADY if an operation is already in progress,
 *           -EFAULT if at least one write occurred outside of the current
 * context, indicating a severe fault
 */
int shd_sync_start_write_session(struct shd_sync_ctx *ctx,
				struct shd_sync_hdr *hdr,
				struct shd_sync_sample *samp,
				const struct shd_data_section_desc *desc);

/*
 * @brief End write session on a sample
 *
 * @param[in,out] ctx : current synchronization context
 * @param[in,out] hdr : pointer to the synchronization part of the section
 * header
 *
 * @return : 0 in case of success,
 *           -EINVAL if arguments are invalid,
 *           -EALREADY if an operation is already in progress
 */
int shd_sync_end_write_session(struct shd_sync_ctx *ctx,
					struct shd_sync_hdr *hdr);

/*
 * @brief Start read session on a sample (most likely the sample at the start
 * of a reading window)
 *
 * @param[in,out] ctx : current synchronization context
 * @param[in,out] samp : pointer to the synchronization part of the sample
 *
 * @return : 0 in case of success,
 *           -EINVAL if arguments are invalid,
 *           -EALREADY if an operation is already in progress
 */
int shd_sync_start_read_session(struct shd_sync_ctx *ctx,
					struct shd_sync_sample *samp);
/*
 * @brief End read session on a sample
 *
 * @param[in,out] ctx : current synchronization context
 * @param[in,out] samp : pointer to the synchronization part of the sample
 *
 * @return : 0 in case of success,
 *           -EINVAL if arguments are invalid,
 *           -EFAULT if read sample(s) was/were overwritten during last reading
 * sequence
 */
int shd_sync_end_read_session(struct shd_sync_ctx *ctx,
					struct shd_sync_sample *samp);

/*
 * @brief Get number of writes on a given sample
 *
 * @param[in] samp : Pointer to the sample
 *
 * @return : number of writes on the sample
 */
int shd_sync_get_nb_writes(const struct shd_sync_sample *samp);

/*
 * @brief Invalidate given sample
 *
 * @param[in] samp : pointer to the sample
 *
 * @return : 0 in all cases
 */
int shd_sync_invalidate_sample(struct shd_sync_sample *samp);

/*
 * @brief Indicates validity of a sample
 *
 * @param[in] samp : pointer to the sample
 *
 * @return : true if the sample is valid,
 *           false otherwise
 */
bool shd_sync_is_sample_valid(const struct shd_sync_sample *samp);

/*
 * @brief Invalidate a data section
 *
 * @param[in,out] ctx : current synchronization context
 * @param[in] hdr : synchronization header for that section
 * @param[in] first_creation : boolean indicating whether the section has just
 * been created
 *
 * @return : 0 in case of success
 */
int shd_sync_invalidate_section(struct shd_sync_ctx *ctx,
					struct shd_sync_hdr *hdr,
					bool creation);

/*
 * @brief Update global data section revision number
 *
 * @todo update revision number only if the section or data format has changed
 *
 * @param[in,out] ctx : current synchronization context
 * @param[in] hdr : synchronization header for that section
 *
 * @return : new revision number in case of success,
 *           -EFAULT if the revision number is incoherent
 */
int shd_sync_update_global_revision_nb(struct shd_sync_ctx *ctx,
					struct shd_sync_hdr *hdr);

/*
 * @brief Update local revision number in the current context
 *
 * @param[in] ctx : current synchronization context
 * @param[in] hdr : synchronization header for that section
 *
 * @return : current revision number in case of success,
 *           -EAGAIN if the section is being updated by a producer
 */
int shd_sync_update_local_revision_nb(struct shd_sync_ctx *ctx,
					struct shd_sync_hdr *hdr);

/*
 * @brief Check whether context and global data section revision number match
 *
 * @param[in] rev : pointer to the revision structure that was output when
 * section was open
 * @param[in] hdr : synchronization header for that section
 *
 * @return : 0 in case of success,
 *           -ENODEV if the section revision number has changed
 */
int shd_sync_check_revision_nb(struct shd_revision *rev,
					struct shd_sync_hdr *hdr);

/*
 * @brief get write index as seen by current context
 *
 * @param[in] ctx : current synchronization context
 *
 * @return : write index in case of success,
 *           -1 if index or arguments are invalid
 */
int shd_sync_get_local_write_index(const struct shd_sync_ctx *ctx);

/*
 * @brief get next write index
 *
 * @param[in] hdr : pointer to the header of the memory section
 * @param[in] desc : description of the data section
 *
 * @return :next write index in case of success,
 *           -1 if index or arguments are invalid
 */
int shd_sync_get_next_write_index(const struct shd_sync_hdr *hdr,
				const struct shd_data_section_desc *desc);

/*
 * @brief get last write index
 *
 * @param[in] hdr : pointer to the header of the memory section
 *
 * @return : write index in case of success,
 *           -1 if index or arguments are invalid
 */
int shd_sync_get_last_write_index(const struct shd_sync_hdr *hdr);


#endif /* _SHD_SYNC_H_ */
