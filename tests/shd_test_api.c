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
 * @file shd_test_api.c
 *
 * @brief API unit tests.
 *
 */

#include "shd_test.h"
#include "shd_test_helper.h"

static void test_api_close(void)
{
	int ret;

	/* Invalid argument */
	ret = shd_close(NULL, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
}

static void test_api_create_close(void)
{
	struct shd_ctx *ctx1, *ctx2, *ctx3, *ctx4;
	char long_blob_name[NAME_MAX];
	char unique_blob_name[NAME_MAX];
	int i;
	int ret;

	/* Invalid arguments */
	ctx1 = shd_create(NULL, NULL, &s_hdr_info, &s_metadata_hdr);
	CU_ASSERT_PTR_NULL(ctx1);
	ctx1 = shd_create("whatever", NULL, NULL, &s_metadata_hdr);
	CU_ASSERT_PTR_NULL(ctx1);
	ctx1 = shd_create("whatever", NULL, &s_hdr_info, NULL);
	CU_ASSERT_PTR_NULL(ctx1);

	for (i = 0; i < NAME_MAX - 1; i++)
		long_blob_name[i] = 'a' + i%26;
	long_blob_name[i] = '\0';

	/* The blob name is too long */
	ctx1 = shd_create(long_blob_name, NULL, &s_hdr_info, &s_metadata_hdr);
	CU_ASSERT_PTR_NULL(ctx1);

	/* The blob name contains a slash */
	ctx1 = shd_create("a_name_with/_a_slash", NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NULL(ctx1);

	/* This shared memory creation is valid */
	ctx1 = shd_create(BLOB_NAME("create-close"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL(ctx1);

	/* Creation of another shared memory section */
	ctx2 = shd_create(BLOB_NAME("create-close2"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL(ctx2);

	/* Even though a memory section has just been created with this name,
	 * this should work */
	ctx3 = shd_create(BLOB_NAME("create-close"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL(ctx3);

	/* Create a memory section with a name guaranteed to be unique */

	CU_ASSERT_TRUE_FATAL(
		get_unique_blob_name(BLOB_NAME("create-close"),
					unique_blob_name) > 0);
	ctx4 = shd_create(unique_blob_name, NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL(ctx4);

	/* The close operations should unfold normally */
	ret = shd_close(ctx1, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx2, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx3, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_api_open_close(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	char long_blob_name[NAME_MAX];
	int i;
	int ret;
	struct shd_revision *rev;

	/* Invalid argument */
	ctx_cons = shd_open(NULL, NULL, &rev);
	CU_ASSERT_PTR_NULL(ctx_cons);

	for (i = 0; i < NAME_MAX - 1; i++)
		long_blob_name[i] = 'a' + i%26;
	long_blob_name[i] = '\0';

	/* The blob name is too long */
	ctx_cons = shd_open(long_blob_name, NULL, &rev);
	CU_ASSERT_PTR_NULL(ctx_cons);

	/* The blob name contains a slash */
	ctx_cons = shd_open("a_name_with/_a_slash", NULL, &rev);
	CU_ASSERT_PTR_NULL(ctx_cons);

	/* Trying to open a section that does not exist */
	ctx_cons = shd_open("non_existing_section", NULL, &rev);
	CU_ASSERT_PTR_NULL(ctx_cons);

	ctx_prod = shd_create(BLOB_NAME("open-close"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL(ctx_prod);

	/* This section has just been created */
	ctx_cons = shd_open(BLOB_NAME("open-close"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* The first close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_api_override_shm_dir(void)
{
	int ret;
	struct shd_ctx *ctx1, *ctx2;
	const char blob_name[] = BLOB_NAME("override");
	const char shm_dir[] = "/tmp";
	const char path[] = "/tmp/shd_" BLOB_NAME("override");
	struct stat st;
	struct shd_revision *rev;

	/* Remove previous if any */
	unlink(path);

	/* Create and check it is in the correct directory */
	ctx1 = shd_create(blob_name, shm_dir, &s_hdr_info, &s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL(ctx1);
	ret = stat(path, &st);
	CU_ASSERT_EQUAL(ret, 0);

	/* Open it */
	ctx2 = shd_open(blob_name, shm_dir, &rev);
	CU_ASSERT_PTR_NOT_NULL(ctx2);

	ret = shd_close(ctx1, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx2, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	unlink(path);
}

static void test_api_open_from_dev_mem(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	struct shd_revision *rev;

	/* Invalid argument */
	ctx_cons = shd_open(NULL, "/dev/mem", &rev);
	CU_ASSERT_PTR_NULL(ctx_cons);

	/* Trying to open a section that does not yet exist */
	ctx_cons = shd_open("non_existing_section", "/dev/mem", &rev);
	CU_ASSERT_PTR_NULL(ctx_cons);

	ctx_prod = shd_create(BLOB_NAME("open-close-dev-mem"), "/dev/mem",
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL(ctx_prod);

	/* This section has just been created */
	ctx_cons = shd_open(BLOB_NAME("open-close-dev-mem"), "/dev/mem", &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* The first close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_api_write_by_blob(void)
{
	struct shd_ctx *ctx;
	int ret;
	struct shd_sample_metadata sample_meta = METADATA_INIT;

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Create a producer context to play with */
	ctx = shd_create(BLOB_NAME("write-by-blob"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);

	/* Invalid arguments */
	ret = shd_write_new_blob(NULL, &s_blob, sizeof(s_blob), &sample_meta);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_write_new_blob(ctx, NULL, sizeof(s_blob), &sample_meta);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_write_new_blob(ctx, &s_blob, sizeof(s_blob), NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* This call is legitimate */
	ret = shd_write_new_blob(ctx, &s_blob, sizeof(s_blob), &sample_meta);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_api_read_from_sample(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	int i1_dest;
	struct prod_blob read_blob;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	struct shd_sample_search bad_search_1 = {
		.nb_values_after_date = 1,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	struct shd_sample_search bad_search_2 = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 1,
		.method = SHD_LATEST
	};
	const struct shd_quantity quantity = {
		offsetof(struct prod_blob, i1), sizeof(s_blob.i1)
	};
	struct shd_quantity_sample q_samp[1] = {
		{ .ptr = &i1_dest, .size = sizeof(i1_dest) }
	};
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) }
	};
	struct shd_revision *rev;

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("read-from-sample"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("read-from-sample"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Produce at least one sample in the shared memory section */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Invalid arguments */
	ret = shd_read_from_sample(NULL, 1, &search, &quantity, q_samp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_read_from_sample(ctx_cons, 1, NULL, &quantity, q_samp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_read_from_sample(ctx_cons, 1, &search, NULL, q_samp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_read_from_sample(ctx_cons, 1, &search, &quantity, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_read_from_sample(ctx_cons, 1, &bad_search_1, &quantity,
					q_samp);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_read_from_sample(ctx_cons, 1, &bad_search_2, &quantity,
					q_samp);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Try to read a sample in a valid way */
	ret = shd_read_from_sample(ctx_cons, 1, &search, &quantity,
					q_samp);
	CU_ASSERT_TRUE(ret > 0);

	/* Try to read the whole blob in a valid way */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_TRUE(ret > 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_api_end_read(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	struct prod_blob read_blob;
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) }
	};
	struct shd_revision *rev;

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("end-read"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("end-read"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Invalid argument */
	ret = shd_end_read(NULL, rev);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_end_read(ctx_cons, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* This call is out-of-sequence */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, -EPERM);

	/* Produce at least one sample in the shared memory section */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Try to read the whole blob in a valid way */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_TRUE_FATAL(ret > 0);

	/* This call is valid */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_api_read_section_hdr(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	struct shd_revision *rev;
	struct shd_hdr_user_info usr_info;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("read-section-hdr"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("read-section-hdr"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Invalid arguments */
	ret = shd_read_section_hdr(NULL, &usr_info, rev);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_read_section_hdr(ctx_cons, NULL, rev);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_read_section_hdr(ctx_cons, &usr_info, NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Valid call */
	ret = shd_read_section_hdr(ctx_cons, &usr_info, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_api_read_mdata_section_hdr(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	struct shd_revision *rev;
	struct blob_metadata_hdr mdata_hdr;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("read-mdata-section-hdr"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("read-mdata-section-hdr"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Invalid arguments */
	ret = shd_read_blob_metadata_hdr(NULL,
						&mdata_hdr,
						sizeof(mdata_hdr),
						rev);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_read_blob_metadata_hdr(ctx_cons,
						NULL,
						sizeof(mdata_hdr),
						rev);
	CU_ASSERT_EQUAL(ret, -EINVAL);
	ret = shd_read_blob_metadata_hdr(ctx_cons,
						&mdata_hdr,
						sizeof(mdata_hdr),
						NULL);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Inadequate buffer size */
	ret = shd_read_blob_metadata_hdr(ctx_cons,
						&mdata_hdr,
						sizeof(mdata_hdr) - 1,
						rev);
	CU_ASSERT_EQUAL(ret, -ENOMEM);

	/* Valid call */
	ret = shd_read_blob_metadata_hdr(ctx_cons,
						&mdata_hdr,
						sizeof(mdata_hdr),
						rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}

CU_TestInfo s_api_tests[] = {
	{(char *)"basic close", &test_api_close},
	{(char *)"producer-side create and close", &test_api_create_close},
	{(char *)"consumer-side open and close", &test_api_open_close},
	{(char *)"override shm dir", &test_api_override_shm_dir},
	{(char *)"create and open from dev/mem", &test_api_open_from_dev_mem},
	{(char *)"producer-side write by blob", &test_api_write_by_blob},
	{(char *)"consumer-side read from sample", &test_api_read_from_sample},
	{(char *)"consumer-side end read", &test_api_end_read},
	{(char *)"consumer-side read section header",
			&test_api_read_section_hdr},
	{(char *)"consumer-side read metadata header",
			&test_api_read_mdata_section_hdr},
	CU_TEST_INFO_NULL,
};
