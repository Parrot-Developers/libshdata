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
 * @file 1prod_1cons.c
 *
 * @brief Basic libshdata example with 1 producer and 1 consumer
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "example_log.h"
#include "common.h"
#include "libshdata.h"

#define BLOB_NAME "example_1prod_1cons"
#define NUMBER_OF_SAMPLES 20

struct example_prod_blob ex_blob = {
	.i1	= 0,
	.c1	= 'a',
	.li1	= 0,
	.ui1	= 0,
	.f1	= 0.0,
	.acc	= { 0.0, 0.0, 0.0 },
	.angles	= {
		0.0,
		0.0,
		0.0
	},
	.state	= 0
};

struct ex_metadata_blob_hdr ex_metadata_hdr = {
	.i1 = 0xCAFE,
	.i2 = 0xDEAD,
	.c1 = "Hello",
};

static struct ex_conf s_conf = {
	.prod_period = 1000,
	.cons_period = 1000,
	.repeats = 50,
	.total_time = 10
};

struct shd_hdr_user_info ex_hdr_info = {
	.blob_size = sizeof(ex_blob),
	.max_nb_samples = NUMBER_OF_SAMPLES,
	.rate = 1000,
	.blob_metadata_hdr_size = sizeof(ex_metadata_hdr)
};

static bool alarm_set;
static void alarmHandler(int dummy)
{
	alarm_set = true;
}

static void usage()
{
	printf("Configurable example code for libshdata\n");
	printf("Usage :\n");
	printf("\tp : producer period\n");
	printf("\tc : consumer period\n");
	printf("\tr : number of loops to execute\n");
	printf("\tt : test duration\n");

	exit(0);
}

static void parse_command(int argc, char *argv[], struct ex_conf *s_conf)
{
	int opt;

	while ((opt = getopt(argc, argv, "p:c:r:t:h")) != -1) {
		switch (opt) {
		case 'p':
			s_conf->prod_period = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'c':
			s_conf->cons_period = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'r':
			s_conf->repeats = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 't':
			s_conf->total_time = (uint32_t)strtol(optarg, NULL, 0);
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}
}

static void producer_loop(void)
{
	struct shd_ctx *ctx_prod;
	struct shd_sample_metadata sample_meta;
	int ret;
	unsigned int index;
	unsigned int repeats = (s_conf.total_time * 1000)
					/ s_conf.prod_period + 1;

	ctx_prod = shd_create(BLOB_NAME, NULL, &ex_hdr_info, &ex_metadata_hdr);
	if (ctx_prod != NULL)
		ULOGP("Successfully created new memory section");

	ualarm(s_conf.prod_period, s_conf.prod_period);
	for (index = 0; index < repeats; index++) {
		if (clock_gettime(CLOCK_MONOTONIC, &sample_meta.ts) < 0) {
			ULOGP("Could not get time");
			return;
		}

		update_blob(&ex_blob);
		ret = shd_write_new_blob(ctx_prod,
					&ex_blob,
					sizeof(ex_blob),
					&sample_meta);

		ULOGP("Wrote sample at date : %ld_%ld", sample_meta.ts.tv_sec,
							sample_meta.ts.tv_nsec);

		while (!alarm_set)
			usleep(s_conf.prod_period/10);
		alarm_set = false;
	}
	shd_close(ctx_prod, NULL);
}

static void consumer_loop(void)
{
	struct shd_ctx *ctx_cons = NULL;
	struct shd_revision *rev;
	struct example_prod_blob read_blob;
	struct shd_sample_search search = {
		.nb_values_after_date = 0,
		.nb_values_before_date = 0,
		.method = SHD_LATEST
	};
	struct shd_quantity_sample blob_samp[1] = {
		{ .ptr = &read_blob, .size = sizeof(read_blob) }
	};
	int ret = -EAGAIN;
	unsigned int index;
	unsigned int repeats = (s_conf.total_time * 1000)
					/ s_conf.cons_period + 1;

	while (ctx_cons == NULL)
		ctx_cons = shd_open(BLOB_NAME, NULL, &rev);

	ULOGC("Successfully open memory section");

	ualarm(s_conf.cons_period, s_conf.cons_period);
	for (index = 0; index < repeats; index++) {
		do {
			ret = shd_read_from_sample(ctx_cons, 0, &search, NULL,
					blob_samp);
			if (ret < 0)
				usleep(s_conf.cons_period/10);
		} while (ret == -EAGAIN);

		if (ret <= 0)
			ULOGC("Error encountered while reading from "
					"sample : %s", strerror(-ret));

		ret = shd_end_read(ctx_cons, rev);
		if (ret < 0)
			ULOGC("Error encountered while ending read : %s",
					strerror(-ret));
		if (ret == -ENODEV) {
			ULOGC("Reopening memory section ...");
			do {
				shd_close(ctx_cons, rev);
				ctx_cons = shd_open(BLOB_NAME, NULL, &rev);
			} while (ctx_cons == NULL);
		} else if (ret == 0) {
			ULOGC("Read sample at date  : %ld_%ld",
					blob_samp[0].meta.ts.tv_sec,
					blob_samp[0].meta.ts.tv_nsec);
		}

		while (!alarm_set)
			usleep(s_conf.cons_period/10);
		alarm_set = false;
	}
	shd_close(ctx_cons, rev);
}

int main(int argc, char *argv[])
{
	parse_command(argc, argv, &s_conf);
	alarm_set = false;
	pid_t pid = fork();

	if (pid == 0) {
		/* Child code */
		ULOGP("PID = %d", getpid());
		signal(SIGALRM, alarmHandler);
		consumer_loop();
		exit(0);
	} else if (pid > 0) {
		/* Parent code */
		ULOGC("PID = %d", getpid());
		signal(SIGALRM, alarmHandler);
		producer_loop();
		ULOG("Waiting for child to terminate");
		wait(NULL);
	} else {
		ULOG("Oops");
	}

	return 0;
}
