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
 * @file common.h
 *
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>

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

struct example_prod_blob {
	int i1;
	char c1;
	long int li1;
	unsigned int ui1;
	float f1;
	struct acceleration acc;
	struct angles angles;
	enum flight_state_t state;
};

struct ex_metadata_blob_hdr {
	int i1;
	int i2;
	const char c1[10];
};

struct ex_conf {
	uint32_t prod_period; /* in us */
	uint32_t cons_period; /* in us */
	uint32_t repeats;
	uint32_t total_time; /* in ms */
};

void update_blob(struct example_prod_blob *s_blob);

#endif /* COMMON_H_ */
