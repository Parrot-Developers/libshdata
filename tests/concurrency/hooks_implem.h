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
 * @file hooks_implem.h
 *
 * @brief Interface for implementation of the hooks added into libshdata's main
 * code that are used for concurrency tests
 *
 * @details
 * This module implements various strategy for concurrency tests. Delays are
 * rather easy to understand, but we also provide a way to define more
 * complex behaviors, such as two threads waiting for each other while they are
 * following the same execution path, or while they follow different execution
 * paths
 */

#ifndef HOOKS_IMPLEM_H_
#define HOOKS_IMPLEM_H_

#include <sys/types.h>
#include <unistd.h>
#include "libshdata-concurrency-hooks.h"

enum concurrency_strategy {
	DO_NOTHING,
	DELAY_FOR_ALL_THREADS,
	DELAY_FOR_FIRST_THREAD,

	/* Strategies to use when the two threads follow a different
	 * execution path */
	FIRST_THREAD_SIGNALS_FIRST_ACTION,
	SECOND_THREAD_WAITS_FIRST_THREAD_FIRST_ACTION,
	SECOND_THREAD_SIGNALS_SECOND_ACTION,
	FIRST_THREAD_WAITS_SECOND_THREAD_SECOND_ACTION,

	/* Strategies to use when the two threads follow the same execution
	 * path */
	PARALLEL_BOTH_THREADS_SIGNAL_ACTION,
	PARALLEL_BOTH_THREADS_WAIT_ALL_ACTIONS_COMPLETE
};

/*
 * @brief Init all hooks to do nothing and reset internal state
 */
void shd_concurrency_clean_hooks(void);

/*
 * @brief Set the strategy to follow for a given hook point
 */
void shd_concurrency_set_hook_strategy(enum shd_concurrency_hook hook,
					enum concurrency_strategy strat);

/*
 * @brief Set the delay that should be applied when such a strategy is chosen
 */
void shd_concurrency_set_applicable_delay(useconds_t delay);

/*
 * @brief Emulate thread completion
 *
 * @details Should be called when the execution path of a given thread is
 * expected to break from normal (e.g. earlier return from a function call due
 * to a correctly-detected error), potentially preventing it from hitting its
 * second hook point (and thus blocking the other thread)
 */
void shd_concurrency_emulate_thread_completion(int thread_id);

/*
 * @brief Return whether thread has completed or not
 *
 * @details this function is useful if we need to complete an action in test
 * code that is supposed to occur only once a given thread as finished its
 * task
 */
int shd_concurrency_has_thread_completed(int thread_id);

/*
 * @brief Emulate end of action
 *
 * @details Should be called when the execution path of a given thread is
 * expected to break from normal (e.g. earlier return from a function call due
 * to a correctly-detected error), potentially preventing it from completing
 * its expected action (and thus blocking the other thread)
 */
void shd_concurrency_emulate_action_complete(void);

/*
 * @brief Entry point for libshdata's main code
 */
void shd_concurrency_hook(enum shd_concurrency_hook hook);

#endif /* HOOKS_IMPLEM_H_ */
