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
 * @brief shared memory section header management.
 *
 */

#include <errno.h>
#include <string.h>		/* for memcpy function */
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

	if (hdr_user == NULL) {
		ret = -EINVAL;
		goto exit;
	}

	if (hdr_start != NULL) {
		struct shd_hdr *hdr = hdr_start;

		memcpy(hdr_user, &hdr->user_info, sizeof(*hdr_user));
	} else {
		struct shd_hdr hdr;

		ret = (*id->backend.hdr_read) (&hdr, id->instance);

		/* Copy the header and library version into user own memory in
		 * all cases, then unmap the shared memory region */
		memcpy(hdr_user, &hdr.user_info, sizeof(*hdr_user));

		/* Check whether the section that was read and is supposed to
		 * be a shared memory section is a shared memory section indeed.
		 * This case can arise e.g. when reading from /dev/mem */
		if (hdr.magic_number != SHD_MAGIC_NUMBER) {
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
		if (hdr.lib_version_maj != SHD_VERSION_MAJOR) {
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
