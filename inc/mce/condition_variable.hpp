//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
/**
@file condition_variable.hpp 
threadsafe and coroutinesafe condition variable
 */
#ifndef __MERCURY_COROUTINE_ENGINE_CONDITION_VARIABLE__
#define __MERCURY_COROUTINE_ENGINE_CONDITION_VARIABLE__

// c++
#include <deque>
#include <thread>
#include <condition_variable>
#include <memory>

// local 
#include "atomic.hpp"
#include "scheduler.hpp"
#include "timer.hpp"

namespace mce {

/**
Condition variable class safe to use in both mce coroutines, normal threaded 
code, or any mixture of the two.
*/
struct condition_variable 
{
    condition_variable() {}
    condition_variable(condition_variable&& rhs) = delete;
    condition_variable(const condition_variable& rhs) = delete;
    condition_variable& operator=(condition_variable&& rhs) = delete;
    condition_variable& operator=(const condition_variable& rhs) = delete;

    /**
    Block until unpark is called for this blocker
    */
    template <class Lock>
    void wait(Lock& lk) 
    { 
        scheduler::parkable p;

        // interleave locks
        std::unique_lock<mce::spinlock> inner_lk(lk_);
        lk.unlock();

        auto key = borrow_key();
        notify_queue_.push_back({ key, { &p, unparker{&p} }});
        p.park(inner_lk);
        return_key(key);

        inner_lk.unlock();
        lk.lock();
    }

    /**
    Block until unpark is called for this blocker and predicate is satisfied
    */
    template <class Lock, class Pred>
    void wait(Lock& lk, Pred p)
    {
        while(!p()) { wait(lk); }
    }

    /**
    Block until unpark is called for this blocker or duration since start of 
    the operation expires
    */
    template <class Lock, class Rep, class Period>
    std::cv_status wait_for(Lock& lk, const std::chrono::duration<Rep, Period>& d)
    {
        auto tp = d+mce::current_time();
        return wait_until(lk, tp);
    }

    /**
    Block until unpark is called for this blocker or duration since start of 
    the operation expires
    */
    template <class Lock, class Rep, class Period, class Pred>
    bool wait_for(Lock& lk, const std::chrono::duration<Rep, Period>& d, Pred p)
    {
        bool result = false;
        std::cv_status status = std::cv_status::no_timeout;

        while(!(result = p())) 
        { 
            status = wait_for(lk, d); 

            // exit loop early because the wait operation took too long and we
            // timed out
            if(status == std::cv_status::timeout) { break; }
        }

        return result;
    }

    /**
    Block until unpark is called for this blocker or until point in time is 
    reached
    */
    template <class Lock, class Clock, class Duration>
    std::cv_status wait_until(
        Lock& user_lk, 
        const std::chrono::time_point<Clock, Duration>& tp)
    {
        std::cv_status status = std::cv_status::no_timeout;
        timer_id id;
        scheduler::parkable p;
        bool notify_available = true;

        // interleave locks
        std::unique_lock<mce::spinlock> lk(lk_);
        user_lk.unlock();
      
        // acquire a unique key for this operation
        auto key = borrow_key();

        notify_queue_.push_back({ 
            key, 
            { &p, unparker_with_flag{&p, &notify_available} } 
        });

        // start the timer 
        id = ts_.timer(
            tp,
            clear_safe_handler{
                std::make_shared<clear_safe_handler::resumer>(
                    [&]
                    { 
                        std::unique_lock<mce::spinlock> lk(lk_);
                        if(notify_available) 
                        { 
                            status = std::cv_status::timeout;
                            notify_with_key(lk,key);
                        }
                    })
            });

        // release the lock and park until unpark or timeout
        p.park(lk);

        // return the key
        return_key(key);

        // release the condition_variable's private lock
        lk.unlock();

        // remove timer if it is enqueued, this operation is synchronized such 
        // that it is guaranteed to not return until the timer is removed and 
        // its timeout handler is not executing
        ts_.remove(id);

        // re-acquire user lock
        user_lk.lock();

        return status;
    }

    /**
    Block until unpark is called for this blocker or until point in time is 
    reached
    */
    template <class Lock, class Clock, class Duration, class Pred>
    void wait_until(Lock& lk, 
                    std::chrono::time_point<Clock, Duration> tp, 
                    Pred p)
    {
        while(!p()) { wait_until(lk, tp); }
    }

    /// Unblock one context calling a wait operation
    inline void notify_one()
    {
        std::unique_lock<mce::spinlock> lk(lk_);
        auto it = notify_queue_.begin();
        if(it != notify_queue_.end())
        {
            it->second((void*)&lk);
            notify_queue_.erase(it);
        }
    }

    /// Unblock all contexts calling a wait operation
    inline void notify_all()
    {
        std::unique_lock<mce::spinlock> lk(lk_);
        auto it = notify_queue_.begin();
        while(it != notify_queue_.end())
        {
            it->second((void*)&lk);
            ++it;
        }
        notify_queue_.clear();
    }

private:
    typedef size_t key_type;
    typedef std::deque<std::pair<key_type,scheduler::parkable_notify>> notify_queue;

    // ensure the handler is called even if the timer is cleared
    struct clear_safe_handler 
    {
        struct resumer 
        {
            resumer(thunk hdl) : 
                hdl_(std::move(hdl)) 
            { }

            ~resumer(){ hdl_(); }
            const thunk hdl_;
        };

        // timeout was reached
        inline void operator()(){ }

        std::shared_ptr<resumer> resumer_;
    };

    struct unparker
    {
        unparker(scheduler::parkable* p) : p_(p) { }
        inline void operator()(void* m) { p_->unpark(*((std::unique_lock<mce::spinlock>*)m)); }
        scheduler::parkable* p_;
    };

    struct unparker_with_flag
    {
        unparker_with_flag(scheduler::parkable* p, bool* flag) :
            p_(p),
            flag_(flag)
        { }

        inline void operator()(void* m) 
        { 
            // this is a guard from unparking twice, in the case that a 
            // call to notify_one/all wakes this up before timeout and 
            // before the timeout is successfully removed
            if(*flag_)
            {
                *flag_ = false;
                p_->unpark(*((std::unique_lock<mce::spinlock>*)m));
            }
        }

        scheduler::parkable* p_;
        bool* flag_;
    };

    // Acquire or generate a unique key. This method requires caller to 
    // implicitly own a lock
    inline size_t borrow_key()
    {
        size_t ret;

        if(!free_keys_.empty())
        {
            ret = free_keys_.front();
            free_keys_.pop_front();
        }
        else 
        {
            ret = key_source_;
            ++key_source_;
        }

        return ret;
    }

    inline void return_key(key_type key) { free_keys_.push_back(key); }

    // Notify a specific blocked operation, unlike notify_one()/notify_all() 
    // which are agnostic.
    inline void notify_with_key(std::unique_lock<mce::spinlock>& lk, key_type key)
    {
        notify_queue::iterator it;
        for(it = notify_queue_.begin(); it != notify_queue_.end(); ++it)
        {
            if(it->first == key) 
            { 
                it->second((void*)&lk);
                notify_queue_.erase(it);
                break; 
            }
        }
    }

    mce::spinlock lk_;
    notify_queue notify_queue_;
    std::deque<key_type> free_keys_;
    size_t key_source_ = 0;
    mce::timer_service& ts_ = default_timer_service();
};

}
#endif
