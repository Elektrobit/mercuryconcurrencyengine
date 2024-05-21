//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
/**
 @file timer.hpp 
 time based operations api
 */
#ifndef __MERCURY_COROUTINE_ENGINE_TIMER__
#define __MERCURY_COROUTINE_ENGINE_TIMER__

// c++ includes
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <thread>
#include <utility>
#include <list>

// local  
#include "function_utility.hpp"
#include "atomic.hpp"
#include "scheduler.hpp"

namespace mce {

enum time_unit 
{
    hour,
    minute,
    second,
    millisecond,
    microsecond,
    nanosecond
};

// use steady clock as we don't want to risk the clock changing near boot if 
// if clock is determined by a runtime source
typedef std::chrono::steady_clock::time_point time_point; 
typedef std::chrono::steady_clock::duration duration;

// Timer utility functions

/// Return duration for count units of the given time unit
inline mce::duration get_duration(mce::time_unit u, size_t count)
{
    mce::duration dur;
    switch(u)
    {
        case time_unit::hour:
            dur = std::chrono::hours(count);
            break;
        case time_unit::minute:
            dur = std::chrono::minutes(count);
            break;
        case time_unit::second:
            dur = std::chrono::seconds(count);
            break;
        case time_unit::millisecond:
            dur = std::chrono::milliseconds(count);
            break;
        case time_unit::microsecond:
            dur = std::chrono::microseconds(count);
            break;
        default:
            dur = std::chrono::milliseconds(0);
            break;
    }
    return dur;
}

/// Return the difference in time units between two time points
inline size_t get_time_point_difference(mce::time_unit u, mce::time_point p0, mce::time_point p1)
{
    size_t dif=0;

    auto calc =  p0 > p1 ? (p0 - p1) : (p1 - p0);

    switch(u)
    {
        case time_unit::hour:
            dif = (size_t)(std::chrono::duration_cast<std::chrono::hours>(calc).count());
            break;
        case time_unit::minute:
            dif = (size_t)(std::chrono::duration_cast<std::chrono::minutes>(calc).count());
            break;
        case time_unit::second:
            dif = (size_t)(std::chrono::duration_cast<std::chrono::seconds>(calc).count());
            break;
        case time_unit::millisecond:
            dif = (size_t)(std::chrono::duration_cast<std::chrono::milliseconds>(calc).count());
            break;
        case time_unit::microsecond:
            dif = (size_t)(std::chrono::duration_cast<std::chrono::microseconds>(calc).count());
            break;
        default:
            break;
    }

    return dif;
}

/// Return the current time. All mce timer time operations are calculated using this function.
inline mce::time_point current_time() 
{
    return std::chrono::steady_clock::now();
}


/**
A tiny asynchronous timer service implementation. This service is not 
designed to work inside of coroutines and is unsafe to do so, it will almost 
certainly cause deadlock. To safely interact with coroutines, extra work 
must be done so that the timeout_handler executes threadsafe code (which can 
include rescheduling a coroutine or notifying a coroutine via a channel).

Start the service:
mce::timer_service my_timer_service;
std::thread thd([&]{ my_timer_service.start(); }).detach();
my_timer_service.ready(); // block until started

Usage is as simple as:
mce::timer_id tid = my_timer_service.timer(mce::time_unit::microsecond, 
                                           microsecs_till_timeout, 
                                           my_timeout_handler,
                                           false);

The timer can be synchronously removed (if it is not already executing) with:
my_timer_service.remove(tid);
*/
struct timer_service 
{
    struct timer_id
    {
        // Utilizing the uniqueness of allocated smart pointers in comparison
        bool operator==(const timer_id& rhs) { return valid == rhs.valid; }

    private:
        // The validy of the timer_id token is determined by if the callback for 
        // this timer has completed or not. Once callback finishes, 
        // *valid==false.
        //
        // Read/write to this value can only be done by the timer_service when 
        // holding its spinlock.
        std::shared_ptr<bool> valid;

        friend struct timer_service;
    };

    /// default constructor
    timer_service() :
        continue_running_(false),
        new_timers_(false),
        running_(false),
        executing_remove_sync_required_(false),
        waiting_for_timeouts_(false)
    { }

    /**
    Shutdown and join with asynchronous timer service if shutdown() has 
    not been previously called.
    */
    ~timer_service(){ shutdown(); }

    /// start timer service on current thread
    inline void start()
    {
        std::unique_lock<mce::spinlock> lk(mut_);
        continue_running_ = true;
        running_ = true;
        executing_remove_sync_required_ = false;
        waiting_for_timeouts_ = false;
        ready_cv_.notify_all();

        // will never return unless shutdown() is called
        check_timers(lk);

        running_ = false;
        join_cv_.notify_all();
    }

    /// blocks until service is running
    inline void ready()
    {
        std::unique_lock<mce::spinlock> lk(mut_);
        while(!running_){ ready_cv_.wait(lk); }
    }
    
    /// inform service to shutdown and join with service
    inline void shutdown()
    { 
        std::unique_lock<mce::spinlock> lk(mut_);
        if(continue_running_) { continue_running_ = false; }
        cv_.notify_one(); // wakeup sleeping thread
        while(running_) { join_cv_.wait(lk); } 
    }
    
    /**
     @brief start timer 

     @param timeout the time when the timeout_handler should be executed 
     @param timeout_handler a Callable accepting no arguments 
     @return a timer id object
     */
    template <typename THUNK>
    timer_id timer(const mce::time_point& timeout, THUNK&& timeout_handler)
    {
        return create_timer({ timeout, std::forward<THUNK>(timeout_handler) });
    } 

    
    /**
     @brief start timer 

     @param timeout the duration till the timeout_handler should be executed 
     @param timeout_handler a Callable accepting no arguments 
     @return a timer id object
     */
    template <typename THUNK>
    timer_id timer(const mce::duration& d, THUNK&& timeout_handler)
    {
        time_point tp = d+current_time();
        return timer(tp, std::forward<THUNK>(timeout_handler));
    }
    
    
    /**
     @brief start timer 

     @param time_unit the unit of time measurement to be used
     @param count the time_unit count till the timeout_handler should be executed 
     @param timeout_handler a Callable accepting no arguments 
     @return a timer id object
     */
    template <typename THUNK>
    timer_id timer(const time_unit u, size_t count, THUNK&& timeout_handler)
    {
        time_point timeout;

        switch(u)
        {
            case time_unit::hour:
                timeout = current_time() + std::chrono::hours(count);
                break;
            case time_unit::minute:
                timeout = current_time() + std::chrono::minutes(count);
                break;
            case time_unit::second:
                timeout = current_time() + std::chrono::seconds(count);
                break;
            case time_unit::millisecond:
                timeout = current_time() + std::chrono::milliseconds(count);
                break;
            case time_unit::microsecond:
                timeout = current_time() + std::chrono::microseconds(count);
                break;
            default:
                timeout = current_time();
                break;
        }

        return timer(timeout, std::forward<THUNK>(timeout_handler));
    }
    
    /// return true if timer is running, else false
    inline bool running(timer_id id)
    {
        std::lock_guard<mce::spinlock> lk(mut_);
        return *(id.valid) || executing_timer_ == id;
    }

    /**
     @brief remove a running timer 

     If an enqueued timer is found, it is removed. If the target timer is 
     *currently* executing, this function will block and not return until the 
     timeout handler completes, ensuring the target timer does not exist by the
     time this function returns.

     @return true if timer was found and removed, else returns false.
     */
    inline bool remove(timer_id id)
    {
        bool success = false;

        std::unique_lock<mce::spinlock> lk(mut_);

        if(executing_timer_ == id)
        {
            // block until timeout handler finishes
            do
            {
                executing_remove_sync_required_ = true;
                remove_sync_cv.wait(lk);
            } while(executing_timer_ == id);
        }
        else if(*(id.valid))
        {
            *(id.valid) = false;

            timer_queue::iterator it;
            for(it = timers_.begin(); it != timers_.end(); ++it)
            {
                if(it->id == id) 
                {
                    success = true;
                    it = timers_.erase(it);
                    break;
                }
            }
        }

        return success;
    }
    
    /**
     @brief remove all pending timers

     WARNING: This is a very, *very* dangerous operation. Any timeout handler 
     which expects to be called and has no handling for being destroyed early 
     may cause *problems* in code.
     */
    inline void clear()
    {
        std::lock_guard<mce::spinlock> lk(mut_);
        timers_.clear();
    }

    /// Return the number of running timers
    inline size_t count()
    {
        std::unique_lock<mce::spinlock> lk(mut_);
        return timers_.size();
    }

private:
    struct timer_data
    {
        mce::time_point tp;
        thunk timeout_handler;
        timer_id id;

        bool operator<(const timer_data& rhs)
        {
            return tp < rhs.tp;
        }
    };

    bool continue_running_;
    bool new_timers_;
    bool running_;
    bool executing_remove_sync_required_;
    bool waiting_for_timeouts_;

    timer_id executing_timer_;

    using timer_queue = std::list<timer_data>;
   
    // queue of timers ordered by soonest timeout to latest
    timer_queue timers_; 

    mce::spinlock mut_;
    std::condition_variable_any cv_;
    std::condition_variable_any ready_cv_;
    std::condition_variable_any join_cv_;
    std::condition_variable_any remove_sync_cv;
    
    timer_id create_timer(timer_data&& td);

    inline timer_id get_id()
    {
        timer_id id;
        id.valid = std::make_shared<bool>(true);
        return id;
    }

    inline void check_timers(std::unique_lock<mce::spinlock>& lk)
    {
        bool resume_required = false;
        mce::time_point cur_time;
        mce::time_point sleep_time;
        timer_queue::iterator it;
        
        while(continue_running_) 
        { 
            do 
            {
                new_timers_ = false;
                cur_time = current_time();
                it = timers_.begin(); 

                while(it != timers_.end()) 
                {
                    // handle timeout
                    if(it->tp <= cur_time) 
                    {
                        executing_timer_ = it->id;
                        *(executing_timer_.valid) = false;

                        {
                            // move timer from map
                            const thunk t = std::move(it->timeout_handler);

                            // do not use this again until checked in while()
                            timers_.erase(it);

                            // execute timeouts *outside* the service lock in case a 
                            // timeout calls this timer service
                            lk.unlock();
                            t();

                            // ensure timeout thunk is destroyed at this point
                        }

                        lk.lock();

                        executing_timer_.valid.reset();

                        if(executing_remove_sync_required_)
                        {
                            executing_remove_sync_required_ = false;
                            remove_sync_cv.notify_all();
                        }

                        // during callback execution service was shutdown, stop 
                        // evaluating timers
                        if(!continue_running_) { break; }
                    } 
                    else 
                    { 
                        // We've reached the last timer we need to iterate. Set sleep 
                        // time to the current timeout.
                        resume_required = true;
                        sleep_time = it->tp;
                        break; 
                    }

                    cur_time = current_time();
                    it = timers_.begin();
                }

            // repeat if timer() was called during timeout execution
            } while(new_timers_);


            // don't do any sleeps (especially not indefinite ones) if we know we need 
            // to shutdown
            if(continue_running_) 
            { 
                // Release lock and sleep until woken up or sleep_time has been reached 
                if(resume_required) 
                { 
                    waiting_for_timeouts_ = true;
                    cv_.wait_until(lk,sleep_time); 
                    resume_required = false;
                }
                else 
                { 
                    // no timers running, block until timers are added
                    waiting_for_timeouts_ = true;
                    cv_.wait(lk); 
                } 
            }
        }
    }
};

/// define mce::timer_id
typedef timer_service::timer_id timer_id;

/// Access to default mce::timer_service object
timer_service& default_timer_service();

namespace detail {

struct timeout_handler_wrapper
{
    // reschedule handler
    inline void operator()()
    { 
        if(scheduler) { scheduler->schedule(timeout_handler); }
        else { timeout_handler(); }
    }

    std::shared_ptr<mce::scheduler> scheduler;
    mce::thunk timeout_handler;
};

}

/**
 @brief launch a timer with a Callable to be called on timeout 
 @param u time_unit used 
 @param count count of time_units representing time until timeout 
 @param cb a Callable (function, Functor, lambda) to execute on timeout 
 @param as... optional arguments to cb 
 @return a timer_id associated with the launched timer
 */
template <typename Callable, typename... As>
timer_id timer(time_unit u, size_t count, Callable&& cb, As&&... as)
{
    return default_timer_service().timer(
        u,
        count,
        detail::timeout_handler_wrapper { 
            in_scheduler() ? this_scheduler() : std::shared_ptr<scheduler>(),
            make_thunk(std::forward<Callable>(cb), std::forward<As>(as)...)
        });
}

/**
 @brief launch a timer with a Callable to be called on timeout 
 @param timeout a time_point when the timer will timeout
 @param cb a Callable (function, Functor, lambda) to execute on timeout 
 @param as... optional arguments to cb 
 @return a timer_id associated with the launched timer
 */
template <typename Callable, typename... As>
timer_id timer(const mce::time_point& timeout, Callable&& cb, As&&... as)
{
    return default_timer_service().timer(
        timeout,
        detail::timeout_handler_wrapper { 
            in_scheduler() ? this_scheduler() : std::shared_ptr<scheduler>(),
            make_thunk(std::forward<Callable>(cb), std::forward<As>(as)...)
        });
}

/**
 @brief launch a timer with a Callable to be called on timeout 
 @param timeout a duration until the timer will timeout
 @param cb a Callable (function, Functor, lambda) to execute on timeout 
 @param as... optional arguments to cb 
 @return a timer_id associated with the launched timer
 */
template <typename Callable, typename... As>
timer_id timer(const mce::duration& timeout, Callable&& cb, As&&... as)
{
    return default_timer_service().timer(
        timeout,
        detail::timeout_handler_wrapper { 
            in_scheduler() ? this_scheduler() : std::shared_ptr<scheduler>(),
            make_thunk(std::forward<Callable>(cb), std::forward<As>(as)...)
        });
}

/// remove a running timer, return true if successful, else false
inline bool remove_timer(timer_id id)
{ 
    return default_timer_service().remove(id);
}

/// return a count of running timers
inline size_t count_timers()
{
    return default_timer_service().count(); 
}

/**
 @brief Put coroutine or thread to sleep in a blocking fashion 
 @param d a duration until the coroutine should resume
 @return false if the coroutine was awoken early due to timer being cleared, else true
 */
inline bool sleep(mce::duration d)
{ 
    // timeout handler functor
    struct wakeup 
    {

        // callback implementation only sets the success flag, indicating that 
        // the timer timed out properly instead of being cleared early
        inline void operator()() { *success = true; }

        struct resumer 
        {
            // the actual logic is in the destructor in this case because we need 
            // to ensure that the coroutine wakes up again no matter what
            ~resumer()
            {
                std::unique_lock<mce::spinlock> lk(*slk);
                pk->unpark(lk);
            }

            mce::scheduler::parkable* pk;
            mce::spinlock* slk;
        };

        bool* success;
        std::shared_ptr<resumer> resumer_;
    };

    // data persists on the coroutine or thread stack 
    mce::scheduler::parkable pk;
    mce::spinlock slk; 
    bool success = false;

    std::unique_lock<mce::spinlock> lk(slk);

    timer(
        d+current_time(), 
        wakeup{ 
            &success, 
            std::shared_ptr<wakeup::resumer>(new wakeup::resumer{ &pk, &slk })
        }
    );

    pk.park(lk);

    return success;
}

/**
 @brief Put coroutine or thread to sleep in a blocking fashion 
 @param u time_unit used 
 @param count count of time_units representing time until timeout 
 @return false if the coroutine was awoken early due to timer being cleared, else true
 */
inline bool sleep(time_unit u, size_t count)
{
    return sleep(get_duration(u, count));
}

}
#endif
