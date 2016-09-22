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
 * @file shd_test_helper.h
 *
 * @brief
 *
 */

#ifndef SHD_TEST_HELPER_H_
#define SHD_TEST_HELPER_H_

#include <stddef.h>
#include <math.h>

#define NUMBER_OF_SAMPLES 20
#define CLOSEST_SEARCH_INDEX 7
#define TEST_BLOB_NAME "myBlob"
#define BLOB_NAME(a) "myBlob_" a
/* Ugly but useful macro to create a quantity from any field in a blob
 * The name of that new structure will be q_blobname_fieldname
 */
#define QUANTITY_DEF(t, b, q) struct shd_quantity q_##b##_##q = \
	QUANTITY_FIELD(t, b, q)

#define QUANTITY_DECL(b, q) extern struct shd_quantity q_##b##_##q;
#define QUANTITY_FIELD(t, b, q) { offsetof(struct t, q), sizeof(b.q) }
#define QTY_SAMPLE_FIELD(b, q) { .ptr = &b.q, .size = sizeof(b.q) }

#define QUANTITY_PTR(b, q) ((void *) &b.q)

#define QUANTITIES_NB_IN_BLOB 8

#define TEST_VAL_i1		(0xAB)
#define TEST_VAL_c1		('h')
#define TEST_VAL_li1		(-0xDEADCAFE)
#define TEST_VAL_ui1		(0xCAFE)
#define TEST_VAL_f1		(1.2345)
#define TEST_VAL_acc_x		(9.81)
#define TEST_VAL_acc_y		(-9.81)
#define TEST_VAL_acc_z		(9.81)
#define TEST_VAL_angles_rho	(180.0)
#define TEST_VAL_angles_phi	(90.0)
#define TEST_VAL_angles_theta	(-270.0)
#define TEST_VAL_state		(STATE_LANDING)
#define FLOAT_PRECISION		(0.0000001)
#define DOUBLE_PRECISION	(0.000000001)

#define TEST_VAL_MDATA_i1	(0xCAFE)
#define TEST_VAL_MDATA_c1	"Hello"
#define TEST_VAL_MDATA_i2	0xDEAD
#define TEST_VAL_MDATA_c2	"Lorem ipsum dolor sit amet, consectetur "\
					"adipiscing elit. Sed non risus. "
#define TEST_VAL_MDATA_li1	0x4567


/* Time simulation functions
 * ... this code should surely be unit tested as well */
#define TIME_INIT ((struct timespec) { .tv_nsec = 0, .tv_sec = 0 })
#define METADATA_INIT ((struct shd_sample_metadata) \
				{ .ts = TIME_INIT, .exp = TIME_INIT })

/*
 * Type declarations for the definition of our test blob
 */
struct acceleration {
	double x;
	double y;
	double z;
};

struct angles {
	double rho;
	double phi;
	double theta;
};

enum flight_state_t {
	STATE_FLYING,
	STATE_HOVERING,
	STATE_LANDING,
	STATE_TAKEOFF
};

struct prod_blob {
	int i1;
	char c1;
	long int li1;
	unsigned int ui1;
	float f1;
	struct acceleration acc;
	struct angles angles;
	enum flight_state_t state;
};

struct prod_blob_alt_size {
	int i1;
	char c1;
	long int li1;
	struct angles angles;
	enum flight_state_t state;
};

struct blob_metadata_hdr {
	int i1;
	const char c1[10];
	int i2;
	const char c2[255];
	long int li1;
};

extern struct prod_blob s_blob;
extern struct prod_blob_alt_size s_blob_alt_size;
extern struct blob_metadata_hdr s_metadata_hdr;
extern struct shd_hdr_user_info s_hdr_info;

QUANTITY_DECL(s_blob, i1);
QUANTITY_DECL(s_blob, c1);
QUANTITY_DECL(s_blob, li1);
QUANTITY_DECL(s_blob, ui1);
QUANTITY_DECL(s_blob, f1);
QUANTITY_DECL(s_blob, acc);
QUANTITY_DECL(s_blob, angles);
QUANTITY_DECL(s_blob, state);


struct timespec time_in_past(struct timespec current, int back_steps);
struct timespec time_in_past_before(struct timespec current, int back_steps);
struct timespec time_in_past_after(struct timespec current, int back_steps);
int time_set(struct timespec *dest, struct timespec value);


bool time_is_equal(struct timespec *t1, struct timespec *t2);
bool time_is_after(struct timespec *t1, struct timespec *t2);
bool time_is_before(struct timespec *t1, struct timespec *t2);
int time_step(struct timespec *t);

bool compare_blobs(struct prod_blob b1, struct prod_blob b2);

int get_unique_blob_name(const char *root, char *dest);

#endif /* SHD_TEST_HELPER_H_ */
