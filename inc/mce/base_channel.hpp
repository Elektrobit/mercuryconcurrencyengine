//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis
/**
@file base_channel.hpp 
pure virtual base channel interface definition
 */
#ifndef __MERCURY_COROUTINE_ENGINE_BASE_CHANNEL__
#define __MERCURY_COROUTINE_ENGINE_BASE_CHANNEL__

#include <typeinfo>
#include <iterator>
#include <exception>
#include <memory>

// Code in this header is shared by all Channels

namespace mce {

/**
 @brief enum for channel operation results
 */
enum result 
{
    // ensure closed is convertable to `false`, allowing while(try_...()) operation loops
    closed = 0, /// channel is closed, operation failed
    success, /// blocking/nonblocking operation succeeded
    failure /// nonblocking operation failed
};

/**
Channels must implement this pure virtual interface to be a valid channel 
object for purposes of 'chan' wrapper.

For clarification, the reason all operations and returned references are 
const is because channels are designed to *ignore* const designations. This 
is because Channels are designed as a trivially easy mechanism for communication 
and are intended to be copied by capture in lambdas. Unfortunately, unless the 
lambda in question is designated "mutable" the captures will become const by
default, which would make channels useless. 

While this design quirk is certainly an anti-pattern, it exists to combat the 
greater anti pattern of having to mark every user lambda as mutable (which also 
limits the usable forms of lambda notation). So for the sake simplicity and 
readability all derived channels which inherit base_channel will typically need 
to specify its members as mutable.
*/
template <typename T>
struct base_channel 
{
    /// template type for channel
    typedef T value_type;

    /// iterator context constructor
    base_channel(const base_channel<T>* rhs_bc) : bc(rhs_bc) { }

    virtual ~base_channel(){}

    /*
    derived channels should implement the following constructors:
    
    derived_channel();
    derived_channel(const derived_channel<T>& rhs);
    derived_channel(derived_channel<T>&& rhs);
    */

    // operations 
    
    /// construct channel context
    virtual void construct() const = 0;

    /// retrieve internal context pointer
    virtual void* context() const = 0; 

    /// retrieve type_info
    virtual const std::type_info& type_info() const = 0;
    
    /// close channel
    virtual void close() const = 0; 
    
    /// report if channel is closed
    virtual bool closed() const = 0; 
    
    /// send a copy of data through channel
    virtual bool send(const T& s) const = 0; 

    /// move data through channel
    virtual bool send(T&& s) const = 0; 

    /// retrieve data from channel
    virtual bool recv(T& r) const = 0; 

    /// attempt to send a copy of data through channel
    virtual result try_send(const T& s) const = 0; 

    /// attempt to move data through channel
    virtual result try_send(T&& s) const  = 0; 

    /// attempt to retrieve data from channel
    virtual result try_recv(T& r) const  = 0; 

    /*
    Derived channels should implement the following operators. They should 
    typically be implemented by directly calling the above pure virtual 
    operations:

    bool operator=(const derived_channel<T>& rhs) const;
    bool operator=(derived_channel<T>&& rhs) const;
    bool operator==(const derived_channel<T>& rhs) const;
    bool operator==(derived_channel<T>&& rhs) const;
    bool operator!=(const derived_channel<T>& rhs) const;
    bool operator!=(derived_channel<T>&& rhs) const;
    const derived_channel<T>& operator<<(const T& s) const;
    const derived_channel<T>& operator<<(T&& s) const;
    T& operator>>(T& r) const;
    */

    //TODO: There is potentially room to turn this into a virtual interface for 
    //large and small channel iterators. IE, rather than use a T for all values, 
    //can use a uptr<T> for large values and T for trivially small 
    //(wordsize?) values and convert this class to a pure virtual interface to 
    //the large & small iterator classes.
    class iterator : public std::input_iterator_tag
    {
    private:
        struct iterator_context 
        {
            iterator_context() : bc(NULL), good(false) { }
            iterator_context(const base_channel<T>* rhs_bc) : bc(rhs_bc), good(true) { }

            inline T& dereference() const { return t; }
            inline T* arrow() const { return &t; }

            inline bool increment() const 
            {
                if(good && !bc->recv(t)) { good=false; }
                return good;
            }

            const base_channel<T>* bc;
            mutable T t;
            mutable bool good;
        };

        inline void make(const base_channel<T>* bc)
        {
            ctx = std::make_shared<iterator::iterator_context>(bc);
        }

        mutable std::shared_ptr<iterator_context> ctx;
        friend struct base_channel<T>;

    public:
        typedef T value_type; //< iterator templated value type
        
        iterator(){ } //< default constructor
        iterator(const iterator& rhs) : ctx(rhs.ctx){ } //< copy constructor
        iterator(iterator&& rhs) noexcept : ctx(std::move(rhs.ctx)){ } //< move constructor

        /// lvalue iterator assignment
        inline const iterator& operator=(const iterator& rhs) const  
        {
            ctx = rhs.ctx;
            return *this;
        }

        /// rvalue iterator assignment
        inline const iterator& operator=(iterator&& rhs) const  
        {
            ctx = std::move(rhs.ctx);
            return *this;
        }

        /// lvalue iterator comparison
        inline bool operator==(const iterator& rhs) const  
        { 
            return ctx == rhs.ctx;
        }

        /// rvalue iterator comparison
        inline bool operator==(iterator&& rhs) const { return *this == rhs; }

        /// lvalue iterator not comparison
        inline bool operator!=(const iterator& rhs) const { return !(*this == rhs); }

        /// rvalue iterator not comparison
        inline bool operator!=(iterator&& rhs) const { return *this != rhs; }

        /// retrieve reference to cached value T
        inline T& operator*() const { return ctx->dereference(); }

        /// retrieve pointer to cached value T
        inline T* operator->() const { return ctx->arrow(); }

        /// retrieve data from the channel iterator
        inline const iterator& operator++() const 
        {
            if(!ctx->increment()){ ctx.reset(); }
            return *this;
        }

        /// retrieve data from the channel iterator
        inline const iterator operator++(int) const 
        { 
            if(!ctx->increment()){ ctx.reset(); }
            return *this; 
        }
    };

    /// iterator to the current beginning of the channel
    inline iterator begin() const
    {
        iterator it;
        it.make(bc);
        ++it; // get an initial value
        return it;
    }

    /// default iterator == end()
    inline iterator end() const { return iterator(); } 

private: 
    const base_channel<T>* bc; // used for iterator
};

// CRTP (Curiously Recurring Template Pattern) for implementing shared channel 
// operator overloads
template <typename T, typename CHANNEL>
struct channel_operators
{
    ///  context constructor
    channel_operators(CHANNEL* rhs_ch) : ch(rhs_ch) { }

    virtual ~channel_operators(){}

    virtual void assign(const CHANNEL& rhs) const = 0;
    virtual void assign(CHANNEL&& rhs) const = 0;
   
    template <typename... As>
    static CHANNEL make(As&&... as)
    {
        CHANNEL ch;
        ch.construct(std::forward<As>(as)...);
        return ch;
    }

    /// return whether channel has a shared context pointer
    inline explicit operator bool() const 
    { 
        return ch->context() ? true : false; 
    }

    // operators
    /// lvalue assign channel context
    inline const CHANNEL& operator=(const CHANNEL& rhs) const
    {
        ch->assign(rhs);
        return *ch;
    }

    /// rvalue assign channel context
    inline const CHANNEL& operator=(CHANNEL&& rhs) const 
    {
        ch->assign(std::move(rhs));
        return *ch;
    }
    
    /// lvalue channel context comparison
    inline bool operator==(const CHANNEL& rhs) const
    {
        return ch->context() == rhs.context();
    }

    /// rvalue channel context comparison
    inline bool operator==(CHANNEL&& rhs) const
    {
        return *ch == rhs;
    }

    /// lvalue channel context not comparison
    inline bool operator!=(const CHANNEL& rhs) const
    {
        return !(*ch == rhs);
    }

    /// rvalue channel context not comparison
    inline bool operator!=(CHANNEL&& rhs) const
    {
        return *ch != rhs;
    }

    inline bool operator<(const CHANNEL& rhs) const 
    {
        return ch->context() < rhs.context();
    }

    inline bool operator<(CHANNEL&& rhs) const 
    {
        return ch->context() < rhs.context();
    }

    inline bool operator<=(const CHANNEL& rhs) const 
    {
        return ch->context() < rhs.context();
    }

    inline bool operator<=(CHANNEL&& rhs) const 
    {
        return ch->context() <= rhs.context();
    }

    inline bool operator>(const CHANNEL& rhs) const 
    {
        return ch->context() > rhs.context();
    }

    inline bool operator>(CHANNEL&& rhs) const 
    {
        return ch->context() > rhs.context();
    }

    inline bool operator>=(const CHANNEL& rhs) const 
    {
        return ch->context() > rhs.context();
    }

    inline bool operator>=(CHANNEL&& rhs) const 
    {
        return ch->context() >= rhs.context();
    }

private:
    CHANNEL* ch;
};

}

#endif
