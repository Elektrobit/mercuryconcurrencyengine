//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "threadpool.hpp"

mce::threadpool*& mce::detail::tl_this_threadpool() 
{
    thread_local mce::threadpool* tltp = nullptr;
    return tltp;
}

// Ensure MCEMAXPROCS is set. MCEMAXPROCS is a compiler define provided to allow 
// user code to specify a number of threads in the default threadpool. If the 
// value is 0, the number of threads is determined by this library.
#ifndef MCEMAXPROCS 
#define MCEMAXPROCS 0
#endif

bool mce::default_threadpool_enabled() { return true; }

mce::threadpool& mce::default_threadpool()
{
    struct default_threadpool_manager
    {
        default_threadpool_manager() :
            tp(mce::threadpool::make(MCEMAXPROCS))
        { }

        ~default_threadpool_manager() { tp->halt(); }
        
        std::shared_ptr<mce::threadpool> tp;
    };

    static default_threadpool_manager dtm;
    return *(dtm.tp);
}

#ifndef MCEBALANCERATIO
#define MCEBALANCERATIO 1.5
#endif

double mce::balance_ratio(){ return MCEBALANCERATIO; }

mce::scheduler& mce::detail::default_threadpool_scheduler()
{
    static mce::scheduler& sch = mce::default_threadpool().worker(0);
    return sch;
}
