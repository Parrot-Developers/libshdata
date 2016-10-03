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
 * @file shd_utils.h
 *
 * @brief shared memory useful macros
 *
 */

#ifndef SHD_UTILS_H_
#define SHD_UTILS_H_

#include <stdint.h>

#define ALIGN_UP(x)		ALIGN(x, 4)
#define ALIGN(x, a)		__ALIGN_MASK(x, (__typeof__(x))(a)-1)
#define __ALIGN_MASK(x, mask)	(((x)+(mask))&~(mask))


static inline int max(int a, int b)
{
	return a > b ? a : b;
}

static inline int min(int a, int b)
{
	return a < b ? a : b;
}


/*
 * idx2 is known to have happened AFTER idx1
 */
static inline uint32_t interval_between(int idx1, int idx2, uint32_t size)
{
	if (idx1 <= idx2)
		return idx2 - idx1;
	else
		return idx2 + size - idx1;
}

static inline int index_n_after(int idx, int n, uint32_t size)
{
	return (idx+n)%size;
}

static inline int index_n_before(int idx, int n, uint32_t size)
{
	return (idx+size-n)%size;
}

static inline int index_next(int idx, uint32_t size)
{
	return index_n_after(idx, 1, size);
}

static inline int index_previous(int idx, uint32_t size)
{
	return index_n_before(idx, 1, size);
}

static inline void index_increment(int *idx, uint32_t size)
{
	int new_idx = index_next(*idx, size);
	*idx = new_idx;
}

static inline void index_increment_from(int *idx_dest,
						int *idx_src,
						uint32_t size)
{
	int new_idx = index_next(*idx_src, size);
	*idx_dest = new_idx;
}

static inline void index_decrement(int *idx, uint32_t size)
{
	int new_idx = index_previous(*idx, size);
	*idx = new_idx;
}

#endif /* SHD_UTILS_H_ */
