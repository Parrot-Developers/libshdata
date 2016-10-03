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

#include <errno.h>
#include <string.h>		/* for memcpy function */
#include <sys/mman.h>		/* For shm and PROT flags */
#include <stdlib.h>		/* For memory allocation functions */
#include <sys/types.h>
#include <unistd.h>		/* For lseek */
#include "shd_private.h"
#include "shd_hdr.h"
#include "libshdata.h"


int shd_hdr_write(void *hdr_start, const struct shd_hdr_user_info *user_hdr)
{
	int ret;
	struct shd_hdr *hdr = hdr_start;

	ret = memcmp(&hdr->user_info, user_hdr, sizeof(*user_hdr));

	if (ret != 0) {
		ULOGI("Writing a new header into memory section");
		memcpy(&hdr->user_info, user_hdr, sizeof(*user_hdr));
		ret = -1;
	} else {
		ULOGI("New user header matches the one already present in "
				"shared memory");
		ret = 0;
	}

	shd_sync_hdr_init(&hdr->sync_info);
	hdr->magic_number = SHD_MAGIC_NUMBER;
	hdr->lib_version_maj = SHD_VERSION_MAJOR;
	hdr->lib_version_min = SHD_VERSION_MINOR;

	return ret;
}

int shd_hdr_read(const struct shd_section_id *id, void *hdr_start,
			struct shd_hdr_user_info *hdr_user)
{
	int ret;
	struct shd_hdr *hdr = NULL;
	uint32_t lib_version_maj;
	uint64_t magic_number;

	if (hdr_user == NULL ||
			((id->type == SHD_SECTION_DEV_SHM ||
				id->type == SHD_SECTION_SHM_OTHER) &&
				id->id.shm_fd < 0 && hdr_start == NULL)) {
		ret = -EINVAL;
		goto exit;
	}

	if (hdr_start != NULL) {
		hdr = hdr_start;
		memcpy(hdr_user, &hdr->user_info, sizeof(*hdr_user));
	} else {
		switch (id->type) {
		case SHD_SECTION_DEV_SHM:
		case SHD_SECTION_SHM_OTHER:
			/* If the producer has just created the shared memory
			 * section file descriptor, and has not yet truncated
			 * it to its proper size, the m'map will succeed but
			 * eventually a SIGBUS will be triggered when trying to
			 * access to the mapped zone. To avoid this, we check
			 * whether the file size is larger than zero, and abort
			 * if it's not. */
			if (lseek(id->id.shm_fd, 0, SEEK_END) <= 0) {
				ULOGW("Can not m'map in zero-sized file");
				ret = -ENOMEM;
				goto exit;
			}

			hdr = mmap(0, sizeof(*hdr), PROT_READ, MAP_SHARED,
					id->id.shm_fd, 0);

			if (hdr == MAP_FAILED) {
				ret = -ENOMEM;
				goto exit;
			}
			break;
		case SHD_SECTION_DEV_MEM:
			hdr = mmap(0, sizeof(*hdr), PROT_READ, MAP_SHARED,
					id->id.dev_mem.fd,
					id->id.dev_mem.offset);

			if (hdr == MAP_FAILED) {
				ret = -ENOMEM;
				goto exit;
			}
			break;
		default:
			break;
		}

		/* Copy the header and library version into user own memory in
		 * all cases, then unmap the shared memory region */
		memcpy(hdr_user, &hdr->user_info, sizeof(*hdr_user));
		lib_version_maj = hdr->lib_version_maj;
		magic_number = hdr->magic_number;
		munmap(hdr, sizeof(*hdr));

		/* Check whether the section that was read and is supposed to
		 * be a shared memory section is a shared memory section indeed.
		 * This case can arise e.g. when reading from /dev/mem */
		if (magic_number != SHD_MAGIC_NUMBER) {
			ULOGE("Mapped memory section is not a shared memory "
					"section");
			ret = -EFAULT;
			goto exit;
		}

		/* If the library version registered in the shared memory
		 * region does not match the one we are using, return an error.
		 * The memcpy was therefore useless but is completely harmless,
		 * and the code to remove the mapping from memory is kept
		 * common.
		 */
		if (lib_version_maj != SHD_VERSION_MAJOR) {
			ULOGE("Trying to read a section created with another "
				"version of the library : update your "
				"software !");
			ret = -EFAULT;
			goto exit;
		}
	}
	ret = 0;

exit:
	return ret;
}

size_t shd_hdr_get_mdata_size(void *hdr_start)
{
	struct shd_hdr_user_info *hdr = hdr_start;

	return hdr->blob_metadata_hdr_size;
}
