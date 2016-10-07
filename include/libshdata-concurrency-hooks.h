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
 * @file libshdata-concurrency-hooks.h
 *
 * @brief Definitions of hooks for concurrency tests
 */

#ifndef _LIBSHDATA_CONCURRENCY_HOOKS_H_
#define _LIBSHDATA_CONCURRENCY_HOOKS_H_

enum shd_concurrency_hook {
	HOOK_SECTION_CREATED_BEFORE_UNLOCK = 0,
	HOOK_SECTION_CREATED_BEFORE_TRUNCATE,
	HOOK_SECTION_CREATED_NOT_RESIZED,
	HOOK_SECTION_CREATED_LOCK_TAKEN,
	HOOK_SECTION_OPEN_START,
	HOOK_SECTION_OPEN_MMAP_DONE,
	HOOK_SAMPLE_WRITE_START,
	HOOK_SAMPLE_WRITE_BEFORE_COMMIT,
	HOOK_SAMPLE_WRITE_AFTER_COMMIT,
	HOOK_WINDOW_SEARCH_START,
	HOOK_WINDOW_SEARCH_OVER,
	HOOK_TOTAL
};

/*
 * @brief Function called at each hook point : defined as a weak symbol here,
 * and as a strong symbol in libshdata's test code. In normal operation this
 * code doesn't do anything but go through the "false" branch of the test in
 * SHD_HOOK (obviously a slight performance hit)
 */
void __attribute__((weak)) shd_concurrency_hook(enum shd_concurrency_hook hook);

#define SHD_HOOK(a)					\
	do {						\
		if (shd_concurrency_hook)		\
			shd_concurrency_hook(a);	\
	} while (0)


#endif /* _LIBSHDATA_CONCURRENCY_HOOKS_H_ */
