//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
/**
@file coroutine.hpp 
Wrapper coroutine object around boost coroutine base types
*/
#ifndef __MERCURY_COROUTINE_ENGINE_COROUTINE__
#define __MERCURY_COROUTINE_ENGINE_COROUTINE__

// c++
#include <thread>
#include <memory>

// boost
#include <boost/coroutine2/all.hpp>

// local
#include "function_utility.hpp"

// test, only uncomment for development of this library
//#include "dev_print.hpp"

namespace mce {

struct coroutine;

namespace detail {

// always points to the running coroutine
coroutine*& tl_this_coroutine();

}

//-----------------------------------------------------------------------------
// coroutine
//----------------------------------------------------------------------------- 
/**
A coroutine object. Argument thunk can yield execution at any point using 
mce::yield(). make_thunk() from the thunk.hpp is helpful for making executable 
functions to pass to coroutine constructor.
*/
struct coroutine
{
    /**
     @brief construct an allocated coroutine from a Callable and arguments
     @param cb a Callable
     @param as arguments to pass to Callable
     @return an allocated coroutine
     */
    template <typename Callable, typename... As>
    static std::unique_ptr<coroutine> make(Callable&& cb, As&&... as)
    {
        return std::unique_ptr<coroutine>(
            new coroutine(
                make_thunk(
                    std::forward<Callable>(cb),
                    std::forward<As>(as)...)));
    }

    /// construct a coroutine from a thunk
    template <typename THUNK>
    coroutine(THUNK&& th) :
        yield_(nullptr),
        co_(co_t::pull_type(wrapper_functor{ yield_, std::forward<THUNK>(th) }))
    { }

    /// construct a coroutine from a stack allocator and thunk
    template <typename StackAllocator, typename THUNK>
    coroutine(StackAllocator&& sa, THUNK&& th) :
        yield_(nullptr),
        co_(std::forward<StackAllocator>(sa), 
            co_t::pull_type(wrapper_functor{ yield_, std::forward<THUNK>(th) }))
    { }

    // Copy Constructor (Not allowed)
    coroutine(const coroutine& source) = delete;

    /// Move Constructor
    coroutine(coroutine&& rhs) noexcept :
        yield_(std::move(rhs.yield_)),
        co_(std::move(rhs.co_))
    { }

    // Copy Assignment (Not allowed)
    coroutine& operator=(const coroutine& rhs) = delete;

    /// Move Assignment
    coroutine& operator=(coroutine&& rhs) noexcept
    {
        yield_ = std::move(rhs.yield_);
        co_ = std::move(rhs.co_);
        return *this;
    }

    virtual ~coroutine(){}

    /**
     @brief Execute until thunk completes or yield() is called 

    Execute until thunk completes or yield() is called. If thunk is complete 
    run() returns immediately.

    This operation is virtual to allow for code injection at run() call.
    */
    virtual inline void run()
    {
        if(!complete())
        {
            auto& tl_co = detail::tl_this_coroutine();

            // store parent coroutine
            auto parent_co = tl_co;

            // set current coroutine ptr 
            tl_co = this; 
            
            // continue coroutine execution
            try { co_(); }
            catch(...)
            {
                // restore parent coroutine ptr
                tl_co = parent_co;
                std::rethrow_exception(std::current_exception());
            }

            // restore parent coroutine ptr
            tl_co = parent_co;
        };
    }

    /**
     @brief Pause execution and return to run() caller 

     Pause execution and return to run() caller. Should only be called from 
     within the associated coroutine while said coroutine is running.
     */
    inline void yield() 
    { 
        (*yield_)(); 
    }

    /// Returns true if thunk is complete, else false since the coroutine yielded early.
    inline bool complete()
    { 
        return !(co_.operator bool());
    }

private:
    typedef boost::coroutines2::coroutine<void> co_t;

    struct wrapper_functor
    {
        inline void operator()(coroutine::co_t::push_type& yield)
        {
            yield_ = &yield;
            yield(); // yield after coroutine construction but before execution 
            th(); // execute thunk when coroutine resumes
        }

        co_t::push_type*& yield_;
        mce::thunk th;
    };

    co_t::push_type* yield_;
    co_t::pull_type co_;
};

/// Returns true if executing in a coroutine, else false
inline bool in_coroutine()
{ 
    return detail::tl_this_coroutine() ? true : false; 
}

/**
Return a pointer to the currently running coroutine object. If not 
running in a coroutine returns nullptr.
*/
inline coroutine* this_coroutine()
{ 
    return detail::tl_this_coroutine(); 
}

/**
Yield out of the current coroutine. No effect if running in a raw thread.
*/
inline void yield()
{ 
    coroutine* c = detail::tl_this_coroutine();
    if(c) { c->yield(); }
}

}

#endif
