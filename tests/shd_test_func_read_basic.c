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
 * @file shd_test_func_read_basic.c
 *
 * @brief
 *
 */

#include "shd_test.h"
#include "shd_test_helper.h"

/* Buffer where blob data is copied, where data is organized in shuffled
 * order compared to the original blob */
struct dst_buffer {
	struct acceleration acc;
	char c1;
	enum flight_state_t state;
	long int li1;
	unsigned int ui1;
	float f1;
	int i1;
	struct angles angles;
};

static void test_func_basic_read_from_sample_latest(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
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
	ctx_prod = shd_create(BLOB_NAME("basic-select-sample-latest"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("basic-select-sample-latest"),
			NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

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

	/* Read the whole blob */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_TRUE(ret > 0);
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
					&sample_meta.ts));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.exp,
					&sample_meta.exp));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Write the whole section over
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

	/* Read the latest sample again and check if the timestamp read matches
	 * the latest one produced */
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


static void test_func_basic_read_from_sample_oldest(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct prod_blob read_blob;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_metadata oldest_sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_OLDEST
	};
	struct shd_revision *rev;
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) }
	};

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("basic-select-sample-oldest"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("basic-select-sample-oldest"),
			NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&oldest_sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Try to read a sample before any has been produced : this should
	 * fail */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_EQUAL(ret, -EAGAIN);

	/* Produce at least one sample in the shared memory section */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Read the whole blob */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_TRUE(ret > 0);
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
					&oldest_sample_meta.ts));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.exp,
					&oldest_sample_meta.exp));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Keep on writing new samples until the whole section is written,
	 * we should always get the first sample ever produced */
	for (index = 0; index < NUMBER_OF_SAMPLES - 2; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret = shd_write_new_blob(ctx_prod,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
		CU_ASSERT_EQUAL_FATAL(ret, 0);

		ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
						blob_samp);
		CU_ASSERT_TRUE(ret > 0);

		CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
						&oldest_sample_meta.ts));
		CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.exp,
						&oldest_sample_meta.exp));
		ret = shd_end_read(ctx_cons, rev);
		CU_ASSERT_EQUAL(ret, 0);
	}

	/* Now that the section has been written in full, we should always
	 * return the second oldest sample */
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		if (time_step(&oldest_sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret = shd_write_new_blob(ctx_prod,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
		CU_ASSERT_EQUAL_FATAL(ret, 0);

		ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
						blob_samp);
		CU_ASSERT_TRUE(ret > 0);

		CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
						&oldest_sample_meta.ts));
		CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.exp,
						&oldest_sample_meta.exp));
		ret = shd_end_read(ctx_cons, rev);
		CU_ASSERT_EQUAL(ret, 0);
	}

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}


static void test_func_basic_read_from_sample_first_after(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_FIRST_AFTER
	};
	struct shd_revision *rev;
	struct prod_blob read_blob;
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) }
	};
	struct timespec first_sample_ts;
	char blob_name[NAME_MAX];

	CU_ASSERT_TRUE_FATAL(
		get_unique_blob_name(
			BLOB_NAME("basic-select-sample-first-after"),
			blob_name) > 0);

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(blob_name, NULL, &s_hdr_info, &s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(blob_name, NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	time_set(&first_sample_ts, sample_meta.ts);

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

		ret += shd_read_from_sample(ctx_cons, 0, &search, NULL,
						blob_samp);
		if (ret < 0)
			CU_FAIL_FATAL("Read from sample failed");

		b_time &= time_is_after(&blob_samp[0].meta.ts,
						&search.date);
		b_time &= time_is_equal(&blob_samp[0].meta.ts,
						&match_expected_ts);

		ret = shd_end_read(ctx_cons, rev);
		if (ret < 0)
			CU_FAIL_FATAL("End of read failed");
	}
	CU_ASSERT_TRUE(b_time);
	CU_ASSERT_TRUE(b_result);
	/* ret should be equal to 0 throughout the loop */
	CU_ASSERT_EQUAL(ret, 0);

	/* Search for the first sample after TIME_INIT : we should find the
	 * oldest sample */
	time_set(&search.date, TIME_INIT);
	match_expected_ts = time_in_past(last_sample_ts, NUMBER_OF_SAMPLES - 1);

	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_TRUE(ret > 0);
	CU_ASSERT_TRUE(time_is_after(&blob_samp[0].meta.ts,
					&search.date));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
					&match_expected_ts));

	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Write yet another sample in the section that will overwrite the
	 * first one that was produced : by now the first sample has been
	 * written twice, while all the other have been written only once. This
	 * should not change anything regarding sample validity */
	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	CU_ASSERT_EQUAL_FATAL(shd_write_new_blob(ctx_prod, &s_blob,
					sizeof(s_blob), &sample_meta), 0);
	time_set(&last_sample_ts, sample_meta.ts);

	/* We're still looking for the oldest sample in the section, which
	 * is now 1 tick more recent than in the previous search */
	match_expected_ts = time_in_past(last_sample_ts, NUMBER_OF_SAMPLES - 1);

	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL, blob_samp);
	CU_ASSERT_EQUAL_FATAL(ret, 1);
	CU_ASSERT_TRUE(time_is_after(&blob_samp[0].meta.ts,
				&search.date));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
				&match_expected_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Search for the first sample after a date set in the future of the
	 * production date of the last sample : we should find nothing */
	time_set(&search.date, last_sample_ts);
	time_step(&search.date);
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	/* Close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_basic_read_from_sample_first_before(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_FIRST_BEFORE
	};
	struct shd_revision *rev;
	struct prod_blob read_blob;
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) }
	};
	struct timespec first_sample_ts;
	char blob_name[NAME_MAX];

	CU_ASSERT_TRUE_FATAL(
		get_unique_blob_name(
			BLOB_NAME("basic-select-sample-first-before"),
			blob_name) > 0);

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(blob_name, NULL, &s_hdr_info, &s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(blob_name, NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	time_set(&first_sample_ts, sample_meta.ts);

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

		ret += shd_read_from_sample(ctx_cons, 0, &search, NULL,
						blob_samp);
		if (ret < 0)
			CU_FAIL_FATAL("Read from sample failed");

		b_time &= time_is_before(&blob_samp[0].meta.ts,
						&search.date);
		b_time &= time_is_equal(&blob_samp[0].meta.ts,
						&match_expected_ts);

		ret = shd_end_read(ctx_cons, rev);
		if (ret < 0)
			CU_FAIL_FATAL("End of read failed");
	}
	CU_ASSERT_TRUE(b_time);
	CU_ASSERT_TRUE(b_result);
	/* ret should be equal to 0 throughout the loop */
	CU_ASSERT_EQUAL(ret, 0);

	/* Search for the first sample before a date set before the first
	 * sample still in memory : we should find none */
	time_set(&search.date,
			time_in_past_after(last_sample_ts,
						NUMBER_OF_SAMPLES + 1));

	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	/* Search for the first sample before a date set after the last sample
	 * still in memory : we should find this last sample */
	time_set(&search.date, last_sample_ts);
	time_step(&search.date);
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_TRUE(ret > 0);

	CU_ASSERT_TRUE(time_is_before(&blob_samp[0].meta.ts,
					&search.date));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
					&last_sample_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Write yet another sample in the section that will overwrite the
	 * first one that was produced : by now the first sample has been
	 * written twice, while all the other have been written only once. This
	 * should not change anything regarding sample validity */
	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	CU_ASSERT_EQUAL_FATAL(shd_write_new_blob(ctx_prod, &s_blob,
					sizeof(s_blob), &sample_meta), 0);
	time_set(&last_sample_ts, sample_meta.ts);

	/* We're still looking for the oldest sample in the section, which
	 * is now 1 tick more recent than in the previous search */
	match_expected_ts = time_in_past(last_sample_ts, NUMBER_OF_SAMPLES - 1);
	search.date = time_in_past_after(match_expected_ts, 0);

	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL, blob_samp);
	CU_ASSERT_EQUAL_FATAL(ret, 1);
	CU_ASSERT_TRUE(time_is_before(&blob_samp[0].meta.ts,
				&search.date));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
				&match_expected_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_basic_read_from_sample_closest(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_CLOSEST
	};
	struct shd_revision *rev;
	struct prod_blob read_blob;
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) }
	};
	char blob_name[NAME_MAX];

	CU_ASSERT_TRUE_FATAL(
		get_unique_blob_name(BLOB_NAME("basic-select-sample-closest"),
					blob_name) > 0);

	/* Create a producer and a consumer context to play with, in a newly
	 * created memory section, so that the way the section is filled depends
	 * only on what is done in this test */
	ctx_prod = shd_create(blob_name, NULL, &s_hdr_info, &s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(blob_name, NULL, &rev);
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

	struct timespec last_sample_ts, match_expected_ts;
	time_set(&last_sample_ts, sample_meta.ts);

	/* Set the search date to be exactly that of the latest sample
	 * The returned sample should be the latest one */
	time_set(&search.date, last_sample_ts);
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL, blob_samp);
	CU_ASSERT_EQUAL_FATAL(ret, 1);
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
				&last_sample_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Set the search date to be a bit after the latest sample
	 * The returned sample should be the latest one */
	time_step(&search.date);
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL, blob_samp);
	CU_ASSERT_EQUAL_FATAL(ret, 1);
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
				&last_sample_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Set the search date to be a bit after a given sample (set
	 * CLOSEST_SEARCH_INDEX in the past) */
	time_set(&search.date,
			time_in_past_after(last_sample_ts,
						CLOSEST_SEARCH_INDEX));
	/* The returned sample timestamp should be equal to this value, since
	 * it is the closest one to the required date */
	match_expected_ts = time_in_past(last_sample_ts, CLOSEST_SEARCH_INDEX);
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL, blob_samp);
	CU_ASSERT_EQUAL_FATAL(ret, 1);
	CU_ASSERT_TRUE(time_is_before(&blob_samp[0].meta.ts,
				&search.date));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
				&match_expected_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Set the search date to be exactly the same as a given sample (set
	 * CLOSEST_SEARCH_INDEX in the past) */
	time_set(&search.date,
			time_in_past(last_sample_ts, CLOSEST_SEARCH_INDEX));
	/* The returned sample timestamp should be equal to this value, since
	 * it is the closest one to the required date */
	match_expected_ts = time_in_past(last_sample_ts, CLOSEST_SEARCH_INDEX);

	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL, blob_samp);
	CU_ASSERT_EQUAL_FATAL(ret, 1);
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
				&match_expected_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Set the search date to be a bit before a given sample (set
	 * CLOSEST_SEARCH_INDEX in the past) */
	time_set(&search.date,
			time_in_past_before(last_sample_ts,
						CLOSEST_SEARCH_INDEX));
	/* The returned sample timestamp should be equal to this value, since
	 * it is the closest one to the required date */
	match_expected_ts = time_in_past(last_sample_ts, CLOSEST_SEARCH_INDEX);

	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL, blob_samp);
	CU_ASSERT_EQUAL_FATAL(ret, 1);
	CU_ASSERT_TRUE(time_is_after(&blob_samp[0].meta.ts,
				&search.date));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
				&match_expected_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Set the search date to be a bit after the oldest sample */
	time_set(&search.date,
			time_in_past_after(last_sample_ts,
						NUMBER_OF_SAMPLES - 1));
	/* The returned sample timestamp should be equal to the date of the
	 * oldest sample */
	match_expected_ts = time_in_past(last_sample_ts, NUMBER_OF_SAMPLES - 1);

	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL, blob_samp);
	CU_ASSERT_EQUAL_FATAL(ret, 1);
	CU_ASSERT_TRUE(time_is_before(&blob_samp[0].meta.ts,
				&search.date));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
				&match_expected_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Set the search date to be a bit before the oldest sample */
	time_set(&search.date,
			time_in_past_before(last_sample_ts,
						NUMBER_OF_SAMPLES - 1));
	/* The returned sample timestamp should be equal to the date of the
	 * oldest sample */
	match_expected_ts = time_in_past(last_sample_ts, NUMBER_OF_SAMPLES - 1);

	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL, blob_samp);
	CU_ASSERT_EQUAL_FATAL(ret, 1);
	CU_ASSERT_TRUE(time_is_after(&blob_samp[0].meta.ts,
				&search.date));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
				&match_expected_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Set the search date to be the oldest possible */
	time_set(&search.date, TIME_INIT);
	/* The returned sample timestamp should be equal to the date of the
	 * oldest sample */
	match_expected_ts = time_in_past(last_sample_ts, NUMBER_OF_SAMPLES - 1);

	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL, blob_samp);
	CU_ASSERT_EQUAL_FATAL(ret, 1);
	CU_ASSERT_TRUE(time_is_after(&blob_samp[0].meta.ts,
				&search.date));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
				&match_expected_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Write yet another sample in the section that will overwrite the
	 * first one that was produced : by now the first sample has been
	 * written twice, while all the other have been written only once. This
	 * should not change anything regarding sample validity */
	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	CU_ASSERT_EQUAL_FATAL(shd_write_new_blob(ctx_prod, &s_blob,
					sizeof(s_blob), &sample_meta), 0);
	time_set(&last_sample_ts, sample_meta.ts);

	/* We're still looking for the oldest sample in the section, which
	 * is now 1 tick more recent than in the previous search */
	match_expected_ts = time_in_past(last_sample_ts, NUMBER_OF_SAMPLES - 1);

	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL, blob_samp);
	CU_ASSERT_EQUAL_FATAL(ret, 1);
	CU_ASSERT_TRUE(time_is_after(&blob_samp[0].meta.ts,
				&search.date));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
				&match_expected_ts));
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_basic_read_from_sample_blob_data(void)
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

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("func-read-from-sample-blob"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("func-read-from-sample-blob"),
				NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Produce at least one sample in the shared memory section */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Read the whole blob */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_TRUE(ret > 0);
	CU_ASSERT_EQUAL(read_blob.i1, TEST_VAL_i1);
	CU_ASSERT_EQUAL(read_blob.li1, TEST_VAL_li1);
	CU_ASSERT_EQUAL(read_blob.ui1, TEST_VAL_ui1);
	CU_ASSERT_EQUAL(read_blob.c1, TEST_VAL_c1);
	CU_ASSERT_EQUAL(read_blob.state, TEST_VAL_state);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.f1, TEST_VAL_f1, FLOAT_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.acc.x, TEST_VAL_acc_x,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.acc.y, TEST_VAL_acc_y,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.acc.z, TEST_VAL_acc_z,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.angles.rho, TEST_VAL_angles_rho,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.angles.theta, TEST_VAL_angles_theta,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.angles.phi, TEST_VAL_angles_phi,
				DOUBLE_PRECISION);

	/* This call is valid */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_basic_read_from_sample_quantity_data(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct prod_blob read_blob;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	struct shd_quantity_sample q_samp[QUANTITIES_NB_IN_BLOB] = {
		QTY_SAMPLE_FIELD(read_blob, i1),
		QTY_SAMPLE_FIELD(read_blob, c1),
		QTY_SAMPLE_FIELD(read_blob, li1),
		QTY_SAMPLE_FIELD(read_blob, ui1),
		QTY_SAMPLE_FIELD(read_blob, f1),
		QTY_SAMPLE_FIELD(read_blob, acc),
		QTY_SAMPLE_FIELD(read_blob, angles),
		QTY_SAMPLE_FIELD(read_blob, state),
	};
	const struct shd_quantity quantity[QUANTITIES_NB_IN_BLOB] = {
		QUANTITY_FIELD(prod_blob, s_blob, i1),
		QUANTITY_FIELD(prod_blob, s_blob, c1),
		QUANTITY_FIELD(prod_blob, s_blob, li1),
		QUANTITY_FIELD(prod_blob, s_blob, ui1),
		QUANTITY_FIELD(prod_blob, s_blob, f1),
		QUANTITY_FIELD(prod_blob, s_blob, acc),
		QUANTITY_FIELD(prod_blob, s_blob, angles),
		QUANTITY_FIELD(prod_blob, s_blob, state),
	};
	struct shd_revision *rev;

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("func-read-from-sample-blob"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("func-read-from-sample-blob"),
				NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Produce at least one sample in the shared memory section */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Read the whole blob */
	ret = shd_read_from_sample(ctx_cons, 8, &search, quantity,
					q_samp);
	CU_ASSERT_TRUE(ret > 0);

	/* Check that the metadata for every quantity is correct */
	for (index = 0; index < QUANTITIES_NB_IN_BLOB; index++) {
		CU_ASSERT_TRUE(time_is_equal(&q_samp[index].meta.ts,
						&sample_meta.ts));
		CU_ASSERT_TRUE(time_is_equal(&q_samp[index].meta.exp,
						&sample_meta.exp));
	}

	/* Check that every quantity read is correct */
	CU_ASSERT_EQUAL(read_blob.i1, TEST_VAL_i1);
	CU_ASSERT_EQUAL(read_blob.li1, TEST_VAL_li1);
	CU_ASSERT_EQUAL(read_blob.ui1, TEST_VAL_ui1);
	CU_ASSERT_EQUAL(read_blob.c1, TEST_VAL_c1);
	CU_ASSERT_EQUAL(read_blob.state, TEST_VAL_state);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.f1, TEST_VAL_f1, FLOAT_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.acc.x, TEST_VAL_acc_x,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.acc.y, TEST_VAL_acc_y,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.acc.z, TEST_VAL_acc_z,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.angles.rho, TEST_VAL_angles_rho,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.angles.theta, TEST_VAL_angles_theta,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.angles.phi, TEST_VAL_angles_phi,
				DOUBLE_PRECISION);

	/* This call is valid */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_basic_read_from_sample_quantity_shuffled_data(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret;
	struct dst_buffer dst_buffer;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	struct shd_quantity_sample q_samp[QUANTITIES_NB_IN_BLOB] = {
		QTY_SAMPLE_FIELD(dst_buffer, i1),
		QTY_SAMPLE_FIELD(dst_buffer, c1),
		QTY_SAMPLE_FIELD(dst_buffer, li1),
		QTY_SAMPLE_FIELD(dst_buffer, ui1),
		QTY_SAMPLE_FIELD(dst_buffer, f1),
		QTY_SAMPLE_FIELD(dst_buffer, acc),
		QTY_SAMPLE_FIELD(dst_buffer, angles),
		QTY_SAMPLE_FIELD(dst_buffer, state),
	};
	const struct shd_quantity quantity[8] = {
		QUANTITY_FIELD(prod_blob, s_blob, i1),
		QUANTITY_FIELD(prod_blob, s_blob, c1),
		QUANTITY_FIELD(prod_blob, s_blob, li1),
		QUANTITY_FIELD(prod_blob, s_blob, ui1),
		QUANTITY_FIELD(prod_blob, s_blob, f1),
		QUANTITY_FIELD(prod_blob, s_blob, acc),
		QUANTITY_FIELD(prod_blob, s_blob, angles),
		QUANTITY_FIELD(prod_blob, s_blob, state),
	};
	struct shd_revision *rev;

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("func-read-from-sample-blob"), NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("func-read-from-sample-blob"),
				NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/* Produce at least one sample in the shared memory section */
	ret = shd_write_new_blob(ctx_prod,
					&s_blob,
					sizeof(s_blob),
					&sample_meta);
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Read the whole blob */
	ret = shd_read_from_sample(ctx_cons, 8, &search, quantity,
					q_samp);
	CU_ASSERT_TRUE(ret > 0);
	CU_ASSERT_EQUAL(dst_buffer.i1, TEST_VAL_i1);
	CU_ASSERT_EQUAL(dst_buffer.li1, TEST_VAL_li1);
	CU_ASSERT_EQUAL(dst_buffer.ui1, TEST_VAL_ui1);
	CU_ASSERT_EQUAL(dst_buffer.c1, TEST_VAL_c1);
	CU_ASSERT_EQUAL(dst_buffer.state, TEST_VAL_state);
	CU_ASSERT_DOUBLE_EQUAL(dst_buffer.f1, TEST_VAL_f1, FLOAT_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(dst_buffer.acc.x, TEST_VAL_acc_x,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(dst_buffer.acc.y, TEST_VAL_acc_y,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(dst_buffer.acc.z, TEST_VAL_acc_z,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(dst_buffer.angles.rho, TEST_VAL_angles_rho,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(dst_buffer.angles.theta, TEST_VAL_angles_theta,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(dst_buffer.angles.phi, TEST_VAL_angles_phi,
				DOUBLE_PRECISION);

	/* This call is valid */
	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close should unfold normally */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}

static void test_func_basic_read_change_blob_structure(void)
{
	struct shd_ctx *ctx_prod, *ctx_cons;
	int ret, index;
	struct prod_blob read_blob;
	struct prod_blob_alt_size read_blob_alt_size;
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
	struct shd_quantity_sample blob_samp_alt_size[1] = {
		{
			.ptr = &read_blob_alt_size,
			.size = sizeof(read_blob_alt_size)
		}
	};
	struct shd_hdr_user_info s_hdr_alt_info;

	/* Create a producer and a consumer context to play with */
	ctx_prod = shd_create(BLOB_NAME("basic-read-change-blob-structure"),
				NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("basic-read-change-blob-structure"),
				NULL,
				&rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Write the whole section
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

	/* Read the latest sample again and check if the timestamp read matches
	 * the latest one produced */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
	CU_ASSERT_TRUE(ret > 0);
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.ts,
					&sample_meta.ts));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp[0].meta.exp,
					&sample_meta.exp));
	CU_ASSERT_EQUAL(read_blob.i1, TEST_VAL_i1);
	CU_ASSERT_EQUAL(read_blob.li1, TEST_VAL_li1);
	CU_ASSERT_EQUAL(read_blob.ui1, TEST_VAL_ui1);
	CU_ASSERT_EQUAL(read_blob.c1, TEST_VAL_c1);
	CU_ASSERT_EQUAL(read_blob.state, TEST_VAL_state);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.f1, TEST_VAL_f1, FLOAT_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.acc.x, TEST_VAL_acc_x,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.acc.y, TEST_VAL_acc_y,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.acc.z, TEST_VAL_acc_z,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.angles.rho, TEST_VAL_angles_rho,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.angles.theta, TEST_VAL_angles_theta,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob.angles.phi, TEST_VAL_angles_phi,
				DOUBLE_PRECISION);

	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close both producer and consumer contexts */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Change blob structure within the producer and repeat all the former
	 * operations : everything should work the same exact way */
	memcpy(&s_hdr_alt_info, &s_hdr_info, sizeof(s_hdr_info));
	s_hdr_alt_info.blob_size = sizeof(s_blob_alt_size);
	ctx_prod = shd_create(BLOB_NAME("basic-read-change-blob-structure"),
				NULL,
				&s_hdr_alt_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_prod);
	ctx_cons = shd_open(BLOB_NAME("basic-read-change-blob-structure"),
				NULL,
				&rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx_cons);

	/*
	 * Before re-creation of the section, it had been filled with valid
	 * samples that should not be visible now that the section has been
	 * created again by the producer. So we setup a search that is supposed
	 * to fail, but would succeed if the section somehow kept a "memory"
	 * of its past state
	 */
	struct shd_sample_search search_before = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.date = sample_meta.ts,
		.method = SHD_FIRST_BEFORE
	};
	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Write at least one new sample, so that we don't fall in the "no
	 * sample produced yet" case */
	ret = shd_write_new_blob(ctx_prod,
			&s_blob_alt_size,
			sizeof(s_blob_alt_size),
			&sample_meta);
	CU_ASSERT_EQUAL(ret, 0);

	/* Try to read a sample that could only be found if the section kept
	 * an history of the samples */
	ret = shd_read_from_sample(ctx_cons, 0, &search_before, NULL,
			blob_samp_alt_size);
	CU_ASSERT_EQUAL(ret, -ENOENT);

	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");
	if (time_step(&search.date) < 0)
		CU_FAIL_FATAL("Could not get time");

	/* Write the whole section
	 * shd_write_new_blob should always return 0 */
	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES * 3/2; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx_prod,
						&s_blob_alt_size,
						sizeof(s_blob_alt_size),
						&sample_meta);
	}
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Read the latest sample again and check if the timestamp read matches
	 * the latest one produced */
	ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp_alt_size);
	CU_ASSERT_TRUE(ret > 0);
	CU_ASSERT_TRUE(time_is_equal(&blob_samp_alt_size[0].meta.ts,
					&sample_meta.ts));
	CU_ASSERT_TRUE(time_is_equal(&blob_samp_alt_size[0].meta.exp,
					&sample_meta.exp));
	CU_ASSERT_EQUAL(read_blob_alt_size.i1, TEST_VAL_i1);
	CU_ASSERT_EQUAL(read_blob_alt_size.li1, TEST_VAL_li1);
	CU_ASSERT_EQUAL(read_blob_alt_size.c1, TEST_VAL_c1);
	CU_ASSERT_EQUAL(read_blob_alt_size.state, TEST_VAL_state);
	CU_ASSERT_DOUBLE_EQUAL(read_blob_alt_size.angles.rho,
				TEST_VAL_angles_rho,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob_alt_size.angles.theta,
				TEST_VAL_angles_theta,
				DOUBLE_PRECISION);
	CU_ASSERT_DOUBLE_EQUAL(read_blob_alt_size.angles.phi,
				TEST_VAL_angles_phi,
				DOUBLE_PRECISION);

	ret = shd_end_read(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);

	/* Close both producer and consumer contexts */
	ret = shd_close(ctx_prod, NULL);
	CU_ASSERT_EQUAL(ret, 0);
	ret = shd_close(ctx_cons, rev);
	CU_ASSERT_EQUAL(ret, 0);
}


CU_TestInfo s_func_basic_read_tests[] = {
	{(char *)"read from latest sample",
		&test_func_basic_read_from_sample_latest},
	{(char *)"read from oldest sample",
		&test_func_basic_read_from_sample_oldest},
	{(char *)"read from sample by \"first after\" search",
		&test_func_basic_read_from_sample_first_after},
	{(char *)"read from sample by \"first before\" search",
		&test_func_basic_read_from_sample_first_before},
	{(char *)"read from sample by \"closest\" search",
		&test_func_basic_read_from_sample_closest},
	{(char *)"read a blob using read from sample",
		&test_func_basic_read_from_sample_blob_data},
	{(char *)"read all quantities in order using read from sample",
		&test_func_basic_read_from_sample_quantity_data},
	{(char *)"read all quantities in shuffled order using read from sample",
		&test_func_basic_read_from_sample_quantity_shuffled_data},
	{(char *)"read from sample and then change blob structure",
		&test_func_basic_read_change_blob_structure},
	CU_TEST_INFO_NULL,

};
