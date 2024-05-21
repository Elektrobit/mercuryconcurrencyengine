//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
/**
 @file unbuffered_channel.hpp 
 unbuffered channel implementation
 */
#ifndef __MERCURY_COROUTINE_ENGINE_UNBUFFERED_CHANNEL__
#define __MERCURY_COROUTINE_ENGINE_UNBUFFERED_CHANNEL__

// c++
#include <memory>
#include <utility>
#include <type_traits>
#include <typeinfo>

// local 
#include "scheduler.hpp"
#include "base_channel.hpp"

// test, only uncomment for development of this library
//#include "dev_print.hpp"

namespace mce {

//-----------------------------------------------------------------------------
/**
unbuffered_channels are communication objects between arbitrary code 
(coroutine or otherwise). They are the easiest (and probably best and safest) 
methods of coroutine communication. Best practice is to use these to 
communicate state in place of mutex locking state variables or using 
condition variables. 

Communication over an unbuffered_channel requires both the putter code and 
the getter code be synchronized before either can progress. 
*/
template <typename T>
struct unbuffered_channel : public base_channel<T>, 
                            public channel_operators<T,unbuffered_channel<T>>
{ 
    inline unbuffered_channel() : 
        base_channel<T>(this),
        channel_operators<T,unbuffered_channel<T>>(this)
    { }

    inline unbuffered_channel(const unbuffered_channel<T>& rhs) : 
        base_channel<T>(this),
        channel_operators<T,unbuffered_channel<T>>(this),
        ctx(rhs.ctx) 
    { }

    inline unbuffered_channel(unbuffered_channel<T>&& rhs) : 
        base_channel<T>(this),
        channel_operators<T,unbuffered_channel<T>>(this),
        ctx(std::move(rhs.ctx)) 
    { }

    // operations
    /// construct channel context
    inline void construct() const
    {
        ctx = std::make_shared<unbuffered_channel_context>();
    }

    /// retrieve internal context pointer
    inline void* context() const { return (void*)(ctx.get()); }

    /// retrieve type_info
    inline const std::type_info& type_info() const { return typeid(*this); }

    /// close channel
    inline void close() const { return ctx->close(); }
    
    /// report if channel is closed
    inline bool closed() const { return ctx->closed(); }

    /// blocking send a copy of data through channel
    inline bool send(const T& s) const 
    { 
        return ctx->send(s,true) == result::success;
    }

    /// blocking move data through channel
    inline bool send(T&& s) const 
    { 
        return ctx->send(std::move(s),true) == result::success;
    }

    /// blocking retrieve data from channel
    inline bool recv(T& r) const 
    { 
        return ctx->recv(r,true) == result::success;
    }

    /// nonblocking attempt to send a copy of data through channel
    inline result try_send(const T& s) const { return ctx->send(s,false); }
    
    /// nonblocking attempt to move data through channel
    inline result try_send(T&& s) const { return ctx->send(std::move(s),false); }

    /// nonblocking attempt to retrieve data from channel
    inline result try_recv(T& r) const { return ctx->recv(r,false); }
   
    /// copy internal context of argument channel
    inline void assign(const unbuffered_channel<T>& rhs) const 
    { 
        this->ctx = rhs.ctx; 
    }

    /// move internal context of argument channel
    inline void assign(unbuffered_channel<T>&& rhs) const
    { 
        this->ctx = std::move(rhs.ctx); 
    }

private:
    struct send_pair 
    {
        bool is_rvalue;
        void* target;

        send_pair(void* rhs_target, bool rhs_is_rvalue) : 
            is_rvalue(rhs_is_rvalue),
            target(rhs_target)
        { }
    };

    struct unbuffered_channel_context
    {
        mce::spinlock spin_lk;
        bool closed_flag;
        scheduler::parkable_notify_queue parked_send;
        scheduler::parkable_notify_queue parked_recv;

        unbuffered_channel_context() : closed_flag(false) {}

        inline void close() 
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

        inline bool closed() 
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);
            return closed_flag;
        }

        inline result send_(void* s, bool block, bool is_rvalue) 
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);

            if(closed_flag)
            { 
                lk.unlock();
                return result::closed;
            }

            // park if no recv is available
            if(parked_recv.empty())
            {
                if(block)
                {
                    bool failed = false;
                    scheduler::parkable p;
                    parked_send.push_back({&p,[&](void* m)
                    {
                        if(m)
                        { 
                            if(is_rvalue){ *((T*)m) = std::move(*((T*)s)); }
                            else{ *((T*)m) = *((const T*)s); }
                        }
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
            // else send value
            else  
            {
                send_pair sp(s,is_rvalue);
                parked_recv.front()((void*)(&sp));
                parked_recv.pop_front();
            }

            // let other coroutines run
            lk.unlock();
            mce::yield();
            return result::success;
        }
        
        inline result send(const T& s, bool block)
        { 
            return send_((void*)&s,block,false); 
        }

        inline result send(T&& s, bool block) 
        { 
            return send_((void*)&s,block,true); 
        }

        inline result recv(T& r, bool block) 
        {
            std::unique_lock<mce::spinlock> lk(spin_lk);

            if(closed_flag)
            { 
                lk.unlock();
                return result::closed;
            }

            // park if no send is available
            if(parked_send.empty())
            {
                if(block)
                {
                    bool failed = false;
                    scheduler::parkable p;
                    parked_recv.push_back({&p,[&](void* m)
                    {
                        if(m)
                        { 
                            send_pair* sp = (send_pair*)m;
                            if(sp->is_rvalue)
                            {
                                r = std::move(*((T*)(sp->target)));
                            }
                            else{ r = *((const T*)(sp->target)); }
                        }
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
            // else recv value
            else  
            {
                parked_send.front()(&r);
                parked_send.pop_front();
            }

            // let other coroutines run
            lk.unlock();
            mce::yield();
            return result::success;
        }
    };

    // Forcibly ignore the existence of the const concept
    mutable std::shared_ptr<unbuffered_channel_context> ctx;
};

}

#endif
