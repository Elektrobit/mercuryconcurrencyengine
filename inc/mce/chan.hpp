//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis
/**
 @file chan.hpp 
 generic wrapper class for any channel which implements base_channel
 */
#ifndef __MERCURY_COROUTINE_ENGINE_CHAN__
#define __MERCURY_COROUTINE_ENGINE_CHAN__ 

// c++
#include <type_traits>
#include <typeinfo>
#include <memory>

// local 
#include "base_channel.hpp"
#include "unbuffered_channel.hpp"

// test, only uncomment for development of this library
//#include "dev_print.hpp"

//-----------------------------------------------------------------------------
namespace mce {

//-----------------------------------------------------------------------------
/** 
A wrapper object which can represent any object which implements base_channel 
interface.
*/
template <typename T>
struct chan : public base_channel<T>,
              public channel_operators<T,chan<T>>
{
    inline chan() : 
        base_channel<T>(this),
        channel_operators<T,chan<T>>(this)
    { }

    inline chan(const chan<T>& rhs) : 
        base_channel<T>(this),
        channel_operators<T,chan<T>>(this),
        ctx(rhs.ctx) 
    { }

    inline chan(chan<T>&& rhs) : 
        base_channel<T>(this),
        channel_operators<T,chan<T>>(this),
        ctx(std::move(rhs.ctx)) 
    { }

    // operations
    /// unique mechanism for constructing a chan<T> object using any object which 
    /// implements base_channel<T,chan<T>>
    template <typename CHANNEL>
    inline void construct(CHANNEL&& ch) const 
    { 
        ctx = std::unique_ptr<base_channel<T>>(
            allocate_context(std::forward<CHANNEL>(ch)));
    }

    /// construct unbuffered channel context
    inline void construct() const 
    {
        unbuffered_channel<T> ch;
        ch.construct();
        construct(std::move(ch));
    }

    /// retrieve internal context pointer
    inline void* context() const { return ctx ? ctx->context() : NULL; }

    /// retrieve type_info
    inline const std::type_info& type_info() const { return typeid(*this); }

    /**
     Retrieve type_info of underlying context. Because chan<T> is a wrapper to 
     other channels, this may fail if chan<T> is uninitialized at the time of 
     call.
     */
    inline const std::type_info& context_type_info() const 
    { 
        return ctx->type_info();
    }

    /**
     Cast base_channel context to templated channel and return the result. 

     Operation was a success if the resulting channel is initialized, a 
     non-default constructed version of the channel. An initialized channel will 
     return non-NULL when context() is called.
     */
    template <typename CHANNEL>
    inline CHANNEL cast() const 
    { 
        if(context() && ctx->type_info() == typeid(CHANNEL))
        {
            return *(dynamic_cast<CHANNEL*>(ctx.get()));
        } 
        else 
        {
            return CHANNEL();
        }
    }

    /// close channel
    inline void close() const { ctx->close(); }
    
    /// report if channel is closed
    inline bool closed() const { return ctx->closed(); }

    /// send a copy of data through channel
    inline bool send(const T& s) const { return ctx->send(s); }

    /// move data through channel
    inline bool send(T&& s) const { return ctx->send(std::move(s)); }

    /// retrieve data from channel
    inline bool recv(T& r) const { return ctx->recv(r); }

    /// attempt to send a copy of data through channel
    inline result try_send(const T& s) const { return ctx->try_send(s); }
    
    /// attempt to move data through channel
    inline result try_send(T&& s) const { return ctx->try_send(std::move(s)); }

    /// attempt to retrieve data from channel
    inline result try_recv(T& s) const { return ctx->try_recv(s); }
   
    /// copy internal context of argument channel
    inline void assign(const chan<T>& rhs) const { this->ctx = rhs.ctx; }

    /// move internal context of argument channel
    inline void assign(chan<T>&& rhs) const { this->ctx = std::move(rhs.ctx); }

    // operators
    /// lvalue assign channel context
    inline const chan<T>& operator=(const chan<T>& rhs) const
    {
        assign(rhs);
        return *this;
    }

    /// rvalue assign channel context
    inline const chan<T>& operator=(chan<T>&& rhs) const 
    {
        assign(std::move(rhs));
        return *this;
    }

private:
    mutable std::shared_ptr<base_channel<T>> ctx;

    template <typename CHANNEL>
    base_channel<T>* allocate_context(CHANNEL&& ch) const
    {
        typedef typename std::decay<CHANNEL>::type DECAY_CH;
        return dynamic_cast<base_channel<T>*>(
                new DECAY_CH(std::forward<CHANNEL>(ch))
        );
    }
};

}

#endif
