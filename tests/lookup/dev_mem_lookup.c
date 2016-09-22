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
 * @file dev_mem_lookup.c
 *
 * @brief Implem file for dev-mem lookup feature for unit-test mode
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <dev_mem_lookup.h>

#define BLOB_NAME_MAX_SIZE 255

/* This function to return the physical address from a virtual address is
 * shamelessly copied-pasted from here : http://stackoverflow.com/a/28987409
 * (Stack overflow topic "How to find the physical address of a variable from
 * user-space in Linux?")
 */
static intptr_t vtop(uintptr_t vaddr)
{
	FILE *pagemap;
	intptr_t paddr = 0;
	int offset = (vaddr / sysconf(_SC_PAGESIZE)) * sizeof(uint64_t);
	uint64_t e;

	/* https://www.kernel.org/doc/Documentation/vm/pagemap.txt */
	pagemap = fopen("/proc/self/pagemap", "r");
	if (pagemap) {
		if (lseek(fileno(pagemap), offset, SEEK_SET) == offset) {
			if (fread(&e, sizeof(uint64_t), 1, pagemap)) {
				if (e & (1ULL << 63)) {
					paddr = e & ((1ULL << 54) - 1);
					paddr = paddr * sysconf(_SC_PAGESIZE);
					paddr = paddr | (vaddr &
						(sysconf(_SC_PAGESIZE) - 1));
				}
			}
		}
		fclose(pagemap);
	}
	return paddr;
}

int dev_mem_lookup(const char *blob_name, intptr_t *phys_addr)
{
	if (!strncmp(blob_name, "myBlob_open-close-dev-mem",
				BLOB_NAME_MAX_SIZE)) {
		static bool first_call = true;
		static void *base_ptr = NULL;

		if (first_call) {
			/* Allocate a given memory space of which we
			 * will then return the physical address */
			long int page_size = sysconf(_SC_PAGESIZE);
			int ret = posix_memalign((void **) &base_ptr,
					page_size, page_size * 4);
			if (ret)
				return -ENOMEM;
			first_call = false;
		}

		/* Access the allocated memory to force a page-fault to
		 * ensure that the page is actually loaded in the TLB,
		 * and the physical address is therefore accessible */
		volatile int dummy = *(int *) base_ptr;
		dummy++;

		*phys_addr = vtop((uintptr_t) base_ptr);
		return 0;
	} else {
		return -ENOENT;
	}
}
