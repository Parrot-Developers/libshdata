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
 * @file shd_section.h
 *
 * @brief shared memory section management : allows abstraction from the
 * implementation of the shared memory section, whose correspondance with a
 * blob name can be either supported by :
 *    * shm_open (setting the section in a given file prefixed by shd_ in
 * /dev/shm)
 *    * open (setting the section in a given file prefixed by shd_ in
 * the directory passed in parameter)
 *    * an external "dev-mem-lookup" module which for each blob name gives the
 * offset in /dev/mem that should be used
 *
 */

#ifndef _SHD_SECTION_H_
#define _SHD_SECTION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <limits.h>		/* For NAME_MAX macro */
#include <inttypes.h>
#include "libshdata.h"

#define SHD_SECTION_PREFIX		"/shd_"
/* -1 for final null of prefix string */
#define SHD_SECTION_PREFIX_LEN		(sizeof(SHD_SECTION_PREFIX) - 1)

#define SHD_SECTION_BLOB_NAME_MAX_LEN	(NAME_MAX - SHD_SECTION_PREFIX_LEN)

/* Type of shared memory section */
enum shd_section_type {
	SHD_SECTION_DEV_SHM,
	SHD_SECTION_SHM_OTHER,
	SHD_SECTION_DEV_MEM,
};

/* Shared memory section identifier : depends on the type of memory section */
struct shd_section_id {
	enum shd_section_type type;
	union {
		/* "Usual" shared memory : section was open either by shm_open
		 * or a plain "open" on a given file */
		int shm_fd;

		/* "/dev/mem" shared memory : we need an open file descriptor
		 * on /dev/mem and an offset in the memory */
		struct {
			int fd;
			intptr_t offset;
		} dev_mem;
	} id;
};

struct shd_section {
	/* m'mapped pointer to the top of the shared memory section */
	void *section_top;
	/* m'mapped pointer to the top of the header of the shared memory
	 * section */
	void *header_top;
	/* m'mapped pointer to the synchronization info in the header of the
	 * shared memory */
	void *sync_top;
	/* m'mapped pointer to the top of the metadata blob section header */
	void *metadata_blob_top;
	/* m'mapped pointer to the top of the data section */
	void *data_top;
	/* Total shared memory section size */
	size_t total_size;
};

/*
 * @brief Attempt to create a new shared memory section for a blob
 *
 * @param[in] blob_name : name of the blob to add to the directory
 * @param[in] shd_root: root directory where shared memory section is located.
 * If NULL, defaults to /dev/shm. Can also be set to any other valid directory
 * or /dev/mem if the shared memory is not in the file system
 *
 * @return : 0 in case of success,
 *           -ENAMETOOLONG if blob_name exceeds max length
 *           -EINVAL if blob_name contains a "/"
 *           -EEXIST if a memory section has already been created for this blob.
 */
int shd_section_new(const char *blob_name, const char *shd_root,
			struct shd_section_id *id);

/*
 * @brief Open a possibly already existing shared memory section for a blob
 *
 * @param[in] blob_name : name of the blob to add to the directory
 * @param[in] shd_root: root directory where shared memory section is located.
 * If NULL, defaults to /dev/shm. Can also be set to any other valid directory
 * or /dev/mem if the shared memory is not in the file system
 *
 * @return : 0 in case of success,
 *           -ENAMETOOLONG if blob_name exceeds max length
 *           -EINVAL if blob_name contains a "/"
 */
int shd_section_open(const char *blob_name, const char *shd_root,
			struct shd_section_id *id);

/*
 * @brief Get file descriptor to shared memory section for a blob
 *
 * @param[in] blob_name : name of the blob to look for
 * @param[in] shd_root: root directory where shared memory section is located.
 * If NULL, defaults to /dev/shm. Can also be set to any other valid directory
 * or /dev/mem if the shared memory is not in the file system
 *
 * @return : 0 in case of success,
 *           negative errno in case of error
 */
int shd_section_get(const char *blob_name, const char *shd_root,
			struct shd_section_id *id);

/*
 * @brief Lock a given memory section
 *
 * @param[in] id : id of the section to lock
 *
 * @return : 0 in case of success,
 *           -EWOULDBLOCK if the section is already locked by another process
 */
int shd_section_lock(const struct shd_section_id *id);

/*
 * @brief Unlock a given memory section
 *
 * @param[in] id : id of the section to unlock
 *
 * @return : 0 in case of success,
 *           -errno in case of error
 */
int shd_section_unlock(const struct shd_section_id *id);

/*
 * @brief Resize a given memory section
 *
 * @param[in] id : id of the section to unlock
 * @param[in] size : target size for the section
 *
 * @return : 0 in case of success,
 */
int shd_section_resize(const struct shd_section_id *id, size_t size);

/*
 * @brief Free a given memory section
 *
 * @param[in] id : id of the section to free
 *
 * @return : 0 in case of success
 */
int shd_section_free(const struct shd_section_id *id);

/*
 * @brief Allocate a structure which contains m'mapped pointers to each
 * subsection of a shared memory section
 *
 * Two possible cases here :
 *    - either the memory section is already properly formatted, and mapping
 *    information is extracted from the actual memory section header (consumer
 *    side, most likely)
 *    - or it is being created, and mapping information is extracted from the
 *    arguments of the function (producer side, most likely)
 *
 * @param[in] id : identifier for the open shared memory section
 * @param[in] hdr_info : header info for that memory section (NULL if this
 * information is not available from caller's private memory)
 * @param[in] prot : memory protection that should be used for those pointers
 * (PROT_READ or PROT_WRITE)
 *
 * @return : pointer to the mapping,
 *           NULL in case of error
 */
struct shd_section *shd_section_mapping_new(const struct shd_section_id *id,
			const struct shd_hdr_user_info *hdr_info,
			int prot);

/*
 * @brief Destroy a memory section mapping
 *
 * @param[in] map : mapping to destroy
 *
 * @return : 0 in all cases
 */
int shd_section_mapping_destroy(struct shd_section *map);

/*
 * @brief Return the total size of a shared memory section
 *
 * @param[in] hdr_info : header info for that memory section (NULL if this
 * information is not available from caller's private memory)
 *
 * @return : size of the section,
 *           -1 in case of error
 */
size_t shd_section_get_total_size(const struct shd_hdr_user_info *hdr_info);

#ifdef __cplusplus
}
#endif

#endif /* _SHD_SECTION_H_ */
