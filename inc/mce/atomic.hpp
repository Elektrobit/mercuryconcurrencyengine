//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
/**
 @file atomic.hpp  
 Essential atomic synchronization primitives
 */
#ifndef __MERCURY_COROUTINE_ENGINE_ATOMIC__
#define __MERCURY_COROUTINE_ENGINE_ATOMIC__

// c++
#include <atomic>
#include <mutex>

namespace mce {

/**
@brief Core mechanism for atomic synchronization. 
*/
struct spinlock 
{
    spinlock()
    {
        lock_.clear();
    }

    inline void lock()
    { 
        while(lock_.test_and_set(std::memory_order_acquire)){ } 
    }

    inline bool try_lock()
    { 
        return !(lock_.test_and_set(std::memory_order_acquire)); 
    }

    inline void unlock()
    { 
        lock_.clear(std::memory_order_release); 
    }

private:
    std::atomic_flag lock_;
};

}

#endif
