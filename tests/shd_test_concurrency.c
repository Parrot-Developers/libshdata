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
 * @file shd_test_concurrency.c
 *
 * @brief Implement tests for concurrency
 *
 */

#define _GNU_SOURCE
#include <stddef.h>
#include <pthread.h>
#include "shd_test.h"
#include "shd_test_helper.h"
#include <string.h>
#include <errno.h>
#include "concurrency/hooks_implem.h"

/*
 * Test concurrent section creation in two separate threads
 */
static void *create_section(void *args)
{
	struct shd_ctx *ctx;

	ctx = shd_create(BLOB_NAME("concurrency-simultaneous-creation"), NULL,
					&s_hdr_info,
					&s_metadata_hdr);

	/* One of the threads should fail in creating its section. In this case,
	 * just do as if it had finished its job, so that the other thread can
	 * finish as well. */
	if (!ctx)
		shd_concurrency_emulate_action_complete();

	return (void *)ctx;
}

static void test_concurrency_simultaneous_creation(void)
{
	pthread_t prod_thread[2];
	void *prod_ret[2];
	int i = 0;

	shd_concurrency_clean_hooks();

	/* The first thread to reach the "unlock" hook will wait for both
	 * threads to have either taken the lock or unsuccessfully completed
	 * before unlocking the section */
	shd_concurrency_set_hook_strategy(HOOK_SECTION_CREATED_LOCK_TAKEN,
					PARALLEL_BOTH_THREADS_SIGNAL_ACTION);
	shd_concurrency_set_hook_strategy(HOOK_SECTION_CREATED_BEFORE_UNLOCK,
					PARALLEL_BOTH_THREADS_WAIT_ALL_ACTIONS_COMPLETE);

	for (i = 0; i < 2; i++)
		pthread_create(&prod_thread[i], NULL,
					&create_section, NULL);

	for (i = 0; i < 2; i++)
		pthread_join(prod_thread[i], &prod_ret[i]);

	/* The first thread to attempt the creation should succeed, while the
	 * other should fail. Both threads have a perfectly symmetrical role so
	 * we obviously don't know which one "won the race". */
	CU_ASSERT_TRUE(prod_ret[0] == NULL || prod_ret[1] == NULL);

	shd_concurrency_clean_hooks();
}

/*
 * Test section open in a thread while the section is being created in another
 * thread in two cases :
 *   - when the section was already existing in memory (and the producer only
 *   re-opens it)
 *   - when the section was not already existing (and the producer REALLY
 *   creates it)
 */
struct thread_open_during_create_args {
	volatile uint32_t *prod_joined_flag;
	char *blob_name;
};

static void *create_section_2(void *args)
{
	struct shd_ctx *ctx;
	struct thread_open_during_create_args *myArgs = args;

	ctx = shd_create(myArgs->blob_name, NULL,
					&s_hdr_info,
					&s_metadata_hdr);
	return (void *)ctx;
}

static void *open_section_then_reopen(void *args)
{
	struct shd_ctx *ctx;
	struct shd_revision *rev;
	struct thread_open_during_create_args *myArgs = args;

	/* Try to open the section : the section already exists so most of the
	 * time this should succeed, but since an operation is going on on
	 * producer-side, the result is not specified. */
	ctx = shd_open(myArgs->blob_name, NULL, &rev);

	/* If the open has failed, do as if it had completed successfully, so
	 * that the producer can complete its job */
	if (!ctx)
		shd_concurrency_emulate_thread_completion(1);

	/* Wait for the producer to have properly created the section, and try
	 * to reopen it : it should succeed this time */
	while(*(myArgs->prod_joined_flag) == 0);
	ctx = shd_open(myArgs->blob_name, NULL, &rev);

	return (void *)ctx;
}

static void test_concurrency_open_during_creation_of_already_existing_section(void)
{
	pthread_t prod_thread, cons_thread;
	void *prod_ret, *cons_ret;
	struct thread_open_during_create_args thread_args[2];
	uint32_t prod_joined_flag;
	char common_blob_name[NAME_MAX] =
			BLOB_NAME("concurrency-open-during-further-creation");
	struct shd_ctx *ctx;
	int i = 0;

	shd_concurrency_clean_hooks();

	prod_joined_flag = 0;
	for (i = 0; i < 2; i++) {
		thread_args[i].prod_joined_flag = &prod_joined_flag;
		thread_args[i].blob_name = common_blob_name;
	}

	/* Make sure that the section has been created at least once */
	ctx = shd_create(common_blob_name, NULL, &s_hdr_info, &s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);
	CU_ASSERT_EQUAL_FATAL(shd_close(ctx, NULL), 0);

	/* Producer thread does not resize the section until the consumer thread
	 * has properly open it and attempted to mmap the memory ... or failed
	 * trying to */
	shd_concurrency_set_hook_strategy(HOOK_SECTION_CREATED_NOT_RESIZED, FIRST_THREAD_SIGNALS_FIRST_ACTION);
	shd_concurrency_set_hook_strategy(HOOK_SECTION_OPEN_START, SECOND_THREAD_WAITS_FIRST_THREAD_FIRST_ACTION);
	shd_concurrency_set_hook_strategy(HOOK_SECTION_OPEN_MMAP_DONE, SECOND_THREAD_SIGNALS_SECOND_ACTION);
	shd_concurrency_set_hook_strategy(HOOK_SECTION_CREATED_BEFORE_TRUNCATE, FIRST_THREAD_WAITS_SECOND_THREAD_SECOND_ACTION);

	pthread_create(&prod_thread, NULL,
				&create_section_2, &thread_args[0]);
	pthread_create(&cons_thread, NULL,
				&open_section_then_reopen, &thread_args[1]);

	pthread_join(prod_thread, &prod_ret);
	prod_joined_flag = 1;
	pthread_join(cons_thread, &cons_ret);

	/* The producer should always succeed in creating its section */
	CU_ASSERT_PTR_NOT_NULL(prod_ret);
	/* The consumer should eventually succeed in opening the section */
	CU_ASSERT_PTR_NOT_NULL(cons_ret);

	shd_concurrency_clean_hooks();
}

static void *open_section_fail_first_then_reopen(void *args)
{
	struct shd_ctx *ctx;
	struct shd_revision *rev;
	struct thread_open_during_create_args *myArgs = args;

	/* Try to open the section : in the conditions of the test, the section
	 * doesn't exist yet so this should fail in all cases */
	ctx = shd_open(myArgs->blob_name, NULL, &rev);


	if (ctx) {
		/* If the open didn't fail, return an error */
		return NULL;
	} else {
		/* Else, do as if it had completed its task, so that the
		 * producer can achieve the section creation */
		shd_concurrency_emulate_thread_completion(1);

		/* ... and wait for the producer to have finished its task. The
		 * open should succeed this time */
		while(*(myArgs->prod_joined_flag) == 0);
		ctx = shd_open(myArgs->blob_name, NULL, &rev);
	}

	return (void *)ctx;
}

static void test_concurrency_open_during_first_creation(void)
{
	pthread_t prod_thread, cons_thread;
	void *prod_ret, *cons_ret;
	struct thread_open_during_create_args thread_args[2];
	uint32_t prod_joined_flag;
	char unique_blob_name[NAME_MAX];
	time_t epoch = time(NULL);
	int i = 0;

	shd_concurrency_clean_hooks();

	/* Producer thread does not resize the section until the consumer thread
	 * has properly open it and attempted to mmap the memory ... or failed
	 * trying to */
	shd_concurrency_set_hook_strategy(HOOK_SECTION_CREATED_NOT_RESIZED, FIRST_THREAD_SIGNALS_FIRST_ACTION);
	shd_concurrency_set_hook_strategy(HOOK_SECTION_OPEN_START, SECOND_THREAD_WAITS_FIRST_THREAD_FIRST_ACTION);
	shd_concurrency_set_hook_strategy(HOOK_SECTION_OPEN_MMAP_DONE, SECOND_THREAD_SIGNALS_SECOND_ACTION);
	shd_concurrency_set_hook_strategy(HOOK_SECTION_CREATED_BEFORE_TRUNCATE, FIRST_THREAD_WAITS_SECOND_THREAD_SECOND_ACTION);

	/* Forge a name guaranteed to be unique if the test is executed with
	 * a period of more than one second */
	snprintf(unique_blob_name, NAME_MAX, "%s-%ju",
			BLOB_NAME("concurrency-open-during-first-creation"),
			(uintmax_t)epoch);

	prod_joined_flag = 0;
	for (i = 0; i < 2; i++) {
		thread_args[i].prod_joined_flag = &prod_joined_flag;
		thread_args[i].blob_name = unique_blob_name;
	}

	pthread_create(&prod_thread, NULL,
				&create_section_2, &thread_args[0]);
	pthread_create(&cons_thread, NULL,
				&open_section_fail_first_then_reopen, &thread_args[1]);

	pthread_join(prod_thread, &prod_ret);
	prod_joined_flag = 1;
	pthread_join(cons_thread, &cons_ret);

	/* The producer should always succeed in creating its section */
	CU_ASSERT_PTR_NOT_NULL(prod_ret);
	/* The consumer should eventually succeed in opening the section */
	CU_ASSERT_PTR_NOT_NULL(cons_ret);

	shd_concurrency_clean_hooks();
}

/*
 * Test simultaneous write on a given sample. Two producers happen to have
 * successfully created the same memory section, and try to write a new sample
 * in it at the same time : at least one should fail
 */
struct thread_write_section_args {
	volatile uint32_t *one_section_created;
	volatile uint32_t *both_sections_created;
};

static int try_to_write_blob(struct shd_ctx *ctx)
{
	int ret;
	struct shd_sample_metadata sample_meta = METADATA_INIT;

	/* Try to write the blob */
	ret = shd_write_new_blob(ctx, &s_blob, sizeof(s_blob), &sample_meta);

	/* If the write fails (due to the other thread writing the sample as
	 * well), do as if it had completed successfully to allow the other
	 * thread to progress */
	if (ret < 0)
		shd_concurrency_emulate_action_complete();

	return ret;
}

static void *write_simultaneously_0(void *args)
{
	struct shd_ctx *ctx;
	struct thread_write_section_args *myArgs = args;

	ctx = shd_create(BLOB_NAME("concurrency-simultaneous-write"), NULL,
					&s_hdr_info,
					&s_metadata_hdr);
	*(myArgs->one_section_created) = 1;

	/* Wait for the other thread to have created its own memory section
	 * as well*/
	while(*(myArgs->both_sections_created) == 0);

	return (void *) try_to_write_blob(ctx);
}

static void *write_simultaneously_1(void *args)
{
	struct shd_ctx *ctx;
	struct thread_write_section_args *myArgs = args;

	/* Wait for the other thread to have created its memory section */
	while(*(myArgs->one_section_created) == 0);

	ctx = shd_create(BLOB_NAME("concurrency-simultaneous-write"), NULL,
					&s_hdr_info,
					&s_metadata_hdr);
	*(myArgs->both_sections_created) = 1;

	return (void *) try_to_write_blob(ctx);
}

static void test_concurrency_simultaneous_write(void)
{
	pthread_t prod_thread[2];
	void *prod_ret[2];
	struct thread_write_section_args thread_args[2];
	uint32_t section_created = 0,
			both_sections_created = 0;
	int i = 0;

	for (i = 0; i < 2; i++) {
		thread_args[i].one_section_created = &section_created;
		thread_args[i].both_sections_created = &both_sections_created;
	}

	shd_concurrency_clean_hooks();
	/* The first thread to reach the "commit" stage while writing a sample
	 * won't do anything before the other one has started writing a sample
	 * on its side */
	shd_concurrency_set_hook_strategy(HOOK_SAMPLE_WRITE_START,
						PARALLEL_BOTH_THREADS_SIGNAL_ACTION);
	shd_concurrency_set_hook_strategy(HOOK_SAMPLE_WRITE_BEFORE_COMMIT,
						PARALLEL_BOTH_THREADS_WAIT_ALL_ACTIONS_COMPLETE);

	pthread_create(&prod_thread[0], NULL,
				&write_simultaneously_0, &thread_args[0]);
	pthread_create(&prod_thread[1], NULL,
				&write_simultaneously_1, &thread_args[1]);

	for (i = 0; i < 2; i++)
		pthread_join(prod_thread[i], &prod_ret[i]);

	CU_ASSERT_TRUE(((int) prod_ret[0] == 0 && (int) prod_ret[1] == -EPERM)
			|| ((int) prod_ret[0] == -EPERM && (int) prod_ret[1] == 0));

	shd_concurrency_clean_hooks();
}

/*
 * Test alternative write in a given section. Two producers happen to have
 * successfully created the same memory section, and try to write a new sample
 * in it alternatively : at least one of them should fail
 */
struct thread_2_producers_args {
	volatile uint32_t *prod0_created_context;
	volatile uint32_t *prod1_created_context;
	volatile uint32_t *prod0_has_written_once;
	volatile uint32_t *prod1_has_written_once;
};

static void *write_alternatively_0(void *args)
{
	struct shd_ctx *ctx;
	struct thread_2_producers_args *myArgs = args;
	int ret;
	struct shd_sample_metadata sample_meta = METADATA_INIT;

	ctx = shd_create(BLOB_NAME("concurrency-write-in-same-section"), NULL,
					&s_hdr_info,
					&s_metadata_hdr);
	*(myArgs->prod0_created_context) = 1;

	/* Wait for the 2nd producer to have created its own context on the same
	 * section */
	while(*(myArgs->prod1_created_context) == 0);

	ret = shd_write_new_blob(ctx,
				&s_blob,
				sizeof(s_blob),
				&sample_meta);

	*(myArgs->prod0_has_written_once) = 1;

	/* Wait for the other thread to have written at least once, now the
	 * current buffer index is 2 when it should be one if the thread was
	 * the only one to write in the section */
	while(*(myArgs->prod1_has_written_once) == 0);
	ret = shd_write_new_blob(ctx,
				&s_blob,
				sizeof(s_blob),
				&sample_meta);

	return (void *)ret;
}

static void *write_alternatively_1(void *args)
{
	struct shd_ctx *ctx;
	int ret;
	struct thread_2_producers_args *myArgs = args;
	struct shd_sample_metadata sample_meta = METADATA_INIT;

	while(*(myArgs->prod0_created_context) == 0);
	ctx = shd_create(BLOB_NAME("concurrency-write-in-same-section"), NULL,
					&s_hdr_info,
					&s_metadata_hdr);
	*(myArgs->prod1_created_context) = 1;

	/* Wait for the other thread to have written at least once, the
	 * current buffer index is 1 */
	while(*(myArgs->prod0_has_written_once) == 0);

	ret = shd_write_new_blob(ctx,
				&s_blob,
				sizeof(s_blob),
				&sample_meta);
	*(myArgs->prod1_has_written_once) = 1;

	return (void *)ret;
}

static void test_concurrency_write_in_same_section(void)
{
	pthread_t prod_thread[2];
	void *prod_ret[2];
	struct thread_2_producers_args t_args[2];
	uint32_t prod0_created_context = 0,
			prod1_created_context = 0,
			prod0_has_written_once = 0,
			prod1_has_written_once = 0;
	int i = 0;

	for (i = 0; i < 2; i++) {
		t_args[i].prod0_created_context = &prod0_created_context;
		t_args[i].prod0_has_written_once = &prod0_has_written_once;
		t_args[i].prod1_created_context = &prod1_created_context;
		t_args[i].prod1_has_written_once = &prod1_has_written_once;
	}

	pthread_create(&prod_thread[0], NULL,
				&write_alternatively_0, &t_args[0]);
	pthread_create(&prod_thread[1], NULL,
				&write_alternatively_1, &t_args[1]);

	for (i = 0; i < 2; i++)
		pthread_join(prod_thread[i], &prod_ret[i]);

	/* Sample production should succeed in the 2nd thread, while in the
	 * first thread it should fail with an error code indicating a severe
	 * fault */
	CU_ASSERT_EQUAL((int)prod_ret[1], 0);
	CU_ASSERT_EQUAL((int)prod_ret[0], -EFAULT);
}

/*
 * Test race conditions on libshdata internal sample metadata read : if a writer
 * happens to overwrite the sample that was the result of a reader's search
 * at the precise moment the search ended, "end_read" should return an error.
 */

struct thread_overwrite_during_search_args {
	volatile uint32_t *prod_has_written_whole_section;
};

static void *overwrite_producer_thread(void *args)
{
	struct thread_overwrite_during_search_args *myArgs = args;
	struct shd_ctx *ctx;
	int ret;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	int index;

	/* The producer starts by writing the whole section once */
	ctx = shd_create(BLOB_NAME("concurrency-overwrite-just-after-search"),
				NULL,
				&s_hdr_info,
				&s_metadata_hdr);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);

	ret = 0;
	for (index = 0; index < NUMBER_OF_SAMPLES; index++) {
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");
		ret += shd_write_new_blob(ctx,
						&s_blob,
						sizeof(s_blob),
						&sample_meta);
	}
	CU_ASSERT_EQUAL_FATAL(ret, 0);

	/* Now that it's done, we can setup the concurrency strategy for the
	 * next "write" */
	shd_concurrency_set_hook_strategy(HOOK_SAMPLE_WRITE_START,
						SECOND_THREAD_WAITS_FIRST_THREAD_FIRST_ACTION);
	shd_concurrency_set_hook_strategy(HOOK_SAMPLE_WRITE_BEFORE_COMMIT,
						SECOND_THREAD_SIGNALS_SECOND_ACTION);

	/* ... and let the consumer advance */
	*(myArgs->prod_has_written_whole_section) = 1;

	/* We make the final write while the consumer is stuck just after the
	 * completion of its search : this overwrites the first produced
	 * sample */
	if (time_step(&sample_meta.ts) < 0)
		CU_FAIL_FATAL("Could not get time");

	ret = shd_write_new_blob(ctx, &s_blob, sizeof(s_blob), &sample_meta);

	return (void*) ret;
}

static void *overwrite_consumer_thread(void *args)
{
	struct thread_overwrite_during_search_args *myArgs = args;

	struct shd_ctx *ctx;
	int ret, index;
	struct prod_blob read_blob;
	struct shd_sample_metadata sample_meta = METADATA_INIT;
	struct shd_sample_search search = {
		.date = TIME_INIT,
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_FIRST_AFTER
	};
	struct shd_revision *rev;
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) }
	};

	/* Do nothing until the producer has written the whole section once */
	while(*(myArgs->prod_has_written_whole_section) == 0);

	/* We open the section for consumption */
	ctx = shd_open(BLOB_NAME("concurrency-overwrite-just-after-search"),
				NULL, &rev);
	CU_ASSERT_PTR_NOT_NULL_FATAL(ctx);

	/* We read the sample we found : we were looking for the first ever
	 * produced by the other thread, but since libshdata uses a ring buffer,
	 * and the producer is going to write another sample while we're reading
	 * ourselves, the sample we'll find will on the countrary be the most
	 * recent one */
	for (index = 0; index < NUMBER_OF_SAMPLES + 1; index++)
		if (time_step(&sample_meta.ts) < 0)
			CU_FAIL_FATAL("Could not get time");

	ret = shd_read_from_sample(ctx, 0, &search, NULL, blob_samp);
	CU_ASSERT_TRUE_FATAL(ret > 0);
	CU_ASSERT_TRUE_FATAL(time_is_equal(&blob_samp[0].meta.ts,
					&sample_meta.ts));
	CU_ASSERT_TRUE_FATAL(time_is_equal(&blob_samp[0].meta.exp,
					&sample_meta.exp));

	/* ... so, we read unconsistent data, but "end_read" should detect that
	 * the sample was overwritten, and return an error */
	ret = shd_end_read(ctx, rev);

	return (void*) ret;
}

static void test_concurrency_overwrite_sample_just_after_search(void)
{
	pthread_t prod_thread, cons_thread;
	void *prod_ret, *cons_ret;
	struct thread_overwrite_during_search_args thread_args[2];
	uint32_t prod_has_written_whole_section;
	int i = 0;

	shd_concurrency_clean_hooks();

	/* Consumer thread will hang in between the end of the sample search
	 * and the internal read of the sample number, and during this period
	 * the producer thread will overwrite the sample that was the result
	 * of the search */
	shd_concurrency_set_hook_strategy(HOOK_DATA_FIND_SEARCH_OVER,
						FIRST_THREAD_SIGNALS_FIRST_ACTION);
	shd_concurrency_set_hook_strategy(HOOK_DATA_FIND_BEFORE_SAMPLE_INDEX_READ,
						FIRST_THREAD_WAITS_SECOND_THREAD_SECOND_ACTION);

	prod_has_written_whole_section = 0;
	for (i = 0; i < 2; i++)
		thread_args[i].prod_has_written_whole_section =
				&prod_has_written_whole_section;

	pthread_create(&prod_thread, NULL,
				&overwrite_producer_thread, &thread_args[0]);
	pthread_create(&cons_thread, NULL,
				&overwrite_consumer_thread, &thread_args[1]);

	pthread_join(prod_thread, &prod_ret);
	pthread_join(cons_thread, &cons_ret);

	/* The producer should always complete its write */
	CU_ASSERT_EQUAL(prod_ret, 0);
	/* The consumer should detect that an overwrite has occurred */
	CU_ASSERT_EQUAL(cons_ret, -EAGAIN);

	shd_concurrency_clean_hooks();
}

CU_TestInfo s_concurrency_tests[] = {
	{(char *)"create the same section simultaneously in 2 threads",
			&test_concurrency_simultaneous_creation},
	{(char *)"open section while it's being created from an already existing section",
			&test_concurrency_open_during_creation_of_already_existing_section},
	{(char *)"open section while it's being created for the first time",
			&test_concurrency_open_during_first_creation},
	{(char *)"write the same section simultaneously in 2 threads",
			&test_concurrency_simultaneous_write},
	{(char *)"write the same section alternatively in 2 threads",
			&test_concurrency_write_in_same_section},
	{(char *)"overwrite a sample right after the consumer has finished its sample search",
			&test_concurrency_overwrite_sample_just_after_search},
	CU_TEST_INFO_NULL,
};
