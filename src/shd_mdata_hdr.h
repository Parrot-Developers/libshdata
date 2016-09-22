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
 * @file shd_mdata_hdr.h
 *
 * @brief shared memory metadata header management.
 *
 */

#ifndef _SHD_MDATA_H_
#define _SHD_MDATA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/*
 * @brief Write metadata section into shared memory
 *
 * Segmentation fault will occur if the process has no write access to the
 * address pointed by hdr_start
 *
 * @param[in] mdata_hdr_start : pointer to the start of the shared memory header
 * @param[in] src : pointer to the memory structure describing the header
 * @param[in] mdata_size : size of the metadata header
 *
 * @return : 0 if the metadata that was already present in the memory section
 * was already filled with the new metadata value,
 *           -1 otherwise
 */
int shd_mdata_hdr_write(void *mdata_hdr_start,
				const void *src,
				size_t mdata_size);

/*
 * @brief Read metadata section from shared memory
 *
 * @param[in] mdata_hdr_start : pointer to the start of the shared memory header
 * @param[in] dst : pointer to the destination memory structure
 * @param[in] dst_size : size of the destination buffer
 *
 * @return : 0 if metadata was correctly copied,
 *           -EINVAL if dst is invalid
 */
int shd_mdata_hdr_read(void *mdata_hdr_start, void *dst, size_t dst_size);

#ifdef __cplusplus
}
#endif

#endif /* _SHD_MDATA_H_ */
