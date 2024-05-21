//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "scheduler.hpp"

// test, only uncomment for development of this library
//#include "dev_print.hpp"

mce::scheduler*& mce::detail::tl_this_scheduler() 
{
    thread_local mce::scheduler* tlts = nullptr;
    return tlts;
}

mce::scheduler*& mce::detail::tl_this_scheduler_redirect() 
{
    thread_local mce::scheduler* tlts = nullptr;
    return tlts;
}
