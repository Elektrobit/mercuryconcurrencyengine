//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
/**
@file mutex.hpp 
threadsafe and coroutinesafe mutex
 */
#ifndef __MERCURY_COROUTINE_ENGINE_MUTEX__
#define __MERCURY_COROUTINE_ENGINE_MUTEX__

// c++
#include <mutex>

// locals 
#include "atomic.hpp"
#include "scheduler.hpp"

namespace mce {

// Mutex class safe to use in both coroutines and normal code. 
struct mutex 
{
    mutex() : acquired_{false} { }
    mutex(const mce::mutex&) = delete;

    /// Lock the mutex, blocking until lock is acquired
    inline void lock()
    {
        std::unique_lock<mce::spinlock> lk(lk_);
        if(acquired_)
        {
            do
            {
                scheduler::parkable p;
                notify_q_.push_back(&p);
                p.park(lk);
            } while(acquired_);
            acquired_ = true;
        }
        else { acquired_ = true; }
    }

    /// Unlock the mutex
    inline void unlock()
    { 
        std::unique_lock<mce::spinlock> lk(lk_);
        if(acquired_) 
        { 
            acquired_ = false;
            if(notify_q_.size())
            {
                notify_q_.front()->unpark(lk);
                notify_q_.pop_front();
            }
        }
        else{ throw mutex_already_unlocked_exception(); }
    }

    /// Attempt to the lock the mutex, returning true if successful, else false
    inline bool try_lock() // returns true if lock acquired, else false
    { 
        std::unique_lock<mce::spinlock> lk(lk_);
        if(!acquired_)
        {
            acquired_ = true;
            return true;
        }
        else { return false; }
    }

    class mutex_already_unlocked_exception : public std::exception
    {
        virtual const char* what() const throw()
        {
            return "Cannot unlock an already unlocked mutex";
        }
    };

private:
    // spinlock always acquired *temporarily*. This is required to synchronize 
    // the coroutine parking process.
    mce::spinlock lk_;
    bool acquired_;
    scheduler::parkable_queue notify_q_;
};

}

#endif
