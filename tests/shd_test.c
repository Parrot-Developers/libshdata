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
 * @file shd_test.c
 *
 * @brief shared memory test.
 *
 */

#include "libshdata.h"
#include "shd_test.h"
#include "stdlib.h"

extern CU_TestInfo s_api_tests[];
extern CU_TestInfo s_api_adv_tests[];
extern CU_TestInfo s_func_basic_write_tests[];
extern CU_TestInfo s_func_adv_read_tests[];
extern CU_TestInfo s_func_basic_read_tests[];
extern CU_TestInfo s_func_read_hdr_tests[];
extern CU_TestInfo s_func_adv_write_tests[];
extern CU_TestInfo s_error_tests[];
extern CU_TestInfo s_concurrency_tests[];

static int use_binary_search(void)
{
	return setenv("LIBSHDATA_CONFIG_INTERNAL_SEARCH_METHOD", "BINARY", 1);
}

static int reset_search_method(void)
{
	return unsetenv("LIBSHDATA_CONFIG_INTERNAL_SEARCH_METHOD");
}

static CU_SuiteInfo s_suites[] = {
	{(char *)"API", NULL, NULL, s_api_tests},
	{(char *)"Advanced API", NULL, NULL, s_api_adv_tests},
	{(char *)"functional write with basic API",
			NULL, NULL, s_func_basic_write_tests},
	{(char *)"functional read with basic API (basic search)",
			NULL, NULL, s_func_basic_read_tests},
	{(char *)"functional read with basic API (binary search)",
			use_binary_search, reset_search_method, s_func_basic_read_tests},
	{(char *)"functional read with advanced API (basic search)",
			NULL, NULL, s_func_adv_read_tests},
	{(char *)"functional read with advanced API (binary search)",
			use_binary_search, reset_search_method, s_func_adv_read_tests},
	{(char *)"functional header read functions",
			NULL, NULL, s_func_read_hdr_tests},
	{(char *)"functional write with advanced API",
			NULL, NULL, s_func_adv_write_tests},
	{(char *)"error cases", NULL, NULL, s_error_tests},
	{(char *)"concurrency tests", NULL, NULL, s_concurrency_tests},
	CU_SUITE_INFO_NULL,
};

int main(void)
{
	CU_initialize_registry();
	CU_register_suites(s_suites);
	if (getenv("CUNIT_OUT_NAME") != NULL)
		CU_set_output_filename(getenv("CUNIT_OUT_NAME"));
	if (getenv("CUNIT_AUTOMATED") != NULL) {
		CU_automated_run_tests();
		CU_list_tests_to_file();
	} else {
		CU_basic_set_mode(CU_BRM_VERBOSE);
		CU_basic_run_tests();
	}
	CU_cleanup_registry();
	return 0;
}
