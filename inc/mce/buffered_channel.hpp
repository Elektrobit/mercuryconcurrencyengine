//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis
/**
 @file buffered_channel.hpp 
 buffered channel implementation
 */
#ifndef __MERCURY_COROUTINE_ENGINE_BUFFERED_CHANNEL__
#define __MERCURY_COROUTINE_ENGINE_BUFFERED_CHANNEL__

// c++
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <typeinfo>

// boost
#include <boost/circular_buffer.hpp>

// local 
#include "scheduler.hpp"
#include "base_channel.hpp"

namespace mce {

/**
Asynchronous channel communication is done internally over a statically sized 
buffer. Otherwise similar to unbuffered_channel.
*/
template <typename T>
struct buffered_channel : public base_channel<T>,
                          public channel_operators<T,buffered_channel<T>>
{
    inline buffered_channel() : 
        base_channel<T>(this),
        channel_operators<T,buffered_channel<T>>(this)
    { }

    inline buffered_channel(const buffered_channel<T>& rhs) : 
        base_channel<T>(this),
        channel_operators<T,buffered_channel<T>>(this),
        ctx(rhs.ctx) 
    { }

    inline buffered_channel(buffered_channel<T>&& rhs) : 
        base_channel<T>(this),
        channel_operators<T,buffered_channel<T>>(this),
        ctx(std::move(rhs.ctx)) 
    { }

    // operations
    /// construct channel context by specifying the internal buffer size
    inline void construct(size_t sz) const
    {
        if(sz<1){ sz = 1; }
        ctx = std::make_shared<buffered_channel_context>(sz);
    }
    
    /// construct channel context with a default internal buffer size
    inline void construct() const { construct(0); }
    
    /// retrieve internal context pointer
    inline void* context() const { return (void*)(ctx.get()); }

    /// retrieve type_info
    inline const std::type_info& type_info() const { return typeid(*this); }

    /// close channel
    inline void close() const { ctx->close(); }

    /// report if channel is closed
    inline bool closed() const { return ctx->closed(); }

    /// return number of values stored in channel
    inline size_t size() const { return ctx->size(); }

    /// return if there are no values in the channel
    inline bool empty() const { return ctx->empty(); }

    /// return if channel buffer is full
    inline bool full() const { return ctx->full(); }

    /// return maximum number of storable values
    inline size_t capacity() const { return ctx->capacity(); }

    /// return count of unused values in the buffer 
    inline size_t reserve() const { return ctx->reserve(); }

    /// blocking send a copy of data through channel
    inline bool send(const T& s) const 
    { 
        return ctx->send(s,true) == result::success;
    }

    /// blocking move data through channel
    inline bool send(T&& s) const 
    { 
        return ctx->send(s,true) == result::success;
    }

    /// blocking retrieve data from channel
    inline bool recv(T& r) const 
    { 
        return ctx->recv(r,true) == result::success;
    }

    /// nonblocking attempt to send a copy of data through channel
    inline result try_send(const T& r) const { return ctx->send(r,false); }

    /// nonblocking attempt to move data through channel
    inline result try_send(T&& r) const { return ctx->send(std::move(r),false); }
    
    /// nonblocking attempt to retrieve data from channel
    inline result try_recv(T& ret) const { return ctx->recv(ret,false); }
   
    /// copy internal context of argument channel
    inline void assign(const buffered_channel<T>& rhs) const 
    { 
        this->ctx = rhs.ctx; 
    }

    /// move internal context of argument channel
    inline void assign(buffered_channel<T>&& rhs) const
    { 
        this->ctx = std::move(rhs.ctx); 
    }

private:
    struct buffered_channel_context
    {
        mutable mce::spinlock spin_lk;
        mutable bool closed_flag;
        mutable boost::circular_buffer<T> buf;
        mutable scheduler::parkable_notify_queue parked_send;
        mutable scheduler::parkable_notify_queue parked_recv;

        buffered_channel_context(size_t sz=1) : closed_flag(false), buf(sz) { }

        inline void close() const
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);
            closed_flag=true;

            auto unpark_queue = [&](scheduler::parkable_notify_queue& lst)
            {
                while(!lst.empty())
                {
                    lst.front()(NULL);
                    lst.pop_front();
                }
            };

            unpark_queue(parked_send);
            unpark_queue(parked_recv);
        }

        inline bool closed() const
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);
            return closed_flag;
        }

        // return number of values stored in channel
        inline size_t size() const
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);
            return buf.size();
        }

        inline bool empty() const
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);
            return buf.empty();
        }

        inline bool full() const
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);
            return buf.full();
        }

        // return maximum number of storable values
        inline size_t capacity() const
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);
            return buf.capacity();
        }

        inline size_t reserve() const
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);
            return buf.reserve();
        }

        inline result send_(void* s, bool block, bool is_rvalue) const
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);
            
            if(closed_flag)
            { 
                lk.unlock();
                return result::closed;
            }

            auto internal_send = [&]
            {
                // enqueue data 
                if(is_rvalue){ buf.push_back(std::move(*((T*)s))); }
                else{ buf.push_back(*((const T*)s)); }
            };

            if(buf.full())
            {
                if(block)// if full and this is a blocking send, park sender
                {
                    bool failed = false;
                    scheduler::parkable p;
                    parked_send.push_back({&p,[&](void* m)
                    {
                        if(m){ internal_send(); }
                        else{ failed = true; }
                        p.unpark(lk);
                    }});
                    p.park(lk);

                    if(failed)
                    { 
                        lk.unlock();
                        return result::closed;
                    }
                }
                else 
                { 
                    // let other coroutines run
                    lk.unlock();
                    mce::yield();
                    return result::failure;
                }
            }
            else { internal_send(); }

            if(!closed_flag) 
            {
                // if recv available, wakeup
                if(parked_recv.size() && !buf.empty())
                {
                    parked_recv.front()((void*)1);
                    parked_recv.pop_front();
                }
            }

            // let other coroutines run
            lk.unlock();
            mce::yield();
            return result::success;
        }
        
        inline result send(const T& s, bool block) const 
        { 
            return send_((void*)&s,block,false); 
        }

        inline result send(T&& s, bool block) const 
        { 
            return send_((void*)&s,block,true); 
        }

        inline result recv(T& r, bool block) const
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);

            if(closed_flag)
            { 
                lk.unlock();
                return result::closed;
            }

            auto internal_recv = [&]
            {
                r = std::move(buf.front());
                buf.pop_front();
            };

            if(buf.empty()) 
            {
                // if empty and this is a blocking recv, park receiver
                if(block)
                {
                    bool failed = false;
                    scheduler::parkable p;
                    parked_recv.push_back({&p,[&](void* m)
                    {
                        if(m){ internal_recv(); }
                        else { failed = true; }
                        p.unpark(lk);
                    }});
                    p.park(lk);

                    if(failed)
                    { 
                        lk.unlock();
                        return result::closed;
                    }
                }
                else 
                { 
                    // let other coroutines run
                    lk.unlock();
                    mce::yield();
                    return result::failure;
                }
            }
            else { internal_recv(); }

            if(!closed_flag)
            {
                // check if any senders are available, unpark one
                if(parked_send.size() && !buf.full())
                {
                    // senders have no memory m pointer to manage
                    parked_send.front()((void*)1);
                    parked_send.pop_front();
                }
            }

            // let other coroutines run
            lk.unlock();
            mce::yield();
            return result::success;
        }
    };

    // Forcibly ignore the existence of the const concept
    mutable std::shared_ptr<buffered_channel_context> ctx;
};

}

#endif
