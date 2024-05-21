//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
/**
 @file await.hpp 
 trivial execution of blocking operations (such as in/out operations) 
*/
#ifndef __MERCURY_COROUTINE_ENGINE_AWAIT__
#define __MERCURY_COROUTINE_ENGINE_AWAIT__

#include <deque>
#include <memory>

#include "function_utility.hpp"
#include "scheduler.hpp"
#include "threadpool.hpp"
#include "unbuffered_channel.hpp"

namespace mce {

//-----------------------------------------------------------------------------
// await operations
//----------------------------------------------------------------------------- 

namespace detail {

/*
 A coroutine which redirects user this_scheduler() and this_threadpool() 
 operations to point to the coroutine's original scheduler/threadpool, 
 implicitly allowing user scheduling to function correctly.
 */
struct await_coroutine : public coroutine 
{
    template <typename... As>
    await_coroutine(std::shared_ptr<threadpool>&& t, std::shared_ptr<scheduler>&& s, As&&... as) : 
        coroutine(std::forward<As>(as)...),
        original_threadpool(std::move(t)),
        original_scheduler(std::move(s))
    { }

    virtual ~await_coroutine(){}

    template <typename... As>
    static std::unique_ptr<coroutine> make(
            std::shared_ptr<threadpool>&& t,
            std::shared_ptr<scheduler>&& s, 
            As&&... as)
    {
        return std::unique_ptr<coroutine>(
            static_cast<coroutine*>(
                new await_coroutine{
                    std::move(t), 
                    std::move(s), 
                    std::forward<As>(as)...}));

    }

    /*
     Every time this coroutine runs we manage the scheduler redirect pointer and 
     threadpool pointer so that if they attempt to access this_scheduler(),
     or this_threadpool(), they will actually get the coroutine's original 
     scheduler/threadpool, not the await_worker's scheduler/threadpool.
     */
    virtual inline void run()
    {
        threadpool*& tl_tp = detail::tl_this_threadpool();
        scheduler*& tl_cs_re = detail::tl_this_scheduler_redirect();

        threadpool* parent_tp = tl_tp;
        scheduler* parent_cs = tl_cs_re;

        tl_tp = original_threadpool.get();
        tl_cs_re = original_scheduler.get();

        try { coroutine::run(); }
        catch(...)
        {
            tl_cs_re = parent_cs;
            tl_tp = parent_tp;
            std::rethrow_exception(std::current_exception());
        }

        tl_cs_re = parent_cs;
        tl_tp = parent_tp;
    }

    std::shared_ptr<threadpool> original_threadpool;
    std::shared_ptr<scheduler> original_scheduler;
    std::unique_ptr<coroutine> co;
};

// workers implicitly start a scheduler on a new thread during 
// construction and shutdown said scheduler during destruction.
struct await_worker
{
    std::shared_ptr<scheduler> sch;
    std::thread thd;

    // thread_local value defaults to false
    static bool& tl_is_await();

    // a functor which takes a copy of the scheduler pointer
    struct worker_task 
    {
        inline void operator()() 
        { 
            // thread_local value is true when executing as an IO worker thread
            tl_is_await() = true;
            sch->run(); 
        }

        scheduler* sch;
    };

    await_worker() : 
        // construct the scheduler
        sch(scheduler::make()),
        // spawn a worker thread with a running scheduler
        thd(std::thread(worker_task{ sch.get() }))
    { }

    ~await_worker()
    {
        if(sch)
        {
            sch->halt();
            thd.join();
        }
    }
};

/**
 @brief specialized threadpool implementation for handling blocking tasks
 */
struct await_threadpool 
{
    /// schedule a callable on the await_threadpool
    template <typename Callable, typename... As>
    static void schedule(Callable&& cb, As&&... args)
    {
        if(in_scheduler() && !detail::await_worker::tl_is_await())
        {
            auto& tl_await_tp = await_threadpool::instance();

            // synchronization between caller and callee
            mce::scheduler::parkable pk;
            mce::spinlock slk; 

            // acquire a worker thread running a scheduler
            auto w = tl_await_tp.checkout_worker();

            {
                std::unique_lock<mce::spinlock> lk(slk);

                w->sch->schedule(detail::await_coroutine::make(
                    mce::in_threadpool() 
                        ? (std::shared_ptr<threadpool>)mce::this_threadpool() 
                        : std::shared_ptr<threadpool>(),
                    mce::in_scheduler() 
                        ? (std::shared_ptr<scheduler>)mce::this_scheduler() 
                        : std::shared_ptr<scheduler>(), 
                    [&]
                    { 
                        // execute Callable in coroutine running on the worker thread
                        cb(std::forward<As>(args)...);
                        
                        std::unique_lock<mce::spinlock> lk(slk);
                        pk.unpark(lk); // resume caller
                    }));

                pk.park(lk); // block until callee unparks
            }

            tl_await_tp.checkin_worker(std::move(w));
        }
        else
        {
            // execute Callable directly if not in a coroutine running in a 
            // scheduler OR if we are already executing in a parent mce::await() 
            // call. No need to worry about blocking other running coroutines... 
            // we already have a dedicated system thread.
            cb(std::forward<As>(args)...);
        }
    }

    static inline size_t worker_count() 
    { 
        return await_threadpool::instance().get_worker_count();
    }

private:
    struct no_threads_t { };

    await_threadpool();
    await_threadpool(no_threads_t) : min_worker_cnt_(0), worker_cnt_(0) { }
    ~await_threadpool(){}

    /// there is only ever one global await_threadpool
    static await_threadpool& instance();

    inline std::unique_ptr<await_worker> checkout_worker() 
    {
        {
            std::unique_lock<mce::spinlock> lk(lk_);
            if(workers_.size()) 
            {
                auto w = std::move(workers_.front());
                workers_.pop_front();
                // return the first available worker
                return w;
            }
        }

        // as a fallback generate a new await worker thread
        ++worker_cnt_;
        return std::unique_ptr<await_worker>(new await_worker);
    }

    inline void checkin_worker(std::unique_ptr<await_worker>&& w)
    {
        std::unique_lock<mce::spinlock> lk(lk_);

        if(workers_.size() < min_worker_cnt_)
        {
            workers_.push_back(std::move(w));
        }
        else 
        {
            --worker_cnt_;
        }
    }
    
    inline size_t get_worker_count() 
    { 
        std::unique_lock<mce::spinlock> lk(lk_);
        return worker_cnt_;
    }

    const size_t min_worker_cnt_;
    size_t worker_cnt_;
    mce::spinlock lk_;
    std::deque<std::unique_ptr<await_worker>> workers_;
};

template <typename Callable, typename... As>
detail::function_return_type<Callable,As...>
await_(std::false_type, Callable&& cb, As&&... args)
{
    typedef detail::function_return_type<Callable,As...> R;
    
    R r;

    // assign the return value to the stack
    await_threadpool::schedule([&]{ r = cb(std::forward<As>(args)...); });

    return r;
}

template <typename Callable, typename... As>
int
await_(std::true_type, Callable&& cb, As&&... args)
{
    await_threadpool::schedule([&]{ cb(std::forward<As>(args)...); });
    return 0;
}

}

/**
@brief Execute Callable potentially on a different thread and block current context until operation completes

await() accepts a Callable, and any number of arguments, and returns the
result of calling the given Callable with the given arguments. await() is a 
special function which enables blocking Callable calls made from a coroutine 
to be executed safely so that it does not block other coroutines from 
running.

Anything that does an OS level block (non-mce sleeping, std::mutex::lock(), 
std::condition_variable::wait(), file operations, etc...) can be executed 
within this call without blocking other scheduler coroutines.

Because the caller of `mce::await()` is blocked during this operations runtime,
it is safe to access values on the caller's stack by reference.

If `mce::await()` is called outside of a coroutine running on a `mce::scheduler` 
OR `mce::await()` was called inside another call to `mce::await()`, the argument 
Callable is executed immediately on the current thread instead of on another 
thread. Otherwise, the calling coroutine will be blocked an the operation passed 
to `mce::await()` will be executed on another, dedicated thread inside another 
`mce::scheduler`.

Calls (or implicit calls) to `this_scheduler()`, or `this_threadpool()` made 
within a call to `mce::await()` will return the values they would have if they 
had been called outside of `mce::await()`. That is, code running in 
`mce::await()` will be able to schedule operations as if it was running in its 
original execution environment. IE, calls to `mce::concurrent()` or 
`mce::parallel()` will function "normally".

@return the result of the Callable or 0 (if Callable returns void)
*/
template <typename Callable, typename... As>
inline detail::convert_void_return<Callable,As...>
await(Callable&& cb, As&&... args)
{
    using isv = typename std::is_void<detail::function_return_type<Callable,As...>>;
    return detail::await_(
        std::integral_constant<bool,isv::value>(),
        std::forward<Callable>(cb),
        std::forward<As>(args)...);
}

/**
 @return true if executing on an await() managed thread, else false
 */
inline bool is_await() { return detail::await_worker::tl_is_await(); }

/**
 @return the count of active await worker threads
 */
inline size_t await_count() { return detail::await_threadpool::worker_count(); }

}

#endif
