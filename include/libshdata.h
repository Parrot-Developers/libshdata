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
 * @file libshdata.h
 *
 * @brief shared memory data low level api.
 *
 * @details
 * Shared memory nomenclature :
 *
 *   - quantity :
 *       a coherent set of raw or elaborated physical quantities,
 * system states, ... (e.g. accelerations on the 3 axis, euler angles, GPS
 * coordinates, temperature, )
 *
 *   - sample :
 *       the value of a quantity at a given instant in time
 *
 *   - blob :
 *       a set of quantities that have been gathered in memory for
 * implementation purposes and whose samples are always timestamped at the
 * same instant. Each blob is given a unique identifier
 *
 *
 * Shared memory layout :
 *
 *   There is one shared memory section per blob.
 *   Each shared memory section features :
 *      - a *public* section header (with all the info required to read the
 * other subsections)
 *      - a blob metadata header (not used per se by this library, but
 * available for upper layers for blob introspection)
 *      - the data subsection, with as many slots as the number of samples the
 * section can contain
 *
 *   A shared memory section is organized this way :
 *
 * -----------------------------------------------------
 * | blob_size | max_nb_samples                        |
 * |---------------------------------------------------|
 * | production_rate | <global_synchro_metadata>       | section_header
 * |---------------------------------------------------|
 * | blob_metadata_hdr_offset | blob_metadata_hdr_size |
 * -----------------------------------------------------
 * | <blob_metadata>                                   | blob_metadata_header
 * -----------------------------------------------------
 * -----------------------------------------------------              ^
 * | <sample_metadata> | <synchro_metadata> | <blob>   | <- sample 1  |
 * -----------------------------------------------------              |
 * | <sample_metadata> | <synchro_metadata> | <blob>   | <- sample 2  |
 * -----------------------------------------------------              |
 * | <sample_metadata> | <synchro_metadata> | <blob>   | <- sample 3  | data
 * -----------------------------------------------------              | section
 * | <sample_metadata> | <synchro_metadata> | <blob>   | <- sample 4  |
 * -----------------------------------------------------              |
 * |                  ...                              |              |
 * -----------------------------------------------------              v
 *
 *
 * Shared memory access :
 *
 *   There is one shared memory section per blob ; the producer creates it,
 * and the consumers can open it.
 *
 *   A producer can either :
 *      - write a complete, coherent blob into memory
 *      - write a new sample quantity by quantity ; in this case it must first
 * declare the start of the production of a new sample, then write quantities
 * one after another, and then "commit" the new sample.
 *
 *   A consumer can either :
 *      - read the whole blob of one or several samples
 *      - read a given quantity in one or several samples
 *   In both cases, it must first select the samples it wishes to read, and
 * then read the quantities of its choice in a subset of those samples.
 *
 */

#ifndef _LIBSHDATA_H_
#define _LIBSHDATA_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>
#include "libshdata-concurrency-hooks.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHD_VERSION_MAJOR 4
#define SHD_VERSION_MINOR 0
#define SHD_MAGIC_NUMBER 0x65756821

/**
 * shared memory context : opaque structure which contains information to
 * both :
 *    - identify a shared memory section in the private memory of the
 * producer/consumer
 *    - track the progress of pending operations (read or write)
 */
struct shd_ctx;

/**
 * shared memory section header info
 */
struct shd_hdr_user_info {
	/* size of a blob */
	size_t blob_size;
	/* max number of samples (history depth) */
	uint32_t max_nb_samples;
	/* informal producer write period in us */
	uint32_t rate;
	/* blob metadata header size */
	size_t blob_metadata_hdr_size;
};

/**
 * shared memory sample search method
 */
enum shd_search_method_t {
	/* Latest sample */
	SHD_LATEST,
	/* Sample with the timestamp closest to the given date, either
	 * after or before */
	SHD_CLOSEST,
	/* Sample whose timestamp is immediately after the given date */
	SHD_FIRST_AFTER,
	/* Sample whose timestamp is immediately before the given date */
	SHD_FIRST_BEFORE
};

enum shd_ref_sample_search_hint {
	SHD_WINDOW_REF_SEARCH_NAIVE,
	SHD_WINDOW_REF_SEARCH_BINARY,
	SHD_WINDOW_REF_SEARCH_DATE
};

/**
 * Sample metadata
 *
 * @todo Add a configuration parameter to keep/remove the "exp" field ?
 */
struct shd_sample_metadata {
	/* sample timestamp */
	struct timespec ts;
	/* sample expiration date */
	struct timespec exp;
};

/**
 * Parameters for sample search : a "viewing window" is defined from a reference
 * sample (the one that matches the search defined by the fields "method" and
 * "date"), from which the user can require a given number of samples before
 * that reference, and after that reference
 */
struct shd_sample_search {
	/* [IN] reference date for sample search */
	struct timespec date;
	/* [IN] sample search method */
	enum shd_search_method_t method;
	/* [IN] maximum number of samples to read before date (can be set to
	 * 0) */
	uint32_t nb_values_before_date;
	/* [IN] maximum number of samples to read after date (can be set to 0)
	 */
	uint32_t nb_values_after_date;
};

/**
 * User-friendly structure to describe search results
 */
struct shd_search_result {
	/* Total number of matches found */
	int nb_matches;
	/* Index of the reference sample in all the arrays returned to the
	 * caller */
	int r_sample_idx;
};

/**
 * Quantity definition
 */
struct shd_quantity {
	/* [IN] offset from start of blob at which quantity is located */
	ptrdiff_t quantity_offset;
	/* [IN] size of quantity */
	size_t quantity_size;
};

/* Structure to hold a quantity sample and describe its destination in user
 * buffers */
struct shd_quantity_sample {
	/* sample metadata */
	struct shd_sample_metadata meta;
	/* Pointer to the user buffer that holds quantity values */
	void *ptr;
	/* Size of the user-buffer */
	size_t size;
};

/**
 * Opaque structure used to check whether the structure of the shared memory
 * section as seen by a consumer is up-to-date with regards to the producer
 * view
 */
struct shd_revision;

/**
 * @brief Create/Open a shared memory section for writer.
 *
 * @param blob_name name of shared memory section
 * @param shd_root: root directory where shared memory section is located. If
 * NULL, defaults to /dev/shm. Can also be set to any other valid directory
 * or /dev/mem if the shared memory is not in the file system
 * @param hdr_info shared memory header info
 * @param blob_metadata_hdr blob metadata header buffer
 *
 * @return shared memory context with write attribute,
 *         NULL on error :
 *           - if arguments are invalid
 *           - if blob_name exceeds max length
 *           - if blob_name contains a "/"
 */
struct shd_ctx *shd_create(const char *blob_name, const char *shd_root,
			   const struct shd_hdr_user_info *hdr_info,
			   const void *blob_metadata_hdr);

/**
 * @brief Open shared memory section for reader.
 *
 * @param[in] blob_name name of shared memory
 * @param shd_root: root directory where shared memory section is located. If
 * NULL, defaults to /dev/shm. Can also be set to any other valid directory
 * or /dev/mem if the shared memory is not in the file system
 * @param[out] rev : pointer to pointer of revision structure of the shared
 * memory (allocated by the library)
 *
 * @return shared memory context with read attribute,
 *         NULL on error (this is to be expected if the memory section
 *         doesn't exist yet)
 */
struct shd_ctx *shd_open(const char *blob_name, const char *shd_root,
			 struct shd_revision **rev);

/**
 * @brief Close shared memory.
 *
 * This function close previously open shared memory and
 * release associated context memory.
 *
 * @note shared memory is not destroyed after this call
 *
 * @param[in,out] ctx : context
 * @param[in] rev : pointer to the revision structure that was output when
 * section was open (set to NULL for a producer)
 *
 * @return : 0 on success,
 *           -EINVAL if ctx is NULL
 */
int shd_close(struct shd_ctx *ctx, struct shd_revision *rev);

/**
 * @brief Write a whole new blob into shared memory.
 * Unlike the case where quantities within a new sample are written one after
 * the other, this function is self-sufficient and performs all three
 * operations in a single function call
 *
 * @param[in,out] ctx : shared memory context
 * @param[in] src : sample to be written
 * @param[in] size : number of bytes in the sample (should be equal to
 * blob_size)
 * @param[in] metadata : metadata that will be associated to this sample
 *
 * @return 0 on success,
 *         -EINVAL if any argument is NULL,
 *         -EALREADY if a new sample is already being written in the same
 * context
 *         -EPERM if another thread is currently writing on this memory
 * section,
 *         -EFAULT if at least one write occurred outside of the current
 * context : this means that another thread has written into the section, which
 * is an unrecoverable fault
 */
int shd_write_new_blob(struct shd_ctx *ctx,
			const void *src,
			const size_t size,
			const struct shd_sample_metadata *metadata);

/**
 * @brief Read quantities from a sample that matches given search criterias
 *
 * @post shd_end_read should be called after
 *
 * @param[in] ctx : Shared memory context
 * @param[in] n_quantities : number of different quantities to read (set to
 * zero to read the whole blob)
 * @param[in] search : sample search parameters (no more than one sample should
 * be required)
 * @param[in] quantity : pointer to an array of structures describing the
 * quantities to read (irrelevant if n_quantities is set to 0)
 * @param[in,out] qty_samples : pointer to an array of structures that describe
 * the buffer where to output the quantities of the matching sample and their
 * metadata
 *
 * @return number of quantities written in dest in case of success,
 *         -EINVAL if any argument is invalid, or search parameters request
 * more than one sample,
 *         -EAGAIN if no value has yet been produced in that memory section
 *         -ENOENT if no sample has been found to match the search,
 *         -EFAULT if the matching sample was overwritten during the search
 */
int shd_read_from_sample(struct shd_ctx *ctx,
				int n_quantities,
				const struct shd_sample_search *search,
				const struct shd_quantity quantity[],
				struct shd_quantity_sample qty_samples[]);

/**
 * @brief Signal end of reading job for a process
 *
 * @pre shd_select_samples or shd_read_from_sample must have be called before
 *
 * @param[in] ctx Shared memory context
 *
 * @return : 0 if reading process was valid,
 *           -EINVAL if argument is NULL,
 *           -EPERM if call was out-of-sequence,
 *           -EFAULT if read sample(s) was/were overwritten during last reading
 * sequence
 *           -ENODEV if blob format changed since the memory section was open
 * (so that memory section should be closed and re-open properly)
 */
int shd_end_read(struct shd_ctx *ctx, struct shd_revision *rev);

/**
 * @brief Read section header info from shared memory
 *
 * @param[in,out] ctx : shared memory context
 * @param[out] hdr_info : header info allocated by user
 * @param[in] rev : pointer to the revision structure that was output when
 * section was open
 *
 * @return : 0 on success,
 *           -ENOMEM if dst_size does not match that of the metadata header,
 *           -EINVAL if any pointer argument is NULL,
 *           -ENODEV if blob format changed since the memory section was open
 * (so that memory section should be closed and re-open properly)
 */
int shd_read_section_hdr(struct shd_ctx *ctx,
				struct shd_hdr_user_info *hdr_info,
				struct shd_revision *rev);

/**
 * @brief Read blob metadata header
 *
 * @param[in,out] ctx : shared memory context
 * @param[out] dst : destination buffer
 * @param[in] dst_size : destination buffer size (should be equal to
 * blob_metadata_hdr_size)
 * @param[in] rev : pointer to the revision structure that was output when
 * section was open
 *
 * @return : 0 on success,
 *           -ENOMEM if dst_size does not match that of the metadata header,
 *           -EINVAL if any pointer argument is NULL,
 *           -ENODEV if blob format changed since the memory section was open
 * (so that memory section should be closed and re-open properly)
 */
int shd_read_blob_metadata_hdr(struct shd_ctx *ctx,
				void *dst,
				size_t dst_size,
				struct shd_revision *rev);

#ifdef SHD_ADVANCED_WRITE_API

/**
 * @brief Declare start of the writing process of a new sample.
 * This will most likely invalidate the oldest available sample for all
 * consumers.
 *
 * @post shd_commit_sample must be called to signal the end of the write
 * process
 *
 * @todo maybe an error should be returned if the time-stamp is older than
 * the most recent sample
 *
 * @param[in,out] ctx : shared memory context
 * @param[in] metadata : metadata that will be associated to this sample
 *
 * @return 0 on success,
 *         -EINVAL if any argument is NULL
 *         -EALREADY if a new sample is already being written in the same
 * context
 *         -EPERM if another thread is currently writing on this memory
 * section,
 *         -EFAULT if at least one write occurred outside of the current
 * context : this means that another thread has written into the section, which
 * is an unrecoverable fault
 */
int shd_new_sample(struct shd_ctx *ctx,
			const struct shd_sample_metadata *metadata);


/**
 * @brief Write a given quantity into the new sample
 *
 * @pre shd_new_sample must have been called
 *
 * @param[in,out] ctx Shared memory context
 * @param[in] quantity Quantity to be written
 * @param[in] src Source buffer
 *
 * @return 0 on success,
 *         -EINVAL if any argument is NULL
 *         -EPERM if shd_new_sample has not been called beforehand
 */
int shd_write_quantity(struct shd_ctx *ctx,
			const struct shd_quantity *quantity,
			const void *src);

/**
 * @brief Declare end of the writing process of a sample.
 * After the call to this function, the sample will be available for every
 * consumer to see.
 *
 * @pre shd_new_sample must have been called
 *
 * @param[in,out] ctx Shared memory context
 *
 * @return 0 on success,
 *         -EINVAL if argument is NULL,
 *         -EPERM if shd_new_sample has not been called beforehand
 */
int shd_commit_sample(struct shd_ctx *ctx);

#endif /* SHD_ADVANCED_WRITE_API */

#ifdef SHD_ADVANCED_READ_API
/**
 * @brief Declare the start of a reading "job".
 * Subsequent reads in the same "job" will only occur within the samples that
 * match the parameters passed here.
 *
 * @post shd_end_read should be called after
 *
 * @param[in,out] ctx : shared memory context
 * @param[in] search : sample search parameters
 * @param[out] metadata : pointer to pointer to array of metadata for the
 * matching samples (array will be allocated by the function)
 * @param[out] result : pointer to a user-allocated result structure
 *
 * @return 0 on success,
 *         -EINVAL if any argument is NULL or number of values to search
 * excesses the size of the section
 *         -EAGAIN if no sample has been written to that section yet
 *         -ENOENT if no sample has been found to match the search,
 *         -EFAULT if the matching sample was overwritten during the search
 */
int shd_select_samples(struct shd_ctx *ctx,
			const struct shd_sample_search *search,
			struct shd_sample_metadata **metadata,
			struct shd_search_result *result);

/**
 * @brief Read a given quantity from shared memory
 *
 * @pre shd_select_samples must have be called before
 * @post several consecutive calls to this function can be made but shd_end_read
 * should be called after
 *
 * @param[in] ctx Shared memory context
 * @param[in] quantity : pointer to a structure describing the quantity to
 * read within the blob, if a partial read is required
 * @param[in,out] dst Destination buffer (allocated by caller)
 * @param[in] dst_size Size of destination buffer
 *
 * @return number of matching samples on success,
 *         -EINVAL if any argument is invalid, or destination buffer is too
 * small
 *         -EPERM if the function was called out of sequence,
 */
int shd_read_quantity(struct shd_ctx *ctx,
			const struct shd_quantity *quantity,
			void *dst,
			size_t dst_size);

#endif /* SHD_ADVANCED_READ_API */

#ifdef __cplusplus
}
#endif

#endif /* _LIBSHDATA_H_ */
