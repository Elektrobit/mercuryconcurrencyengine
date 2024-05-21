//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
/**
 @file threadpool.hpp 
 threadpool executor API
 */
#ifndef __MERCURY_COROUTINE_ENGINE_THREADPOOL__
#define __MERCURY_COROUTINE_ENGINE_THREADPOOL__

// c 
#include <limits.h>

// c++
#include <vector>
#include <mutex>
#include <iostream>
#include <algorithm>
#include <memory>

// local
#include "function_utility.hpp"
#include "timer.hpp"
#include "scheduler.hpp"

namespace mce {

struct threadpool;

namespace detail {

mce::threadpool*& tl_this_threadpool();

}

/**
Threadpool objects launch, maintain, and shutdown worker threads running schedulers
*/
struct threadpool : public lifecycle, protected lifecycle::implementation
{
    /**
     @brief construct an allocated threadpool with a count of worker threads
     */
    static inline std::shared_ptr<threadpool> make(size_t worker_count = 0)
    {
        threadpool* tpp = new threadpool(worker_count);
        std::shared_ptr<threadpool> tp(tpp);
        tpp->self_wptr_ = tp;
        tp->init_();
        return tp;
    };

    /// halt(), join() and delete all workers
    virtual ~threadpool() { }

    /// return the count of worker threads
    inline size_t size() const { return workers_schedulers_.size(); }
    
    /// access the scheduler for a worker at a given index
    inline scheduler& worker(size_t idx) const
    {
        return *(workers_schedulers_[idx]);
    }

    /// return the least busy worker scheduler at time of call
    inline scheduler& worker() 
    { 
        const size_t start_idx = current_scheduler_idx_();
        auto sz = workers_schedulers_.size();
        size_t i = start_idx;
        size_t end = sz; // end is our 'break out of loop' index
        auto least_weight = workers_schedulers_[i]->measure();
        scheduler* ret = workers_schedulers_[i];
        bool found_empty = false;

        ++i; // start comparisons at index 1

        auto compare = [&]
        {
            for(; i<end; ++i)
            {
                // acquire the current scheduling load of a scheduler
                auto cur_weight = workers_schedulers_[i]->measure();

                // end iteration if we find an empty scheduler
                if(cur_weight) 
                {
                    if(cur_weight < least_weight)
                    {
                        // update return value
                        least_weight = cur_weight;
                        ret = workers_schedulers_[i];
                    }
                }
                else 
                {
                    found_empty = true;
                    ret = workers_schedulers_[i];
                    break;
                }
            }
        };

        compare();

        // if we didn't find a 0 weight scheduler and our start_idx > 0
        if(!found_empty && start_idx)
        {
            i = 0; // rotate back around to 0
            end = start_idx; // our new end is our original beginning
            compare();
        }

        return *ret;
    }

    /**
     This operation is more expensive than calling worker(). It should be
     unnecessary in most cases to call this method, but it is provided in case
     some sort of high level scheduler management is desired by the user and 
     access to all of `std::vector`'s utility functionality would be beneficial.

     @return the schedulers for all running worker threads
     */
    inline std::vector<std::shared_ptr<scheduler>> workers() 
    {
        std::vector<std::shared_ptr<scheduler>> ret(workers_schedulers_.size());

        std::transform(
            workers_schedulers_.begin(),
            workers_schedulers_.end(),
            ret.begin(),
            [](scheduler* sch){ return (std::shared_ptr<scheduler>)(*sch); }
        );

        return ret;
    }

    /// return a copy of this threadpool's shared pointer by conversion
    inline operator std::shared_ptr<threadpool>() { return self_wptr_.lock(); }

protected:
    // return the workers' state
    inline lifecycle::state get_state_impl()
    {
        std::lock_guard<mce::spinlock> lk(lk_);

        auto lf = (lifecycle::implementation*)(workers_schedulers_.front());

        // state should be the same on all workers
        return lf->get_state_impl();
    }

    // suspend all workers, returning true if all workers suspend() == true, else false
    inline bool suspend_impl()
    {
        bool ret = true;

        std::lock_guard<mce::spinlock> lk(lk_);

        for(auto& sch : workers_schedulers_) 
        {
            ret = ret && ((lifecycle::implementation*)sch)->suspend_impl();
        }

        return ret;
    }

    // resume all workers
    inline void resume_impl()
    {
        std::lock_guard<mce::spinlock> lk(lk_);

        for(auto& sch : workers_schedulers_) 
        {
            ((lifecycle::implementation*)sch)->resume_impl();
        }
    }

    // halt all workers
    inline void halt_impl()
    {
        std::lock_guard<mce::spinlock> lk(lk_);

        for(auto& worker : workers_memory_) 
        { 
            auto lf = (lifecycle::implementation*)(worker->sch.get());

            if(lf->get_state_impl() != lifecycle::state::halted)
            {
                lf->halt_impl();
                worker->thd.join();
            }
        }
    }

private:
    struct worker_thread
    {
        std::shared_ptr<scheduler> sch;
        std::thread thd;

        worker_thread(std::shared_ptr<threadpool> tp) :
            sch(scheduler::make(tp.get())),
            thd([tp,this]() mutable
            { 
                auto& tl_tp = detail::tl_this_threadpool();
                auto parent_tp = tl_tp;
                tl_tp = tp.get();

                try { while(this->sch->run()){ } }
                catch(...)
                {
                    tl_tp = parent_tp;
                    std::rethrow_exception(std::current_exception());
                }

                tl_tp = parent_tp;
            })
        { }

        worker_thread() = delete;
        worker_thread(worker_thread&&) = delete;

        ~worker_thread() 
        {
            auto lf = (lifecycle::implementation*)(sch.get());

            if(lf->get_state_impl() != lifecycle::state::halted)
            { 
                lf->halt_impl(); 
            }

            if(thd.joinable()) { thd.join(); }
        }
    };

    threadpool(size_t worker_count) :
        lifecycle(this),
        workers_memory_(
            [=]() mutable -> size_t
            { 
                if(worker_count == 0) 
                {
                    worker_count = std::thread::hardware_concurrency(); 

                    // enforce a minimum of 1 worker threads
                    if(worker_count == 0) { worker_count = 1; }
                }

                return worker_count;
            }()),
        workers_schedulers_(workers_memory_.size())
    { }

    // separate worker init from constructor so self shared_ptr can be setup
    inline void init_()
    {
        auto self = self_wptr_.lock();
        auto it = workers_schedulers_.begin();
       
        // initialize worker threads, no need for synchronization because no 
        // operations are scheduled on the schedulers till
        // threadpool::make() returns.
        for(auto& w : workers_memory_)
        { 
            w = std::unique_ptr<worker_thread>(new worker_thread(self)); 
            *it = w->sch.get();
            ++it;
        }
    }

    // return the index of the worker we should measure() first
    inline size_t current_scheduler_idx_()
    {
        std::lock_guard<mce::spinlock> lk(lk_);

        auto ret = current_scheduler_idx_val_;

        // rotate current scheduler to limit lock contention
        if((current_scheduler_idx_val_+1) < workers_memory_.size())
        {
            ++current_scheduler_idx_val_;
        }
        else 
        {
            current_scheduler_idx_val_ = 0;
        }

        return ret;
    }

    // as a general rule, anything relying on access to lk_ should not block 
    // on anything else for the duration of the lock
    mce::spinlock lk_;

    // avoid circular shared memory structures through a weak_ptr
    std::weak_ptr<threadpool> self_wptr_;
    
    // This vector never changes post initialization until the threadpool is 
    // destroyed. The fact that this vector doesn't change which worker is 
    // stored in what index is important to ensure that calls to 
    // `workers(size_t)` are consistent.
    //
    // Because this vector never changes until threadpool is destroyed, it can 
    // be read without a lock. However, a lock may be required for synchronized 
    // calls to scheduler operations.
    std::vector<std::unique_ptr<worker_thread>> workers_memory_;

    // A vector of schedulers. This vector is not changed post init till 
    // destruction; it can be read without a lock.
    std::vector<scheduler*> workers_schedulers_;

    // value which wraps around back to 0 when incremented past the max size,
    // used for limiting lock contention by ensuring measurement()s are taken 
    // equally among all schedulers
    size_t current_scheduler_idx_val_ = 0;
};

/// Return true if calling context is running in a threadpool
inline bool in_threadpool()
{ 
    return detail::tl_this_threadpool();
}

/// return a reference to the threadpool the calling code is executing in
inline threadpool& this_threadpool()
{ 
    return *detail::tl_this_threadpool();
}

/// return true if default_threadpool() can be safely called, else false
bool default_threadpool_enabled();

/// return the default threadpool's
threadpool& default_threadpool();

/// return the balance ratio, set by compiler define: MCEBALANCERATIO
double balance_ratio();

namespace detail {

// select an arbitrary scheduler from the default_threadpool to always return
scheduler& default_threadpool_scheduler();

inline scheduler& concurrent_algorithm()
{
    return in_scheduler()
        ? this_scheduler()
        : default_threadpool_scheduler();
}

inline scheduler& parallel_algorithm()
{
    return in_threadpool() 
        ? this_threadpool().worker()
        : default_threadpool().worker();
}

inline scheduler& balance_algorithm()
{
    if(in_threadpool())
    {
        /// scheduler with least workload 
        scheduler* least_sch;

        // return true if the workload is imbalanced, else false
        auto imbalanced = [&]() -> bool
        {
            auto& tp = this_threadpool();
            size_t sz = tp.size();

            /// result of scheduler::measure() with the least load
            scheduler::measurement least;

            /// result of scheduler::measure() for the worker with the greatest load  
            scheduler::measurement most;
            
            {
                auto& sch = tp.worker(0);

                least_sch = &(sch);
                least = sch.measure();
                most = least;
            }

            // begin on index 1, we've already taken 0
            for(size_t i=1; i<sz; ++i)
            {
                auto& sch = tp.worker(i);
                scheduler::measurement weight = sch.measure(); 

                if(weight < least) 
                { 
                    least_sch = &(sch);
                    least = weight; 
                }
                else if(weight > most) { most = weight; }
            }

            // returns true if the difference between workloads is greater than 
            // the balance_ratio()
            auto past_limit = [](size_t lhs, size_t rhs) -> bool
            {
                // cast to long double for a floating point division
                return (static_cast<long double>(lhs) / rhs) >= balance_ratio();
            };
                
            return past_limit(most.scheduled(), least.scheduled());
        };

        return imbalanced()
            ? *least_sch // select the least burdened scheduler
            // select the current thread's scheduler
            : this_scheduler();
    }
    else { return default_threadpool().worker(); }
}

}

/**
 @brief Launch user function and optional arguments as a coroutine running on a scheduler.

 Prefers to schedule coroutines on the thread local scheduler for fastest 
 context switching (allows for fastest communication with other coroutines 
 running on the current thread) with the default threadpool as a fallback.

 This is the recommended way to launch a coroutine unless multicore processing 
 is required. Even then, it is recommended that child coroutines be scheduled 
 with concurrent() as communication speed is often the bottleneck in small
 asynchronous programs.
 */
template <typename... As>
void concurrent(As&&... args)
{
    detail::concurrent_algorithm().schedule(std::forward<As>(args)...);
}

/**
 @brief Launch user function and optional arguments as a coroutine running on a scheduler.

 Prefers to be scheduled on the current threadpool for maximally efficient CPU 
 distribution (allows for greater CPU throughput at the cost of potentially 
 slower communication between coroutines), but will fallback to the thread local 
 scheduler or to the default threadpool.

 If some coroutine consistently spawns many child coroutines, it may be useful 
 to schedule them with `mce::parallel()` instead of `mce::concurrent()` to 
 encourage balanced usage of CPU resources.
 */
template <typename... As>
void parallel(As&&... args)
{
    detail::parallel_algorithm().schedule(std::forward<As>(args)...);
}

/**
 @brief Launch user function and optional arguments as a coroutine running on a scheduler.
 A best-effort algorithm for balancing CPU throughput and communication latency.
 
 This algorithm sacrifices initial scheduling launch time to calculate whether 
 the workload of the current threadpool needs to be rebalanced or else 
 preferring scheduling on the calling thread's scheduler. That is, it is slower 
 than either `mce::concurrent()` or `mce::parallel()` to initially schedule 
 coroutines (it has no further effect on coroutines once they have been 
 scheduled the first time). However if initial scheduling of coroutines is not a
 bottleneck, this mechanism may be used to encourage balanced usage of computing 
 resources without unnecessarily sacrificing communication latency gains.

 This algorithm may be most useful in long running programs, where neither CPU 
 throughput nor communication latency are the highest demand but consistency 
 *is*. In such a case, the user may consider replacing many or all usages of 
 `mce::concurrent()` with `mce::balance()`, exchanging a minor scheduling cost 
 for a long term stability increase..

 Template value `RATIO_LIMIT` the maximum ratio value of dividing the largest by 
 the smallest load before rebalancing. For instance, with a `RATIO_LIMIT` of 
 `2.0`, the scheduler with the heaviest load must be twice as loaded as the 
 lightest scheduler before rebalancing occurs. 

 The operating theory of this algorithm is that if rebalancing occurs only when 
 it is truly necessary, then the newly scheduled coroutines will themselves 
 schedule additional operations on their thread (instead of the thread of their 
 parent coroutine), allowing the workload to balance naturally. Manual 
 rebalancing will recur until the scheduled coroutines are naturally scheduling 
 additional coroutines on their current threads in roughly equal amounts. 

 @param f a Callable 
 @param as... any Callable arguments
 */
template <typename... As>
void balance(As&&... args)
{
    detail::balance_algorithm().schedule(std::forward<As>(args)...);
}

}
#endif
