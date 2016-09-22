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
 * @file shd_test_func_write.c
 *
 * @brief
 *
 */

#include "shd_test.h"
#include "shd_test_helper.h"

static void test_func_basic_write_by_blob_whole_once(void)
{
	struct shd_ctx *ctx;
	int ret;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	int index;

	/* Create a producer context to play with */
	ctx = shd_create(BLOB_NAME("write-by-b-once"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);

	/* Write the whole section once
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_basic_write_by_blob_whole_twice(void)
{
	struct shd_ctx *ctx;
	int ret;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	int index;

	/* Create a producer context to play with */
	ctx = shd_create(BLOB_NAME("write-by-b-twice"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);

	/* Write the whole section twice
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < 2 * NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_write_change_blob_structure(void)
{
	struct shd_ctx *ctx;
	int ret;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	int index;
	struct shd_hdr_user_info s_hdr_alt_info;

	/* Create a producer context to play with */
	ctx = shd_create(BLOB_NAME("write-change-blob"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);

	/* Write the whole section once
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx, NULL);
	CU_ASSERT_EQUAL(ret, 0);

	/* Recreate the same section with a different blob size and repeat the
	 * same operations : everything should unfold normally */
	memcpy(&s_hdr_alt_info, &s_hdr_info, sizeof(s_hdr_info));
	s_hdr_alt_info.blob_size = sizeof(s_blob_alt_size);
	ctx = shd_create(BLOB_NAME("write-change-blob"), NULL,
				&s_hdr_alt_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);

	/* Write the whole section once
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx,
						&s_blob_alt_size,
						sizeof(s_blob_alt_size),
						&sample_meta);
	}
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

CU_TestInfo s_func_basic_write_tests[] = {
	{(char *)"write by blob whole section once",
			&test_func_basic_write_by_blob_whole_once},
	{(char *)"write by blob whole section twice",
			&test_func_basic_write_by_blob_whole_twice},
	{(char *)"write by blob and then change blob structure",
			&test_func_write_change_blob_structure},
	CU_TEST_INFO_NULL,
};
