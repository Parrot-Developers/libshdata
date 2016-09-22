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
 * @file shd_test_func_read_hdr.c
 *
 * @brief
 *
 */

#include "shd_test.h"
#include "shd_test_helper.h"

static void test_func_read_hdr(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	struct shd_revision *rev;
	struct shd_hdr_user_info usr_info;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("func-read-hdr"),
				NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("func-read-hdr"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Read the section header and check that all the fields match exactly
	 * to their original counterpart */
	ret = shd_read_section_hdr(ctx_cons, &usr_info, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_EQUAL(usr_info.blob_metadata_hdr_size,
				s_hdr_info.blob_metadata_hdr_size);
	CU_ASSERT_EQUAL(usr_info.blob_size,
				s_hdr_info.blob_size);
	CU_ASSERT_EQUAL(usr_info.max_nb_samples,
				s_hdr_info.max_nb_samples);
	CU_ASSERT_EQUAL(usr_info.rate,
				s_hdr_info.rate);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_read_metadata_hdr(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	struct shd_revision *rev;
	struct blob_metadata_hdr mdata_hdr;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("func-read-metadata-hdr"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("func-read-metadata-hdr"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Read the section header and check that all the fields match exactly
	 * to their original counterpart */
	ret = shd_read_blob_metadata_hdr(ctx_cons,
						&mdata_hdr,
						sizeof(mdata_hdr),
						rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_EQUAL(mdata_hdr.i1, TEST_VAL_MDATA_i1);
	CU_ASSERT_EQUAL(mdata_hdr.i2, TEST_VAL_MDATA_i2);
	CU_ASSERT_EQUAL(mdata_hdr.li1, TEST_VAL_MDATA_li1);
	CU_ASSERT_EQUAL(strncmp(mdata_hdr.c1,
					TEST_VAL_MDATA_c1,
					strlen(TEST_VAL_MDATA_c1)), 0);
	CU_ASSERT_EQUAL(strncmp(mdata_hdr.c2,
					TEST_VAL_MDATA_c2,
					strlen(TEST_VAL_MDATA_c2)), 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}

CU_TestInfo s_func_read_hdr_tests[] = {
	{(char *)"Read section header",
			&test_func_read_hdr},
	{(char *)"Read blob metadata header",
			&test_func_read_metadata_hdr},
	CU_TEST_INFO_NULL,
};

