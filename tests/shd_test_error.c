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
 * @file shd_test_error.c
 *
 * @brief
 *
 */

#include "shd_test.h"
#include "shd_test_helper.h"

static void test_error_overwrite_during_read(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	/* The search is setup to lookup for the maximum possible number of
	 * samples */
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	struct shd_revision *rev;
	struct prod_blob read_blob;
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) }
	};

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("overwrite-during-read"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("overwrite-during-read"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Produce at least one sample */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Try to read the whole blob in a valid way */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_TRUE_FATAL(ret > 0);

	/* Now that the latest the sample has been selected for read, write
	 * just enough new samples to overwrite the slot which was being read
	 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES + 1; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx_prod,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Declare end of read */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, -EFAULT);

	/* Close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_error_read_with_no_produced_sample(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	struct prod_blob read_blob;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	struct shd_revision *rev;
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) }
	};

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("error-read-with-no-prod-sample"),
				NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("error-read-with-no-prod-sample"),
				NULL,
				&rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Try to read the whole blob */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_EQUAL(ret, -EAGAIN);

	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, -EPERM);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_error_read_data_undersized_buffer(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	struct prod_blob read_blob;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	struct shd_revision *rev;
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) - 1 }
	};
	struct shd_quantity_sample q_samp[2] = {
		{ .ptr = &read_blob.i1, .size = sizeof(read_blob.i1) - 1 },
		QTY_SAMPLE_FIELD(read_blob, c1)
	};
	const struct shd_quantity quantity[2] = {
		QUANTITY_FIELD(prod_blob, s_blob, i1),
		QUANTITY_FIELD(prod_blob, s_blob, c1)
	};

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("error-read-undersized-buffer"),
				NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("error-read-undersized-buffer"),
				NULL,
				&rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Produce at least one sample in the shared memory section */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Try to read the whole blob ; this should fail since the destination
	 * buffer is too small */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_EQUAL(ret, 0);

	/* Try to read 2 quantities ; one buffer is under-sized, so only one
	 * should be read */
	ret = shd_read_from_sample(ctx_cons, 2, &search, quantity,
					q_samp);
	CU_ASSERT_EQUAL(ret, 1);

	/* This call is valid */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_error_revision_nb(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	struct prod_blob read_blob;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	struct shd_revision *rev;
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) }
	};

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("error-revision-nb"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("error-revision-nb"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* The producer closes and re-opens its memory section, possibly
	 * updating the blob format ; in the mean time the consumer keeps its
	 * context as it was when the producer first opened the section */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ctx_prod = shd_create(BLOB_NAME("error-revision-nb"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Produce at least one sample in the shared memory section */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Try to read the whole blob ; this should work */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_TRUE(ret > 0);

	/* ... but this function call should signal that the revision number
	 * has changed */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, -ENODEV);

	/* The consumer then closes and reopen its context */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ctx_cons = shd_open(BLOB_NAME("error-revision-nb"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* And now everything should work properly */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_TRUE(ret > 0);
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
					&sample_meta.ts));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.exp,
					&sample_meta.exp));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}


CU_TestInfo s_error_tests[] = {
	{(char *)"sample overwrite during read",
			&test_error_overwrite_during_read},
	{(char *)"try to read when no sample has been produced yet",
			&test_error_read_with_no_produced_sample},
	{(char *)"try to read data with under-sized buffers",
			&test_error_read_data_undersized_buffer},
	{(char *)"try to read without updating revision number",
			&test_error_revision_nb},
	CU_TEST_INFO_NULL,
};
