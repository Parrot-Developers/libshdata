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
 * @file shd_test_func_write_2.c
 *
 * @brief
 *
 */

#define SHD_ADVANCED_READ_API
#define SHD_ADVANCED_WRITE_API
#include "shd_test.h"
#include "shd_test_helper.h"

static void test_func_adv_write_by_quantity(void)
{

	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	struct shd_sample_metadata *metadata = NULL;
	struct shd_search_result result;
	struct prod_blob read_blob;
	struct shd_revision *rev;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("write-2-by-quantity"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("write-2-by-quantity"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Declare production of a new sample */
	ret = shd_new_sample(ctx_prod, &sample_meta);
	CU_ASSERT_EQUAL(ret, 0);

	/* Write the whole blob quantity by quantity, ensure that everything
	 * goes right at each step */
	ret = shd_write_quantity(ctx_prod, &q_s_blob_i1,
					QUANTITY_PTR(s_blob, i1));
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_write_quantity(ctx_prod, &q_s_blob_c1,
					QUANTITY_PTR(s_blob, c1));
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_write_quantity(ctx_prod, &q_s_blob_li1,
					QUANTITY_PTR(s_blob, li1));
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_write_quantity(ctx_prod, &q_s_blob_ui1,
					QUANTITY_PTR(s_blob, ui1));
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_write_quantity(ctx_prod, &q_s_blob_f1,
					QUANTITY_PTR(s_blob, f1));
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_write_quantity(ctx_prod, &q_s_blob_acc,
					QUANTITY_PTR(s_blob, acc));
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_write_quantity(ctx_prod, &q_s_blob_angles,
					QUANTITY_PTR(s_blob, angles));
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_write_quantity(ctx_prod, &q_s_blob_state,
					QUANTITY_PTR(s_blob, state));
	CU_ASSERT_EQUAL(ret, 0);

	/* Commit the new sample */
	ret = shd_commit_sample(ctx_prod);
	CU_ASSERT_EQUAL(ret, 0);

	/* Select the newly-produced sample for reading */
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Read the whole sample blob and check it matches the original */
	ret = shd_read_quantity(ctx_cons, NULL,
				&read_blob, sizeof(read_blob));
	CU_ASSERT_TRUE(ret > 0);
	CU_ASSERT_TRUE(compare_blobs(s_blob, read_blob));

	/* Declare end of read */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

CU_TestInfo s_func_adv_write_tests[] = {
	{(char *)"write by quantity", &test_func_adv_write_by_quantity},
	CU_TEST_INFO_NULL,
};
