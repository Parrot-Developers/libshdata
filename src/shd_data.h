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
 * @file shd_data.h
 *
 * @brief shared memory data section management.
 *
 */

#ifndef _SHD_DATA_H_
#define _SHD_DATA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

struct shd_data_section_desc {
	/* Start of data section (pointer to the first sample) */
	void *data_section_start;
	/* Size of a blob */
	size_t blob_size;
	/* Total number of samples */
	uint32_t nb_samples;
};

#include "libshdata.h"

/*
 * @brief Get pointer to sample
 *
 * @param[in] desc : description of the data section
 * @param[in] index : index of the sample
 *
 * @return : pointer to the sample
 */
struct shd_sample *
shd_data_get_sample_ptr(const struct shd_data_section_desc *desc,
				int index);

/*
 * @brief Clear a given memory section (setting it all to zero)
 *
 * @param[in] id : id of the section to clear
 *
 * @return : 0 in case of success
 */
int shd_data_clear_section(const struct shd_data_section_desc *desc);

/*
 * @brief Reserve write in a new data slot
 *
 * @param[in,out] ctx : current shared memory context
 *
 * @return : 0 in case of success,
 *           -EINVAL if arguments are invalid,
 *           -EALREADY if an operation is already in progress,
 *           -EFAULT if at least one write occurred outside of the current
 * context, indicating a severe fault
 */
int shd_data_reserve_write(struct shd_ctx *ctx);

/*
 * @brief Write metadata for the current data slot
 *
 * @param[in,out] ctx : current shared memory context
 * @param[in] metadata : metadata to write
 *
 * @return : 0 in case of success,
 *           -EINVAL if arguments are invalid,
 *           -EALREADY if an operation is already in progress,
 *           -EPERM if the operation is not permitted (out-of-sequence call)
 */
int shd_data_write_metadata(struct shd_ctx *ctx,
				const struct shd_sample_metadata *metadata);

/*
 * @brief Write quantity into the current data slot
 *
 * @param[in,out] ctx : current shared memory context
 * @param[in] quantity : quantity to write
 * @param[in] src : quantity source
 *
 * @return : 0 in case of success,
 *           -EPERM if the operation is not permitted (out-of-sequence call)
 */
int shd_data_write_quantity(struct shd_ctx *ctx,
				const struct shd_quantity *quantity,
				const void *src);

/*
 * @brief End write operation to current data slot
 *
 * @param[in,out] ctx : current shared memory context
 *
 * @return : 0 in case of success,
 *           -EPERM if the operation is not permitted
 */
int shd_data_end_write(struct shd_ctx *ctx);

/*
 * @brief Find the set of samples that match a given search
 *
 * @param[in,out] ctx : current shared memory context
 * @param[in] search : search parameters
 *
 * @return : number of matches in case of success,
 *           -EINVAL if number of values to search excesses the size of the
 * section
 *           -EAGAIN if no value has yet been produced in that memory section
 *           -ENOENT if no sample has been found to match the search
 */
int shd_data_find(struct shd_ctx *ctx,
			const struct shd_sample_search *search);

/*
 * @brief Allocate and fill a structure describing metadata for a range of
 * samples that matched a previous search
 *
 * @param[in,out] ctx : current shared memory context
 * @param[out] metadata : pointer to pointer to an array of read metadata
 *
 * @return : index of the reference sample in the metadata array in case of
 * success,
 *           -EPERM if the function was called out of sequence,
 */
int shd_data_read_metadata(struct shd_ctx *ctx,
				struct shd_sample_metadata **metadata);

/*
 * @brief Copy all the blobs of the previously selected samples into a user-
 * defined buffer
 *
 * @param[in] ctx : current shared memory context
 * @param[out] dst : destination buffer
 * @param[in] dst_size : size of destination buffer
 *
 * @return : number of samples that were read on success,
 *           -EINVAL if any argument is invalid, or destination buffer is too
 * small
 *           -EPERM if the function was called out of sequence
 */
int shd_data_read_blob(struct shd_ctx *ctx, void *dst, size_t dst_size);

/*
 * @brief Copy a given quantity of the previously selected samples into a user-
 * defined buffer
 *
 * @param[in] ctx : current shared memory context
 * @param[in] quantity : quantity to read
 * @param[out] dst : destination buffer
 * @param[in] dst_size : size of destination buffer
 *
 * @return : number of samples that were read on success,
 *           -EINVAL if any argument is invalid, or destination buffer is too
 * small
 *           -EPERM if the function was called out of sequence
 */
int shd_data_read_quantity(struct shd_ctx *ctx,
				const struct shd_quantity *quantity,
				void *dst,
				size_t dst_size);

/*
 * @brief Copy a set of quantities of a previously defined sample into user-
 * defined buffers
 *
 * @param[in] ctx : current shared memory context
 * @param[in] n_quantities : number of different quantities to read (set to
 * zero to read the whole blob)
 * @param[in] quantity : pointer to an array of structures describing the
 * quantities to read (irrelevant if n_quantities is set to 0)
 * @param[in,out] qty_samples : pointer to an array of structures that describe
 * the buffer where to output the quantities of the matching sample and their
 * metadata
 *
 * @return : number of quantities read in total,
 *           -EPERM if the function is called out-of-sequence
 *           -EINVAL if more than one sample has to be read
 *
 */
int shd_data_read_quantity_sample(struct shd_ctx *ctx,
				int n_quantities,
				const struct shd_quantity quantity[],
				struct shd_quantity_sample qty_samples[]);

/*
 * @brief Check if last read session was valid
 *
 * @param[in] ctx : current shared memory context
 * @param[in] rev : pointer to the revision structure that was output when
 * section was open
 *
 * @return : 0 if no problem was encountered,
 *           -EPERM if call was out-of-sequence,
 *           -EFAULT if read sample(s) was/were overwritten during last reading
 * sequence,
 *           -ENODEV if blob format changed since the memory section was open
 */
int shd_data_check_validity(struct shd_ctx *ctx, struct shd_revision *rev);

/*
 * @brief Free all data associated to last read session
 *
 * @param[in] ctx : current shared memory context
 *
 * @return : 0 if no problem was encountered,
 *           -EINVAL if argument is NULL,
 *           -EPERM if call was out-of-sequence,
 *           -errno in case of error
 */
int shd_data_end_read(struct shd_ctx *ctx);

/*
 * @brief Get total size of the data section
 *
 * @param[in] blob_size : size of a blob
 * @param[in] max_nb_samples : total number of samples in the section
 *
 * @return : total size of the data section
 */
size_t shd_data_get_total_size(size_t blob_size,
				uint32_t max_nb_samples);

/*
 * @brief Allocate and get data section description
 *
 * @param[in] ctx : current shared memory context
 * @param[in] hdr_info : user header info
 *
 * @return : data section description,
 *           NULL in case of error
 */
struct shd_data_section_desc *shd_data_section_desc_new(struct shd_ctx *ctx,
				const struct shd_hdr_user_info *hdr_info);

#ifdef __cplusplus
}
#endif

#endif /* _SHD_DATA_H_ */
