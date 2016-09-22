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
 * @file shd_test_helper.c
 *
 * @brief
 *
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>
#include "libshdata.h"
#include "shd_test_helper.h"

struct prod_blob s_blob = {
	.i1	= TEST_VAL_i1,
	.c1	= TEST_VAL_c1,
	.li1	= TEST_VAL_li1,
	.ui1	= TEST_VAL_ui1,
	.f1	= TEST_VAL_f1,
	.acc	= { TEST_VAL_acc_x, TEST_VAL_acc_y, TEST_VAL_acc_z },
	.angles	= {
		TEST_VAL_angles_rho,
		TEST_VAL_angles_phi,
		TEST_VAL_angles_theta
	},
	.state	= TEST_VAL_state
};

struct prod_blob_alt_size s_blob_alt_size = {
	.i1	= TEST_VAL_i1,
	.c1	= TEST_VAL_c1,
	.li1	= TEST_VAL_li1,
	.angles	= {
		TEST_VAL_angles_rho,
		TEST_VAL_angles_phi,
		TEST_VAL_angles_theta
	},
	.state	= TEST_VAL_state
};

/*
 * Declaration of all the quantity structures
 */
QUANTITY_DEF(prod_blob, s_blob, i1);
QUANTITY_DEF(prod_blob, s_blob, c1);
QUANTITY_DEF(prod_blob, s_blob, li1);
QUANTITY_DEF(prod_blob, s_blob, ui1);
QUANTITY_DEF(prod_blob, s_blob, f1);
QUANTITY_DEF(prod_blob, s_blob, acc);
QUANTITY_DEF(prod_blob, s_blob, angles);
QUANTITY_DEF(prod_blob, s_blob, state);

struct blob_metadata_hdr s_metadata_hdr = {
	.i1 = TEST_VAL_MDATA_i1,
	.c1 = TEST_VAL_MDATA_c1,
	.i2 = TEST_VAL_MDATA_i2,
	.c2 = TEST_VAL_MDATA_c2,
	.li1 = TEST_VAL_MDATA_li1
};

struct shd_hdr_user_info s_hdr_info = {
	.blob_size = sizeof(s_blob),
	.max_nb_samples = NUMBER_OF_SAMPLES,
	.rate = 1000,
	.blob_metadata_hdr_size = sizeof(s_metadata_hdr)
};

static bool compare_d(double d1, double d2)
{
	return fabs((double)(d1) - (d2)) <= fabs((double)(DOUBLE_PRECISION));
}
static bool compare_f(float f1, float f2)
{
	return fabs((double)(f1) - (f2)) <= fabs((double)(FLOAT_PRECISION));
}

bool compare_blobs(struct prod_blob b1, struct prod_blob b2)
{
	return b1.i1 == b2.i1
		&& b1.c1 == b2.c1
		&& b1.li1 == b2.li1
		&& b1.ui1 == b2.ui1
		&& compare_f(b1.f1, b2.f1)
		&& compare_d(b1.acc.x, b2.acc.x)
		&& compare_d(b1.acc.y, b2.acc.y)
		&& compare_d(b1.acc.z, b2.acc.z)
		&& compare_d(b1.angles.rho, b2.angles.rho)
		&& compare_d(b1.angles.phi, b2.angles.phi)
		&& compare_d(b1.angles.theta, b2.angles.theta)
		&& b1.state == b2.state;
}

#define BILLION 1000000000
static struct timespec time_add(struct timespec t1, struct timespec t2)
{
	long sec = t2.tv_sec + t1.tv_sec;
	long nsec = t2.tv_nsec + t1.tv_nsec;
	if (nsec >= BILLION) {
		nsec -= BILLION;
		sec++;
	}
	return (struct timespec){ .tv_sec = sec, .tv_nsec = nsec };
}

static struct timespec time_remove(struct timespec t1, struct timespec t2)
{
	long sec = t1.tv_sec - t2.tv_sec;
	long nsec = t1.tv_nsec - t2.tv_nsec;
	if (nsec < 0) {
		nsec += BILLION;
		sec--;
	}
	return (struct timespec){ .tv_sec = sec, .tv_nsec = nsec };
}

static int time_cmp(struct timespec a, struct timespec b)
{
	if (a.tv_sec != b.tv_sec) {
		if (a.tv_sec > b.tv_sec)
			return 1;
		else
			return -1;
	} else {
		if (a.tv_nsec > b.tv_nsec)
			return 1;
		else if (a.tv_nsec < b.tv_nsec)
			return -1;
		else
			return 0;
	}
}

bool time_is_equal(struct timespec *t1, struct timespec *t2)
{
	return (time_cmp(*t1, *t2) == 0);
}
/* Return true is t1 is after t2 */
bool time_is_after(struct timespec *t1, struct timespec *t2)
{
	return (time_cmp(*t1, *t2) > 0);
}
/* Return true is t1 is before t2 */
bool time_is_before(struct timespec *t1, struct timespec *t2)
{
	return (time_cmp(*t1, *t2) < 0);
}

#define STEP_NANO 1000000
static const struct timespec step = {
	.tv_sec = 0,
	.tv_nsec = STEP_NANO
};
static const struct timespec small_step = {
	.tv_sec = 0,
	.tv_nsec = STEP_NANO / 10
};

int time_step(struct timespec *t)
{
	*t = time_add(*t, step);
	return 0;
}
static int time_small_step(struct timespec *t)
{
	*t = time_add(*t, small_step);
	return 0;
}

static int time_back_step(struct timespec *t)
{
	*t = time_remove(*t, step);
	return 0;
}
static int time_back_small_step(struct timespec *t)
{
	*t = time_remove(*t, small_step);
	return 0;
}

struct timespec time_in_past(struct timespec current, int back_steps)
{
	int i;
	struct timespec ret;

	ret = (struct timespec){ .tv_sec = current.tv_sec,
					.tv_nsec = current.tv_nsec};
	for (i = 0; i < back_steps; i++)
		time_back_step(&ret);

	return ret;
}

struct timespec time_in_past_before(struct timespec current, int back_steps)
{
	struct timespec ret = time_in_past(current, back_steps);
	time_back_small_step(&ret);
	return ret;
}

struct timespec time_in_past_after(struct timespec current, int back_steps)
{
	struct timespec ret = time_in_past(current, back_steps);
	time_small_step(&ret);
	return ret;
}

int time_set(struct timespec *dest, struct timespec value)
{
	dest->tv_nsec = value.tv_nsec;
	dest->tv_sec = value.tv_sec;
	return 0;
}

int get_unique_blob_name(const char *root, char *dest)
{
	struct timespec current_time;
	if (clock_gettime(CLOCK_REALTIME, &current_time) < 0)
		return -1;

	return snprintf(dest, NAME_MAX, "%s-%ld-%ld", root,
			current_time.tv_sec,
			current_time.tv_nsec);
}
