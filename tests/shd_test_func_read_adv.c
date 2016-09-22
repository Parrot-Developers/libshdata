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
 * @file shd_test_func_read.c
 *
 * @brief
 *
 */

#define SHD_ADVANCED_READ_API
#include "shd_test.h"
#include "shd_test_helper.h"


static void test_func_adv_read_select_sample_one_latest(void)
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
	struct shd_revision *rev;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("select-sample-1-latest"), NULL,
					&s_hdr_info,
					&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("select-sample-1-latest"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Produce a sample to read */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* The consumer should read the same values in the latest sample than
	 * what was published by the producer */
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(metadata);
	CU_ASSERT_EQUAL(result.nb_matches, 1);
	CU_ASSERT_TRUE(time_is_equal(&metadata[result.r_sample_idx].ts,
					&sample_meta.ts));
	CU_ASSERT_TRUE(time_is_equal(&metadata[result.r_sample_idx].exp,
					&sample_meta.exp));

	/* Declare end of read */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_adv_read_select_sample_many_latest(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index, step_back;
	bool b_ret;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	/* The search is setup to lookup for the maximum possible number of
	 * samples */
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = NUMBER_OF_SAMPLES - 1,
		.method = SHD_LATEST
	};
	struct shd_sample_metadata *metadata = NULL;
	struct shd_search_result result;
	struct shd_revision *rev;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("select-sample-many-latest"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("select-sample-many-latest"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Write the whole section once
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx_prod,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* That search should allow to retrieve all the samples that were
	 * published by the producer */
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL(ret, 0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(&metadata);
	CU_ASSERT_EQUAL(result.nb_matches, NUMBER_OF_SAMPLES);
	CU_ASSERT_EQUAL(result.r_sample_idx, NUMBER_OF_SAMPLES - 1);

	/* Check that all the timestamps read are correct */
	step_back = 0;
	b_ret = true;
	for (index = result.nb_matches - 1; index >= 0; index--, step_back++) {
		struct timespec old_ts = time_in_past(sample_meta.ts,
							step_back);
		ret &= time_is_equal(&metadata[index].ts, &old_ts);
	}
	CU_ASSERT_TRUE(b_ret);

	/* This search should fail since the number of requested samples
	 * excesses the capacity of the memory section */
	search.nb_values_before_date = NUMBER_OF_SAMPLES;
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL(ret, -EINVAL);

	/* Declare end of read */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_adv_read_select_sample_first_after(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_FIRST_AFTER
	};
	struct shd_sample_metadata *metadata = NULL;
	struct shd_search_result result;
	struct shd_revision *rev;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("select-sample-first-after"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("select-sample-first-after"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Write the whole section once
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx_prod,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	struct timespec last_sample_ts, match_expected_ts;
	time_set(&last_sample_ts, sample_meta.ts);

	/*
	 * We search for the sample whose timestamp is set after a date set
	 * a little amount of time before the timestamp of each sample that was
	 * previously produced
	 * For all those searches we should find only one sample, whose
	 * timestamp is after the search date, and equals that of the sample
	 * that was produced
	 */
	bool b_time = true;
	bool b_result = true;
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES - 1; index++) {
		time_set(&search.date,
				time_in_past_before(last_sample_ts, index));
		match_expected_ts = time_in_past(last_sample_ts, index);

		ret += shd_select_samples(ctx_cons,
				&search,
				&metadata,
				&result);

		CU_ASSERT_PTR_NOT_NULL_FATAL(metadata);

		b_result &= (result.nb_matches == 1);
		b_result &= (result.r_sample_idx == 0);

		b_time &= time_is_after(&metadata[result.r_sample_idx].ts,
				&search.date);
		b_time &= time_is_equal(&metadata[result.r_sample_idx].ts,
						&match_expected_ts);
		/* Declare end of read */
		ret = shd_end_read(ctx_cons, rev);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE(b_time);
	CU_ASSERT_TRUE(b_result);
	/* ret should be equal to 0 throughout the loop */
	CU_ASSERT_EQUAL(ret, 0);


	/* Close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_adv_read_select_sample_first_before(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_FIRST_BEFORE
	};
	struct shd_sample_metadata *metadata = NULL;
	struct shd_search_result result;
	struct shd_revision *rev;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("select-sample-first-before"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("select-sample-first-before"),
			NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Write the whole section once
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx_prod,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	struct timespec last_sample_ts, match_expected_ts;
	time_set(&last_sample_ts, sample_meta.ts);

	/*
	 * We search for the sample whose timestamp is set before a date set
	 * a little amount of time after the timestamp of each sample that was
	 * previously produced
	 * For all those searches we should find only one sample, whose
	 * timestamp is before the search date, and equals that of the sample
	 * that was produced
	 */
	bool b_time = true;
	bool b_result = true;
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		time_set(&search.date,
				time_in_past_after(last_sample_ts, index));

		match_expected_ts = time_in_past(last_sample_ts, index);

		ret += shd_select_samples(ctx_cons,
				&search,
				&metadata,
				&result);

		CU_ASSERT_EQUAL_FATAL(ret, 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(metadata);

		b_result &= (result.nb_matches == 1);
		b_result &= (result.r_sample_idx == 0);

		b_time &= time_is_before(&metadata[result.r_sample_idx].ts,
				&search.date);
		b_time &= time_is_equal(&metadata[result.r_sample_idx].ts,
						&match_expected_ts);

		/* Declare end of read */
		ret = shd_end_read(ctx_cons, rev);
		CU_ASSERT_EQUAL_FATAL(ret, 0);
	}
	CU_ASSERT_TRUE(b_time);
	CU_ASSERT_TRUE(b_result);
	/* ret should be equal to 0 throughout the loop */
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_adv_read_select_sample_closest(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_CLOSEST
	};
	struct shd_sample_metadata *metadata = NULL;
	struct shd_search_result result;
	struct shd_revision *rev;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("select-sample-closest"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("select-sample-closest"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Write the whole section once
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx_prod,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	struct timespec last_sample_ts, match_expected_ts;
	time_set(&last_sample_ts, sample_meta.ts);

	time_set(&search.date, last_sample_ts);
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE(time_is_equal(&metadata[result.r_sample_idx].ts,
				&last_sample_ts));

	/* Set the search date to be a bit after a given sample (set
	 * CLOSEST_SEARCH_INDEX in the past) */
	time_set(&search.date,
			time_in_past_after(last_sample_ts,
						CLOSEST_SEARCH_INDEX));
	/* The returned sample timestamp should be equal to this value, since
	 * it is the closest one to the required date */
	match_expected_ts = time_in_past(last_sample_ts, CLOSEST_SEARCH_INDEX);
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE(time_is_before(&metadata[result.r_sample_idx].ts,
				&search.date));
	CU_ASSERT_TRUE(time_is_equal(&metadata[result.r_sample_idx].ts,
				&match_expected_ts));

	/* Set the search date to be exactly the same as a given sample (set
	 * CLOSEST_SEARCH_INDEX in the past) */
	time_set(&search.date,
			time_in_past(last_sample_ts, CLOSEST_SEARCH_INDEX));
	/* The returned sample timestamp should be equal to this value, since
	 * it is the closest one to the required date */
	match_expected_ts = time_in_past(last_sample_ts, CLOSEST_SEARCH_INDEX);
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE(time_is_equal(&metadata[result.r_sample_idx].ts,
				&match_expected_ts));

	/* Set the search date to be a bit before a given sample (set
	 * CLOSEST_SEARCH_INDEX in the past) */
	time_set(&search.date,
			time_in_past_before(last_sample_ts,
						CLOSEST_SEARCH_INDEX));
	/* The returned sample timestamp should be equal to this value, since
	 * it is the closest one to the required date */
	match_expected_ts = time_in_past(last_sample_ts, CLOSEST_SEARCH_INDEX);
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE(time_is_after(&metadata[result.r_sample_idx].ts,
				&search.date));
	CU_ASSERT_TRUE(time_is_equal(&metadata[result.r_sample_idx].ts,
				&match_expected_ts));

	/* Close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_adv_read_incomplete_sample_range(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 2,
		.nb_values_before_date = 2,
		.method = SHD_CLOSEST
	};
	struct shd_sample_metadata *metadata = NULL;
	struct shd_search_result result;
	struct shd_revision *rev;
	struct timespec first_sample_ts, last_sample_ts;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("select-incomplete-range"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("select-incomplete-range"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Write the whole section once
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");

		if (index == 0)
			time_set(&first_sample_ts, sample_meta.ts);

		ret += shd_write_new_blob(ctx_prod,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	time_set(&last_sample_ts, sample_meta.ts);

	/**
	 * Set the search date to the penultimate sample. The research should
	 * only return one sample more recent than the requested sample.
	 *
	 * Here is an overview of the shared memory around the researched
	 * sample. It represents the timestamps stored in the section, assuming
	 * the section size is N.
	 *  ------------------------------------------
	 *  | ts_N-3 | ts_N-2 | ts_N-1 | ts_N | ts_1 |
	 *  ------------------------------------------
	 *                         ^
	 *                         |
	 *
	 * shd_select_samples() should match the sample with timestamp ts_N-1.
	 * Because ts_1 is older, only 4 samples should match.
	 */
	time_set(&search.date, time_in_past(last_sample_ts, 1));

	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE(time_is_equal(&metadata[result.r_sample_idx].ts,
				&search.date));
	CU_ASSERT_EQUAL(result.nb_matches, 4);
	CU_ASSERT_EQUAL(result.r_sample_idx, 2);

	/**
	 * Set the search date to second sample. The research should
	 * only return one sample older than the requested sample.
	 *
	 * Here is an overview of the shared memory around the researched
	 * sample. It represents the timestamps stored in the section, assuming
	 * the section size is N.
	 *  ------------------------------------
	 *  | ts_N | ts_1 | ts_2 | ts_3 | ts_4 |
	 *  ------------------------------------
	 *                    ^
	 *                    |
	 *
	 * shd_select_samples() should match the sample with timestamp ts_2.
	 * Because ts_N is more recent, only 4 samples should match.
	 */
	time_set(&search.date, first_sample_ts);
	time_step(&search.date);

	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL_FATAL(ret, 0);
	CU_ASSERT_TRUE(time_is_equal(&metadata[result.r_sample_idx].ts,
				&search.date));
	CU_ASSERT_EQUAL(result.nb_matches, 4);
	CU_ASSERT_EQUAL(result.r_sample_idx, 1);

	/* Close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_adv_read_latest_whole_blob(void)
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
	struct prod_blob *read_blob = NULL;
	struct shd_revision *rev;

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");
	read_blob = calloc(1, sizeof(*read_blob));
	if (read_blob == NULL)
		CU_FAIL_FATAL("Could not allocate blob");

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("read-latest-whole-b"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("read-latest-whole-b"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Produce at least one sample in the shared memory section */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Select latest sample */
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Try to read the whole blob */
	ret = shd_read_quantity(ctx_cons, NULL,
				read_blob, sizeof(*read_blob));
	CU_ASSERT_TRUE(ret >= 0);
	CU_ASSERT_EQUAL(ret, 1);
	CU_ASSERT_EQUAL(read_blob->i1, TEST_VAL_i1);
	CU_ASSERT_EQUAL(read_blob->li1, TEST_VAL_li1);
	CU_ASSERT_EQUAL(read_blob->ui1, TEST_VAL_ui1);
	CU_ASSERT_EQUAL(read_blob->c1, TEST_VAL_c1);
	CU_ASSERT_EQUAL(read_blob->state, TEST_VAL_state);
	CU_ASSERT_DOUBLE_EQUAL(read_blob->f1, TEST_VAL_f1, FLOAT_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob->acc.x, TEST_VAL_acc_x,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob->acc.y, TEST_VAL_acc_y,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob->acc.z, TEST_VAL_acc_z,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob->angles.rho, TEST_VAL_angles_rho,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob->angles.theta, TEST_VAL_angles_theta,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob->angles.phi, TEST_VAL_angles_phi,
				DOUBLE_PRECISION);

	/* Declare end of read */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	free(read_blob);
}

static void test_func_adv_read_several_latest_whole_blob(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = NUMBER_OF_SAMPLES - 1,
		.method = SHD_LATEST
	};
	struct shd_sample_metadata *metadata = NULL;
	struct shd_search_result result;
	struct prod_blob read_blob[NUMBER_OF_SAMPLES];
	struct shd_revision *rev;

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("read-several-latest-whole-b"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("read-several-latest-whole-b"),
			NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Write the whole section once
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx_prod,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Select latest sample */
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Try to read the whole blob */
	ret = shd_read_quantity(ctx_cons, NULL,
				&read_blob, sizeof(read_blob));
	CU_ASSERT_TRUE(ret > 0);
	CU_ASSERT_EQUAL(ret, NUMBER_OF_SAMPLES);
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		CU_ASSERT_EQUAL(read_blob[index].i1, TEST_VAL_i1);
		CU_ASSERT_EQUAL(read_blob[index].li1, TEST_VAL_li1);
		CU_ASSERT_EQUAL(read_blob[index].ui1, TEST_VAL_ui1);
		CU_ASSERT_EQUAL(read_blob[index].c1, TEST_VAL_c1);
		CU_ASSERT_EQUAL(read_blob[index].state, TEST_VAL_state);
		CU_ASSERT_DOUBLE_EQUAL(read_blob[index].f1, TEST_VAL_f1,
					FLOAT_PRECISION);
		CU_ASSERT_DOUBLE_EQUAL(read_blob[index].acc.x, TEST_VAL_acc_x,
					DOUBLE_PRECISION);
		CU_ASSERT_DOUBLE_EQUAL(read_blob[index].acc.y, TEST_VAL_acc_y,
					DOUBLE_PRECISION);
		CU_ASSERT_DOUBLE_EQUAL(read_blob[index].acc.z, TEST_VAL_acc_z,
					DOUBLE_PRECISION);
		CU_ASSERT_DOUBLE_EQUAL(read_blob[index].angles.rho,
					TEST_VAL_angles_rho,
					DOUBLE_PRECISION);
		CU_ASSERT_DOUBLE_EQUAL(read_blob[index].angles.theta,
					TEST_VAL_angles_theta,
					DOUBLE_PRECISION);
		CU_ASSERT_DOUBLE_EQUAL(read_blob[index].angles.phi,
					TEST_VAL_angles_phi,
					DOUBLE_PRECISION);
	}

	/* Declare end of read */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_adv_read_latest_by_quantity(void)
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
	struct angles read_angles;
	int read_i1 = 0;
	long int read_li1 = 0;
	char read_c1 = '\0';
	struct shd_revision *rev;

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("read-latest-quantity"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("read-latest-quantity"), NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Produce at least one sample in the shared memory section */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Select latest sample */
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Read a subset of all the quantities in the blob */
	ret = shd_read_quantity(ctx_cons, &q_s_blob_i1,
				&read_i1, sizeof(read_i1));
	CU_ASSERT_EQUAL(ret, 1);
	CU_ASSERT_EQUAL(read_i1, TEST_VAL_i1);

	ret = shd_read_quantity(ctx_cons, &q_s_blob_c1,
				&read_c1, sizeof(read_c1));
	CU_ASSERT_EQUAL(ret, 1);
	CU_ASSERT_EQUAL(read_c1, TEST_VAL_c1);

	ret = shd_read_quantity(ctx_cons, &q_s_blob_li1,
				&read_li1, sizeof(read_li1));
	CU_ASSERT_EQUAL(ret, 1);
	CU_ASSERT_EQUAL(read_li1, TEST_VAL_li1);

	ret = shd_read_quantity(ctx_cons, &q_s_blob_angles,
				&read_angles, sizeof(read_angles));
	CU_ASSERT_EQUAL(ret, 1);
	CU_ASSERT_DOUBLE_EQUAL(read_angles.rho, TEST_VAL_angles_rho,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_angles.theta, TEST_VAL_angles_theta,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_angles.phi, TEST_VAL_angles_phi,
				DOUBLE_PRECISION);

	/* Declare end of read */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_adv_read_several_latest_by_quantity(void)
{

	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = NUMBER_OF_SAMPLES - 1,
		.method = SHD_LATEST
	};
	struct shd_sample_metadata *metadata = NULL;
	struct shd_search_result result;
	struct angles read_angles[NUMBER_OF_SAMPLES];
	int read_i1[NUMBER_OF_SAMPLES];
	long int read_li1[NUMBER_OF_SAMPLES];
	char read_c1[NUMBER_OF_SAMPLES];
	struct shd_revision *rev;

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("read-several-latest-quantity"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("read-several-latest-quantity"),
			NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Write the whole section once
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx_prod,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Select latest sample */
	ret = shd_select_samples(ctx_cons, &search,
			&metadata, &result);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Read a subset of all the quantities in the blob */
	ret = shd_read_quantity(ctx_cons, &q_s_blob_i1,
				&read_i1, sizeof(read_i1));
	CU_ASSERT_EQUAL(ret, NUMBER_OF_SAMPLES);
	ret = shd_read_quantity(ctx_cons, &q_s_blob_c1,
				&read_c1, sizeof(read_c1));
	CU_ASSERT_EQUAL(ret, NUMBER_OF_SAMPLES);
	ret = shd_read_quantity(ctx_cons, &q_s_blob_li1,
				&read_li1, sizeof(read_li1));
	CU_ASSERT_EQUAL(ret, NUMBER_OF_SAMPLES);
	ret = shd_read_quantity(ctx_cons, &q_s_blob_angles,
				&read_angles, sizeof(read_angles));
	CU_ASSERT_EQUAL(ret, NUMBER_OF_SAMPLES);

	/* Check that the values read are correct */
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		CU_ASSERT_EQUAL(read_i1[index], TEST_VAL_i1);
		CU_ASSERT_EQUAL(read_li1[index], TEST_VAL_li1);
		CU_ASSERT_EQUAL(read_c1[index], TEST_VAL_c1);
		CU_ASSERT_DOUBLE_EQUAL(read_angles[index].rho,
					TEST_VAL_angles_rho,
					DOUBLE_PRECISION);
		CU_ASSERT_DOUBLE_EQUAL(read_angles[index].theta,
					TEST_VAL_angles_theta,
					DOUBLE_PRECISION);
		CU_ASSERT_DOUBLE_EQUAL(read_angles[index].phi,
					TEST_VAL_angles_phi,
					DOUBLE_PRECISION);

	}

	/* Declare end of read */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}


CU_TestInfo s_func_adv_read_tests[] = {
	{(char *)"select latest sample",
			&test_func_adv_read_select_sample_one_latest},
	{(char *)"select many samples by latest search",
			&test_func_adv_read_select_sample_many_latest},
	{(char *)"select samples by \"first after\" search",
			&test_func_adv_read_select_sample_first_after},
	{(char *)"select samples by \"first before\" search",
			&test_func_adv_read_select_sample_first_before},
	{(char *)"select samples by \"closest\" search",
			&test_func_adv_read_select_sample_closest},
	{(char *)"select an incomplete sample range",
			&test_func_adv_read_incomplete_sample_range},
	{(char *)"read latest whole blob",
			&test_func_adv_read_latest_whole_blob},
	{(char *)"read several latest whole blob",
			&test_func_adv_read_several_latest_whole_blob},
	{(char *)"read latest by quantity",
			&test_func_adv_read_latest_by_quantity},
	{(char *)"read several latest by quantity",
			&test_func_adv_read_several_latest_by_quantity},
	CU_TEST_INFO_NULL,
};

