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
 * @file hooks_implem.c
 *
 * @brief Implem file for libshdata concurrency hook points
 */

#include "hooks_implem.h"
#include <unistd.h>
#include <stdint.h>

#define WAIT_ON_CONDITION(cond) 	\
	do {				\
		while(cond) { 		\
			usleep(10000);	\
		}			\
	} while(0)

/* Global structure used to share information between libshdata inner code and
 * test code */
struct {
	useconds_t applicable_delay;
	enum concurrency_strategy stategy[HOOK_TOTAL];
	int counter;
	uint32_t flags[2][2];
	uint32_t parallel_counter;
} concurrency_env;

void shd_concurrency_clean_hooks(void)
{
	int i, j;
	concurrency_env.counter = 0;
	concurrency_env.applicable_delay = 10000;
	for (i = 0; i < HOOK_TOTAL; i++)
		concurrency_env.stategy[i] = DO_NOTHING;
	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++)
			concurrency_env.flags[i][j] = 0;
	concurrency_env.parallel_counter = 0;
}

void shd_concurrency_set_hook_strategy(enum shd_concurrency_hook hook,
					enum concurrency_strategy strat)
{
	concurrency_env.stategy[hook] = strat;
}

void shd_concurrency_set_applicable_delay(useconds_t delay)
{
	concurrency_env.applicable_delay = delay;
}

void shd_concurrency_emulate_thread_completion(int thread_id)
{
	int i;
	for (i = 0; i < 2; i++)
		concurrency_env.flags[thread_id][i] = 1;
}

int shd_concurrency_has_thread_completed(int thread_id)
{
	return concurrency_env.flags[thread_id][1] == 1;
}

void shd_concurrency_emulate_action_complete()
{
	__sync_fetch_and_add(&concurrency_env.parallel_counter, 1);
}

void shd_concurrency_hook(enum shd_concurrency_hook hook)
{
	switch(concurrency_env.stategy[hook])
	{
	case DO_NOTHING:
		return;
	case DELAY_FOR_ALL_THREADS:
		usleep(concurrency_env.applicable_delay);
		break;
	case DELAY_FOR_FIRST_THREAD:
		if (__sync_fetch_and_add(&concurrency_env.counter, 1) == 0)
			usleep(concurrency_env.applicable_delay);
		break;

	case FIRST_THREAD_SIGNALS_FIRST_ACTION:
		__sync_fetch_and_add(&concurrency_env.flags[0][0], 1);
		break;

	case SECOND_THREAD_WAITS_FIRST_THREAD_FIRST_ACTION:
		WAIT_ON_CONDITION(concurrency_env.flags[0][0] == 0);
		concurrency_env.flags[1][0] = 1;
		break;

	case SECOND_THREAD_SIGNALS_SECOND_ACTION:
		if (concurrency_env.flags[1][0] == 1)
			__sync_fetch_and_add(&concurrency_env.flags[1][1], 1);
		break;

	case FIRST_THREAD_WAITS_SECOND_THREAD_SECOND_ACTION:
		WAIT_ON_CONDITION(concurrency_env.flags[1][1] == 0);
		concurrency_env.flags[0][1] = 1;
		break;

	case PARALLEL_BOTH_THREADS_SIGNAL_ACTION:
		__sync_fetch_and_add(&concurrency_env.parallel_counter, 1);
		break;

	case PARALLEL_BOTH_THREADS_WAIT_ALL_ACTIONS_COMPLETE:
		WAIT_ON_CONDITION(concurrency_env.parallel_counter < 2);
		break;

	default:
		break;
	}
}
