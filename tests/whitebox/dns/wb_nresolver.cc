//: ----------------------------------------------------------------------------
//: Copyright (C) 2015 Verizon.  All Rights Reserved.
//: All Rights Reserved
//:
//: \file:    wb_nresolver.cc
//: \details: TODO
//: \author:  Reed P. Morrison
//: \date:    09/30/2015
//:
//:   Licensed under the Apache License, Version 2.0 (the "License");
//:   you may not use this file except in compliance with the License.
//:   You may obtain a copy of the License at
//:
//:       http://www.apache.org/licenses/LICENSE-2.0
//:
//:   Unless required by applicable law or agreed to in writing, software
//:   distributed under the License is distributed on an "AS IS" BASIS,
//:   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//:   See the License for the specific language governing permissions and
//:   limitations under the License.
//:
//: ----------------------------------------------------------------------------

//: ----------------------------------------------------------------------------
//: Includes
//: ----------------------------------------------------------------------------
#include "nresolver.h"
#include "ai_cache.h"
#include "host_info.h"
#include "time_util.h"
#include "catch/catch.hpp"

#include <unistd.h>
#include <sys/poll.h>

//: ----------------------------------------------------------------------------
//: Constants
//: ----------------------------------------------------------------------------
#define GOOD_DATA_VALUE_1 0xDEADBEEFDEADBEEFUL
#define GOOD_DATA_VALUE_2 0xDEADBEEFDEADBEEEUL
#define BAD_DATA_VALUE_1  0xDEADBEEFDEADF154UL
#define BAD_DATA_VALUE_2  0xDEADBEEFDEADF155UL

//: ----------------------------------------------------------------------------
//: Globals
//: ----------------------------------------------------------------------------
static uint32_t g_dns_reqs_qd = 0;
static uint32_t g_lkp_sucess = 0;
static uint32_t g_lkp_fail = 0;
static uint32_t g_lkp_err = 0;

//: ----------------------------------------------------------------------------
//: Test helpers
//: ----------------------------------------------------------------------------
int32_t test_resolved_cb(const ns_hlx::host_info *a_host_info, void *a_data)
{
        //printf("DEBUG: test_resolved_cb: a_host_info: %p a_data: %p\n", a_host_info, a_data);
        --g_dns_reqs_qd;
        if(a_host_info &&
           ((a_data == (void *)(GOOD_DATA_VALUE_1)) ||
            (a_data == (void *)(GOOD_DATA_VALUE_2))))
        {
                ++g_lkp_sucess;
        }
        else if(!a_host_info &&
               ((a_data == (void *)(BAD_DATA_VALUE_1)) ||
               (a_data == (void *)(BAD_DATA_VALUE_2))))

        {
                ++g_lkp_fail;
        }
        else
        {
                ++g_lkp_err;
        }

        return 0;
}

//: ----------------------------------------------------------------------------
//: Get cache key
//: ----------------------------------------------------------------------------
TEST_CASE( "get cache key", "[get_cache_key]" )
{
        SECTION("Verify get cache key")
        {
                std::string l_host = "google.com";
                uint16_t l_port = 7868;
                std::string l_label = ns_hlx::get_cache_key(l_host, l_port);
                REQUIRE((l_label == "google.com:7868"));
        }
}

//: ----------------------------------------------------------------------------
//: nresolver
//: ----------------------------------------------------------------------------
TEST_CASE( "nresolver test", "[nresolver]" )
{
        SECTION("Validate No cache")
        {
                ns_hlx::nresolver *l_nresolver = new ns_hlx::nresolver();
                l_nresolver->init("", false);
                ns_hlx::host_info *l_host_info = NULL;
                l_host_info = l_nresolver->lookup_tryfast("google.com", 80);
                REQUIRE(( l_host_info == NULL ));
                l_host_info = l_nresolver->lookup_sync("google.com", 80);
                REQUIRE(( l_host_info != NULL ));
                l_host_info = l_nresolver->lookup_tryfast("google.com", 80);
                REQUIRE(( l_host_info == NULL ));
                bool l_use_cache = l_nresolver->get_use_cache();
                ns_hlx::ai_cache *l_ai_cache = l_nresolver->get_ai_cache();
                REQUIRE(( l_use_cache == false ));
                REQUIRE(( l_ai_cache == NULL ));
                delete l_nresolver;
        }
        SECTION("Validate cache")
        {
                ns_hlx::nresolver *l_nresolver = new ns_hlx::nresolver();
                l_nresolver->init("", true);
                ns_hlx::host_info *l_host_info = NULL;
                l_host_info = l_nresolver->lookup_sync("google.com", 80);
                REQUIRE(( l_host_info != NULL ));
                l_host_info = l_nresolver->lookup_sync("yahoo.com", 80);
                REQUIRE(( l_host_info != NULL ));
                l_host_info = l_nresolver->lookup_tryfast("yahoo.com", 80);
                REQUIRE(( l_host_info != NULL ));
                l_host_info = l_nresolver->lookup_tryfast("google.com", 80);
                REQUIRE(( l_host_info != NULL ));
                bool l_use_cache = l_nresolver->get_use_cache();
                ns_hlx::ai_cache *l_ai_cache = l_nresolver->get_ai_cache();
                REQUIRE(( l_use_cache == true ));
                REQUIRE(( l_ai_cache != NULL ));
                delete l_nresolver;
        }
#ifdef ASYNC_DNS_SUPPORT
        SECTION("Validate async")
        {
                ns_hlx::nresolver *l_nresolver = new ns_hlx::nresolver();
                void *l_ctx = NULL;
                int l_fd = -1;
                l_nresolver->init_async(&l_ctx, l_fd);
                REQUIRE((l_fd > 0));
                REQUIRE((l_ctx != NULL));

                // Set up poller
                struct pollfd l_pfd;
                l_pfd.fd = l_fd;
                l_pfd.events = POLLIN;

                uint64_t l_active;
                ns_hlx::nresolver::lookup_job_q_t l_lookup_job_q;
                int32_t l_status = 0;

                // Set globals
                g_lkp_sucess = 0;
                g_lkp_fail = 0;
                g_lkp_err = 0;

                // Good
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_ctx,
                                                     "google.com", 80,
                                                     test_resolved_cb,
                                                     (void *)(GOOD_DATA_VALUE_1),
                                                     l_active, l_lookup_job_q);
                REQUIRE((l_status == 0));

                // Good
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_ctx,
                                                     "yahoo.com", 80,
                                                     test_resolved_cb,
                                                     (void *)(GOOD_DATA_VALUE_2),
                                                     l_active, l_lookup_job_q);
                REQUIRE((l_status == 0));

                // Bad
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_ctx,
                                                     "arfarhfarf$$!!//", 8948,
                                                     test_resolved_cb,
                                                     (void *)(BAD_DATA_VALUE_1),
                                                     l_active, l_lookup_job_q);
                REQUIRE((l_status == 0));

                // Bad
                ++g_dns_reqs_qd;
                l_status = l_nresolver->lookup_async(l_ctx,
                                                     "wonbaombaboiuiuiuoad.com", 80,
                                                     test_resolved_cb,
                                                     (void *)(BAD_DATA_VALUE_2),
                                                     l_active, l_lookup_job_q);
                REQUIRE((l_status == 0));


                uint32_t l_timeout_s = 5;
                uint64_t l_start_s = ns_hlx::get_time_s();
                while(g_dns_reqs_qd && ((ns_hlx::get_time_s()) < (l_start_s + l_timeout_s)))
                {
                        int l_count;
                        l_count = poll(&l_pfd, 1, 10);
                        //printf("DEBUG: poll: %d\n", l_count);
                        if(l_count)
                        {
                                l_status = l_nresolver->handle_io(l_ctx);
                                REQUIRE((l_status == 0));
                        }
                }
                uint64_t l_end_s = ns_hlx::get_time_s();

                REQUIRE((l_end_s < (l_start_s + l_timeout_s)));
                REQUIRE((g_lkp_sucess == 2));
                REQUIRE((g_lkp_fail == 2));
                REQUIRE((g_lkp_err == 0));

                l_nresolver->destroy_async(l_ctx, l_fd);
                delete l_nresolver;
        }
#endif
}

