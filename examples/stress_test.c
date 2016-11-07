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
 * @file stress_test.c
 *
 * @brief Configurable stress test for libshdata for dimensioning
 *
 * @details The setup of this test is pretty basic : a producer and a consumer
 * run in separate processes at frequencies multiple of the same base
 * clock @50us.
 *   - Every time it wakes up, the producer tries to write a blob of
 *   configurable size.
 *   - Every time it wakes up, the consumer tries to read a configurable number
 *   of samples, and accounts for the samples it has missed.
 *
 * The heuristic used in order to do that is one that has been seen in many use
 * cases, where the consumer keeps track of the last sample it has seen, and at
 * each iteration tries to get the sample that is just after this one (and a
 * few other ones, depending on the respective consumer/producer periods ; e.g.
 * in the consumer runs at 10ms while the producer runs at 1ms, we'll have to
 * get 10 samples at each run of the consumer)
 *
 * The following parameters are configurable :
 *   - the producer/consumer periods
 *   - the blob size
 *   - the shared memory section size
 *   - the number of samples to retrieve at each consumer loop
 *   - the total number of loops
 *
 * At the moment, both the consumer and the producer run at the maximum
 * real-time priority.
 *
 * Example command line :
 *   libshdata-stress -p 200 -c 1000 -r 10000 -s 100 -d 4 -b 1024
 * ... runs the test with a producer running every 200us until it has produced
 * 10000 samples of 1Ko, a consumer running every 1ms looking for 4 + 1 previous
 * samples, and a section containing 100 samples at max.
 */

#include <stdio.h>
#include <sys/mman.h>		/* For shm and PROT flags */
#include <sys/stat.h>		/* For mode constants */
#include <fcntl.h>		/* For O_* constants */
#include <sys/timerfd.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "example_log.h"
#include "common.h"
#define SHD_ADVANCED_READ_API
#include "libshdata.h"
#include <sched.h>
#include <futils/timetools.h>

#define BLOB_NAME "stress_test"
#define COMMUNICATION_ZONE_NAME "libshdata-stress-test"
#define TIMER_PERIOD_NS 50000

struct ex_metadata_blob_hdr ex_metadata_hdr = {
	.i1 = 0,
	.i2 = 0xDEAD,
	.c1 = "Hello",
};

static void usage()
{
	printf("Stress test code for libshdata\n");
	printf("A producer is set to run at a given period. The consumer runs "
			"alongside and tries to catch all the samples that are "
			"produced.\n");
	printf("Usage :\n");
	printf("\tp : producer period (in us)\n");
	printf("\tc : consumer period (in us)\n");
	printf("\tr : number of producer loops to execute\n");
	printf("\tb : size of the blob (in bytes)\n");
	printf("\ts : size of the section (in number of samples)\n");
	printf("\td : history depth on consumer-side\n");

	exit(0);
}

struct test_setup {
	const char *blob_name;
	uint32_t max_nb_samples;
	uint32_t rate;
	int prod_scaler;
	int cons_scaler;
	int timer_period;
	int max_producer_loops;
	int blob_size;
	int samples_after;
};

struct test_result {
	int missed_loops;
	int total_loops;
	int missed_samples;
	int produced_samples;
	int last_missed;
};

struct cmd_line_args {
	uint32_t prod_period; /* in us */
	uint32_t cons_period; /* in us */
	uint32_t repeats;
	uint32_t blob_size;
	uint32_t section_size;
	uint32_t samples_before;
};

struct communication_zone {
	int consumer_ready;
	int timer_fd;
	int test_over;
	struct pollfd poll;
	struct test_result res_prod;
	struct test_result res_cons;
};
static struct communication_zone *communication_zone_get(void)
{
	struct communication_zone *zone = NULL;

	int fd = shm_open(COMMUNICATION_ZONE_NAME, O_CREAT | O_RDWR, 0666);

	if (fd < 0)
		ULOGI("Could not open shared section : %s", strerror(errno));

	zone = mmap(0, sizeof(struct communication_zone),
			PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);

	if (zone == MAP_FAILED)
		ULOGI("Could not mmap : %s", strerror(errno));

	return zone;
}

static void communication_zone_destroy(struct communication_zone *zone)
{
	if (zone)
		munmap(zone, sizeof(struct communication_zone));
	shm_unlink(COMMUNICATION_ZONE_NAME);
}

static struct communication_zone *communication_zone_create(void)
{
	int ret;
	struct communication_zone *zone = NULL;
	int fd = shm_open(COMMUNICATION_ZONE_NAME,
			  O_CREAT | O_EXCL | O_RDWR, 0666);

	/* If creation fails, we attempt to destroy the shared memory first ;
	 * it may still be there from a previous failed test */
	if (fd < 0 && errno == EEXIST) {
		communication_zone_destroy(zone);
		fd = shm_open(COMMUNICATION_ZONE_NAME,
			      O_CREAT | O_EXCL | O_RDWR, 0666);
	}

	if (fd < 0) {
		ULOGI("Could not open shared section : %s", strerror(errno));
		return NULL;
	}

	ret = ftruncate(fd, sizeof(struct communication_zone));

	if (ret < 0)
		ULOGI("Could not truncate zone : %s", strerror(errno));

	zone = mmap(0, sizeof(struct communication_zone),
			PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);

	if (zone == MAP_FAILED)
		ULOGI("Could not mmap : %s", strerror(errno));

	return zone;
}

static void get_size(int size, char *string)
{
	if (size >= 1024 * 1024)
		sprintf(string, "%0.2fMB", (float) size / 1024.0 / 1024.0);
	else if (size >= 1024)
		sprintf(string, "%0.2fKB", (float) size / 1024.0);
	else
		sprintf(string, "%iB", size);
}

static void parse_command(int argc, char *argv[], struct cmd_line_args *args)
{
	int opt;
	char size[128];
	args->prod_period = 100;
	args->cons_period = 100;
	args->repeats = 100;
	args->blob_size = 1;
	args->section_size = 100;
	args->samples_before = 0;

	while ((opt = getopt(argc, argv, "p:c:r:b:s:d:h")) != -1) {
		switch (opt) {
		case 'p':
			args->prod_period = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'c':
			args->cons_period = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'r':
			args->repeats = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'b':
			args->blob_size = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 's':
			args->section_size = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'd':
			args->samples_before = (uint32_t)strtol(optarg,
								NULL, 0);
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}
	ULOGI("Test configuration :");
	ULOGI("    - producer period : %i us", args->prod_period);
	ULOGI("    - consumer period : %i us", args->cons_period);
	get_size(args->blob_size, size);
	ULOGI("    - blob size : %s", size);
	get_size(args->blob_size * args->section_size, size);
	ULOGI("    - section size : %i samples (at least %s)",
			args->section_size, size);
	ULOGI("    - history depth : %i samples", args->samples_before);
}

static void sig_int_handler(int sig)
{
	struct communication_zone *zone = communication_zone_get();
	zone->test_over = 1;
	communication_zone_destroy(zone);
}

static void producer_loop(struct test_setup *setup)
{
	struct shd_ctx *ctx_prod;
	struct shd_sample_metadata sample_meta = { { 0, 0 }, { 0, 0 } };
	struct communication_zone *zone = NULL;
	struct pollfd pollfd;
	uint8_t *data = calloc(sizeof(uint8_t), setup->blob_size);
	if (!data) {
		ULOGP("Could not allocate blob memory : %s", strerror(errno));
		return;
	}
	struct shd_hdr_user_info hdr_info = {
		.blob_size = sizeof(uint8_t) * setup->blob_size,
		.max_nb_samples = setup->max_nb_samples,
		.rate = setup->rate,
		.blob_metadata_hdr_size = sizeof(ex_metadata_hdr)
	};
	int index = 0;
	int currentLoop = 0;
	int nextLoopIndex = 0;

	zone = communication_zone_get();
	if (!zone)
		ULOGI("Could not get communication zone");

	ctx_prod = shd_create(BLOB_NAME, NULL, &hdr_info, &ex_metadata_hdr);
	if (!ctx_prod) {
		ULOGP("Could not create new memory section");
		goto exit;
	}

	while (zone->consumer_ready == 0)
		usleep(1);

	pollfd.fd = zone->timer_fd;
	pollfd.events = POLLIN;

	while (!zone->test_over) {
		uint64_t timer_value;
		nextLoopIndex = (currentLoop + 1) * setup->prod_scaler;

		/* The loop "wakes up" at every timer tick : however we only
		 * wish to really wake up at some moments */
		int ret = poll(&pollfd, 1, 1000);
		if (ret == 0) {
			ULOGP("poll timeout");
			break;
		} else if (ret < 0) {
			ULOGP("poll error : %s", strerror(errno));
			break;
		}

		ret = read(pollfd.fd, &timer_value, sizeof(timer_value));
		index += timer_value;
		if (index < nextLoopIndex)
			continue;
		else if (index > nextLoopIndex + setup->prod_scaler) {
			ULOGP("Producer didn't have time to write a "
					"blob in time after %i loops!",
					currentLoop);
			zone->res_prod.missed_loops++;
		}

		if (time_timespec_add_us(&sample_meta.ts,
					 setup->prod_scaler
						* setup->timer_period / 1000,
					 &sample_meta.ts) < 0) {
			ULOGP("Could not get time");
			break;
		}

		ret = shd_write_new_blob(ctx_prod, data,
						setup->blob_size, &sample_meta);

		if (ret < 0) {
			ULOGP("Error writing blob : %s", strerror(-ret));
			break;
		}

		if (currentLoop * 10 % setup->max_producer_loops == 0)
			ULOGP("%i %%",
			      currentLoop * 100 / setup->max_producer_loops);

		currentLoop++;

		if (currentLoop >= setup->max_producer_loops)
			break;
	}

exit:
	zone->test_over = 1;
	zone->res_prod.total_loops = currentLoop;
	zone->res_prod.produced_samples = currentLoop;
	communication_zone_destroy(zone);
	shd_close(ctx_prod, NULL);
	free(data);
}

static void consumer_one_sample_loop(struct test_setup *setup,
					struct communication_zone *zone,
					struct shd_ctx **ctx_cons,
					struct shd_revision **rev,
					struct pollfd *pollfd)
{
	int index = 0;
	int myPeriod = setup->cons_scaler * setup->timer_period / 1000;
	int currentLoop = 0;
	int nextLoopIndex = 0;
	struct shd_sample_search search = {
		.date = { 0, 0 },
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	uint8_t *read_data = calloc(sizeof(uint8_t), setup->blob_size);
	if (!read_data) {
		ULOGC("Could not allocate blob memory : %s", strerror(errno));
		return;
	}
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = read_data, .size = sizeof(uint8_t) * setup->blob_size }
	};

	while (!zone->test_over) {
		uint64_t timer_value;
		nextLoopIndex = (currentLoop + 1) * setup->cons_scaler;

		int ret = poll(pollfd, 1, 1000);
		if (ret == 0) {
			ULOGC("poll timeout");
			break;
		} else if (ret < 0) {
			ULOGC("poll error : %s", strerror(errno));
			break;
		}

		ret = read(pollfd->fd, &timer_value, sizeof(timer_value));
		index = timer_value;
		if (index < nextLoopIndex)
			continue;
		else if (index > nextLoopIndex + setup->prod_scaler) {
			ULOGC("Consumer didn't execute in time after %i loops!",
					currentLoop);
			zone->res_cons.missed_loops++;
		}

		do {
			ret = shd_read_from_sample(*ctx_cons, 0, &search, NULL,
					blob_samp);
			if (ret < 0)
				usleep(myPeriod/10);
		} while (ret == -EAGAIN);

		if (ret <= 0)
			ULOGC("Error encountered while reading from "
					"sample : %s", strerror(-ret));

		ret = shd_end_read(*ctx_cons, *rev);
		if (ret < 0 && ret == -ENODEV) {
			ULOGC("Reopening memory section ...");
			do {
				shd_close(*ctx_cons, *rev);
				*ctx_cons = shd_open(BLOB_NAME, NULL, rev);
			} while (*ctx_cons == NULL);
		} else if (ret < 0) {
			ULOGC("Error encountered while ending read : %s",
					strerror(-ret));
			zone->test_over = 1;
		}
		currentLoop++;
	}
	zone->res_cons.total_loops = currentLoop;
	free(read_data);
}

static int try_read(struct shd_ctx **ctx_cons,
		    struct communication_zone *zone,
		    struct test_setup *setup,
		    struct shd_sample_metadata *metadata,
		    struct shd_revision **rev,
		    uint8_t *read_data,
		    int current_loop,
		    struct timespec *most_recent,
		    struct shd_search_result *result)
{
	int ret;
	struct timespec diff = { 0, 0 };
	uint64_t diff_us = 0;
	uint64_t prod_period_us = setup->prod_scaler
					* setup->timer_period / 1000;

	ret = shd_read_quantity(*ctx_cons, NULL, read_data,
				setup->blob_size * (setup->samples_after + 1));
	if (ret < 0)
		ULOGC("Problem reading quantity : %s", strerror(-ret));

	ret = shd_end_read(*ctx_cons, *rev);
	if (ret < 0 && ret == -ENODEV) {
		ULOGC("Reopening memory section ...");
		do {
			shd_close(*ctx_cons, *rev);
			*ctx_cons = shd_open(BLOB_NAME, NULL, rev);
		} while (*ctx_cons == NULL);
	} else if (ret >= 0) {
		ret = time_timespec_cmp(most_recent, &metadata[0].ts);
		if (ret > 0) {
			ULOGC("Retrieved sample is outdated");
		} else {
			ret = time_timespec_diff(most_recent, &metadata[0].ts,
						 &diff);
			if (ret < 0)
				ULOGC("Error computing diff : %s",
						strerror(-ret));

			ret = time_timespec_to_us(&diff, &diff_us);
			if (ret < 0)
				ULOGC("Error converting diff : %s",
						strerror(-ret));

			if (diff_us > prod_period_us) {
				ULOGC("Missed %llu samples",
						diff_us / prod_period_us);
				zone->res_cons.missed_samples +=
					diff_us / prod_period_us;
				zone->res_cons.last_missed = current_loop;
			}

			*most_recent = metadata[result->nb_matches - 1].ts;
		}
	}

	return 0;
}

static void consumer_several_sample_loop(struct test_setup *setup,
					struct communication_zone *zone,
					struct shd_ctx **ctx_cons,
					struct shd_revision **rev,
					struct pollfd *pollfd)
{
	int index = 0;
	int myPeriod = setup->cons_scaler * setup->timer_period / 1000;
	int currentLoop = 0;
	int nextLoopIndex = 0;
	struct shd_sample_search search = {
		.date = { 0, 0 },
		.nb_values_after_date = setup->samples_after,
		.nb_values_before_date = 0,
		.method = SHD_FIRST_AFTER
	};
	uint8_t *read_data = calloc(sizeof(uint8_t),
				    setup->blob_size * (setup->samples_after
							+ 1));
	if (!read_data) {
		ULOGC("Could not allocate reading buffer zone : %s",
		      strerror(errno));
		return;
	}
	struct shd_sample_metadata *metadata = NULL;
	struct shd_search_result result;
	struct timespec most_recent = { 0, 0 };

	while (!zone->test_over) {
		uint64_t timer_value;
		nextLoopIndex = (currentLoop + 1) * setup->cons_scaler;

		int ret = poll(pollfd, 1, 1000);
		if (ret == 0) {
			ULOGC("poll timeout");
			break;
		} else if (ret < 0) {
			ULOGC("poll error : %s", strerror(errno));
			break;
		}

		ret = read(pollfd->fd, &timer_value, sizeof(timer_value));
		index += timer_value;
		if (index < nextLoopIndex)
			continue;
		else if (index > nextLoopIndex + setup->cons_scaler) {
			ULOGC("Consumer didn't execute in time after %i loops!",
					currentLoop);
			zone->res_cons.missed_loops++;
		}

		currentLoop++;
		time_timespec_add_us(&most_recent, 1, &search.date);

		do {
			ret = shd_select_samples(*ctx_cons, &search, &metadata,
						 &result);
			if (ret < 0)
				usleep(myPeriod/10);
		} while (ret == -EAGAIN);

		if (ret < 0) {
			if (ret != -ENOENT)
				ULOGC("Error encountered while reading from "
						"sample : %s", strerror(-ret));
		} else {
			try_read(ctx_cons, zone, setup,
				 metadata, rev, read_data,
				 currentLoop, &most_recent, &result);
		}
	}
	zone->res_cons.total_loops = currentLoop;
	free(read_data);
}

static void consumer_loop(struct test_setup *setup)
{
	struct shd_ctx *ctx_cons = NULL;
	struct shd_revision *rev;
	struct communication_zone *zone = NULL;
	struct pollfd pollfd;

	zone = communication_zone_get();
	if (!zone)
		ULOGI("Could not get communication zone");

	while (ctx_cons == NULL)
		ctx_cons = shd_open(BLOB_NAME, NULL, &rev);

	pollfd.fd = zone->timer_fd;
	pollfd.events = POLLIN;

	zone->consumer_ready = 1;

	if (setup->samples_after == 0)
		consumer_one_sample_loop(setup, zone, &ctx_cons, &rev, &pollfd);
	else
		consumer_several_sample_loop(setup, zone, &ctx_cons, &rev,
					     &pollfd);

	shd_close(ctx_cons, rev);
	communication_zone_destroy(zone);
}

static void launch_test(int timer_fd, struct test_setup *setup)
{
	struct itimerspec period = {
		.it_interval = { .tv_sec = 0, .tv_nsec = setup->timer_period },
		.it_value = { .tv_sec = 0, .tv_nsec = 100000000 }
	};
	int ret = timerfd_settime(timer_fd, 0, &period, NULL);
	if (ret < 0)
		ULOGI("Error setting the timer : %s", strerror(errno));
	struct sched_param sched_params;
	sched_params.__sched_priority = sched_get_priority_max(SCHED_RR);

	pid_t pid = fork();

	if (pid == 0) {
		/* Child code */
		ULOGC("Created with PID = %d", getpid());
		if (sched_setscheduler(0, SCHED_RR, &sched_params) < 0)
			ULOGC("Could not apply scheduling policy");

		signal(SIGINT, sig_int_handler);
		consumer_loop(setup);
		ULOGC("... ended");
		exit(0);
	} else if (pid > 0) {
		/* Parent code */
		ULOGP("Created with PID = %d", getpid());
		if (sched_setscheduler(0, SCHED_RR, &sched_params) < 0)
			ULOGP("Could not apply scheduling policy");

		signal(SIGINT, sig_int_handler);
		producer_loop(setup);
		ULOGP("... ended, waiting for consumer to exit");
		wait(NULL);
	} else {
		ULOGI("Oops");
	}
}

int main(int argc, char *argv[])
{
	struct cmd_line_args args;
	parse_command(argc, argv, &args);
	struct communication_zone *zone;

	zone = communication_zone_create();
	if (!zone) {
		ULOGI("Error creating the communication zone : %s",
		      strerror(errno));
		return -1;
	}

	zone->timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (zone->timer_fd < 0) {
		ULOGI("Error creating the timer : %s", strerror(errno));
		return -1;
	}

	zone->consumer_ready = 0;
	zone->test_over = 0;
	zone->res_cons.missed_loops = 0;
	zone->res_cons.total_loops = 0;
	zone->res_cons.missed_samples = 0;
	zone->res_prod.missed_loops = 0;
	zone->res_prod.total_loops = 0;
	zone->res_prod.missed_samples = 0;

	if (args.cons_period * 1000 % TIMER_PERIOD_NS != 0) {
		ULOGI("Consumer period should be an integer multiple of %i ns",
				TIMER_PERIOD_NS);
	} else if (args.prod_period * 1000 % TIMER_PERIOD_NS != 0) {
		ULOGI("Producer period should be an integer multiple of %i ns",
				TIMER_PERIOD_NS);
	}

	struct test_setup setup = {
		.blob_name = BLOB_NAME,
		.max_nb_samples = args.section_size,
		.rate = 1000,
		.cons_scaler = args.cons_period * 1000 / TIMER_PERIOD_NS,
		.prod_scaler = args.prod_period * 1000 / TIMER_PERIOD_NS,
		.timer_period = TIMER_PERIOD_NS,
		.max_producer_loops = args.repeats,
		.blob_size = args.blob_size,
		.samples_after = args.samples_before
	};

	launch_test(zone->timer_fd, &setup);

	ULOGI("Producer missed %i loops out of %i", zone->res_prod.missed_loops,
						zone->res_prod.total_loops);
	ULOGI("Consumer missed %i samples (for the last time at iteration #%i "
			"out of %i loops)",
			zone->res_cons.missed_samples,
			zone->res_cons.last_missed,
			zone->res_cons.total_loops);

	communication_zone_destroy(zone);
	return 0;
}
