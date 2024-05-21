//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "atomic.hpp"
#include "await.hpp"

#ifdef MCE_DISABLE_DEFAULT_THREADS
#define MCE_DISABLE_DEFAULT_await_THREADS
#endif

// Ensure MCEMINAWAITPROCS is set. MCEMINAWAITPROCS is a compiler define provided to 
// allow user code to specify the minimum number of threads in the default await
// threadpool. If the value is 0, the number of threads is determined by this 
// library.
//
// Modifying this value is probably only necessary in circumstances where the 
// user code is expected to execute a lot of await calls, in which case they can 
// specify a minimum based on profiling.
#ifndef MCEMINAWAITPROCS
#define MCEMINAWAITPROCS 0
#endif

bool& mce::detail::await_worker::tl_is_await()
{
    thread_local bool is_await = false;
    return is_await;
}

mce::detail::await_threadpool::await_threadpool() : 
    min_worker_cnt_(MCEMINAWAITPROCS ? MCEMINAWAITPROCS : 1),
    worker_cnt_(min_worker_cnt_),
    workers_(min_worker_cnt_) 
{ 
    for(auto& worker : workers_) 
    { 
        worker = std::unique_ptr<await_worker>(new await_worker); 
    }
}

#ifdef MCE_DISABLE_DEFAULT_await_THREADS 
mce::detail::await_threadpool& mce::detail::await_threadpool::instance()
{
    static mce::detail::await_threadpool await_tp(mce::detail::await_threadpool::no_threads_t());
    return await_tp;
}
#else
mce::detail::await_threadpool& mce::detail::await_threadpool::instance()
{
    static mce::detail::await_threadpool await_tp;
    return await_tp;
}
#endif 
