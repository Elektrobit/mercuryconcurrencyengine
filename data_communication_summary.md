# Summary of Data Communication Operations 
## A word about concurrency
![mercury_icon](img/mercury_icon_tiny.png)

While all channel objects below can be used with `std::thread` their 
simplest and most efficient usage occurs when used when running in 
a `mce::coroutine` provided by the concurrency features of this library.

See [Concurrency Summary](concurrency_summary.md).

## Table of Contents 
![mercury_icon](img/mercury_icon_tiny.png)

- [shared channel API](#shared-channel-api)
- [sending void](#sending-void)
- [unbuffered channel](#unbuffered-channel)
- [buffered channel](#buffered-channel)
- [chan](#chan)

### shared channel API
The following is taken from `mce/base_channel.hpp`. All objects therein are in 
the `mce` namespace.

The following result enum is provided:
```
// enum for channel operation results
enum result 
{
    // ensure closed is convertable to `false`, allowing while(try_...()) operation loops
    closed = 0, /// channel is closed, operation failed
    success, /// blocking/nonblocking operation succeeded
    failure /// nonblocking operation failed
};
```

All channel objects implement or inherit the following API from `mce::base_channel<T>`:
```
// Constructs the underlying channel context. Must be called or copied from 
// another channel before other operations are called. Copies of channels 
// communicate to one another through their shared context.
virtual void construct() const = 0; 

virtual const std::type_info& type_info() const = 0; // retrieve type_info for the channel
virtual void* context() const= 0; // return the internal context pointer
virtual void close() const= 0; // close the channel
virtual bool closed() const= 0; // return true if the channel is close, else false
virtual bool send(const T& s) const= 0; // send value over the channel
virtual bool send(T&& s) const= 0; // move value over the channel
virtual bool recv(T& r) const; // receive value over the channel
virtual result try_send(const T& s) const= 0; // attempt to send a value over the channel
virtual result try_send(T&& s) const= 0; // attempt to move a value over the channel
virtual result try_recv(T& r) const= 0; // attempt to receive a value over the channel
base_channel<T>::iterator begin() const; // retrieve iterator to next value received on the channel
base_channel<T>::iterator end() const; // retrieve an iterator to the end/closed state of the channel
```

For clarification, the reason all operations and returned references are 
const is because channels are designed to *ignore* const designations. This 
is because channels are designed as a trivially easy mechanism for communication 
and are intended to be copied by capture in lambdas. Unfortunately, unless the 
lambda in question is designated "mutable" the captures will become const by
default, which would make channels useless. 

While this design quirk is certainly an anti-pattern, it exists to combat the 
greater anti pattern of having to mark every user lambda as mutable (which also 
limits the usable forms of lambda notation). So for the sake simplicity and 
readability all derived channels which inherit base_channel will typically need 
to specify its members as mutable.

All channels also inherit the following operator overloads from
`mce::channel_operators<T,CHANNEL>`:
```
/*
 Statically construct a channel with CHANNEL::construct(as...) 

 @params optional context constructor arguments
 @return the constructed channel
 */
template <typename... As>
static CHANNEL make(As&&... as);

virtual void assign(const CHANNEL& rhs) const = 0; // copy context function
virtual void assign(CHANNEL&& rhs) const = 0; // move context function
explicit operator bool() const; // return whether context() is non-NULL
bool operator=(const CHANNEL<T>& rhs) const; // copy context from argument channel
bool operator=(CHANNEL<T>&& rhs) const; // copy context from argument channel
bool operator==(const CHANNEL<T>& rhs) const; // return true if channels share context, else return false
bool operator==(CHANNEL&& rhs) const; // return true if channels share context, else return false
bool operator!=(const CHANNEL<T>& rhs) const; // return false if channels share context, else return true
bool operator!=(CHANNEL<T>&& rhs) const;  // return false if channels share context, else return true
```

Channels function equivalently for both `std::thread` operating system threads AND for `mce::coroutine` coroutines (any function scheduled with `mce::parallel()`, 
`mce::concurrent()`, `mce::threadpool::schedule()`, or `mce::scheduler::schedule()` is converted to and executed as a coroutine). 

That is, any 'blocking' operation is handled correctly in both cases, allowing 
other threads and coroutines to run while the caller is blocked.

The `try_` variant of sends and receives will return a `mce::result::success` if the operation
succeeded, `mce::result::failure` if the operation failed or `mce::result::closed` if the channel is closed. In some cases (like `unbuffered_channel`) if both
the sender and receiver use only `try_` calls, no operation will EVER 
succeed, because internally the `try_` operations require a blocking 
complimentary receiver or sender. Best practice is to avoid using `try_` 
entirely for most operations. However, if `try_` methods must be used communication 
problems can be generally solved by having only one side of a send/receive use 
a `try_` variant, with the other side using a blocking call.

Calling `close()` on the channel causes all code blocked on said channel 
or any future calls to this channel's communication API (other than `make()`,
`close()`, `closed()`, or other non-communication functionality) will cause said 
opereations to fail.

Channel iterators are equal to `base_channel<T>::iterator::end()` once 
the channel is closed. Operator `++` will internally call a blocking 
`recv()`. If the channel is closed during this operation, the iterator will 
become == to `end()`.

Channel iterators are `std::input_iterator`s (and inherit 
`std::input_iterator_tag`), meaning they support the following operators:
```
const iterator& operator=(const iterator& rhs) const;
const iterator& operator=(iterator&& rhs) const;
bool operator==(const iterator& rhs) const;
bool operator==(iterator&& rhs) const;
bool operator!=(const iterator& rhs) const;
bool operator!=(iterator&& rhs) const;
T& operator*() const;
T* operator->() const;
const iterator& operator++() const;
const iterator operator++(int) const;
```

Because channel objects implement iterators, they can be used in range-for 
loops, which will safely exit the loop when the channel is closed:
```
void foo(mce::chan<int> ch)
{
    for(auto& e : ch)
    {
        std::cout << "e: " << e << std::endl;
    }
}
```

### sending void
Channels cannot be trivially templated to `void` because of how their shared API is designed. IE, `send(const T&)` (and other code which references `T`) is invalid in `c++` when applied to type `void`. 

The user who wants channel synchronization but wants to minimize copy costs can use basic types like `int`, `size_t`, or `char` as the templated type. However, a clarity workaround can be made by utilizing empty structs:

[example_028 source](ex/src/example_028.cpp)
```
// example_028
#include <iostream>
#include "mce.hpp"

// only an object pointer will be constructed, but no data will ever be copied
struct void_t { };

void receiver(mce::chan<void_t> ch) {
    void_t r;
    ch.recv(r);
    std::cout << "receiver received" << std::endl;
    ch.send({});
}

int main() {
    auto ch = mce::chan<void_t>::make();
    mce::concurrent(receiver, ch);

    ch.send({});
    void_t r;
    ch.recv(r);
    std::cout << "main received" << std::endl;

    return 0;
}
```

Terminal output:
```
$ ./ex/example_028
receiver received
main received
$
```

### unbuffered channel
Implemented in `mce/unbuffered_channel.hpp`
```
namespace mce {

template <typename T>
class unbuffered_channel : public base_channel<T>, 
                           public channel_operators<T,unbuffered_channel<T>>;

}
```

Templated, synchronized communication mechanism to send data between 
concurrent code. This object *always* blocks on either `send()` or `recv()` 
until there is a complementary `recv()` or `send()`. The channel does not 
allocate storage space for the data being sent, it directly copies or moves the 
data from the sender to the receiver.

unbuffered_channel is special in that its `try_send()`/`try_recv()` operations 
still require a complimentary blocking `recv()`/`send()` or they will *always 
fail*. 

[example_021 source](ex/src/example_021.cpp)
```
// example_021
#include <iostream>
#include "mce.hpp"
int main()
{
    mce::unbuffered_channel<int> ch1;
    mce::unbuffered_channel<int> ch2;
    ch1.construct();
    ch2.construct();
    
    std::thread thd([](mce::unbuffered_channel<int> ch1, 
                   mce::unbuffered_channel<int> ch2)
    {
        int x;
        ch1.recv(x);
        std::cout << x << std::endl;
        ch2.send(0);
    }, ch1, ch2);
    
    ch1.send(14);
    int r;
    ch2.recv(r);
    thd.join();
    return 0;
}
```

Terminal output:
```
$ ./ex/example_021
14
$
```


### buffered channel
Implemented in `mce/buffered_channel.hpp`
```
namespace mce {

template <typename T>
class buffered_channel : public base_channel<T>,
                         public channel_operators<T,buffered_channel<T>>
{
    // specialized or unique API:

    void construct(size_t sz); // allocate buffer of size sz
    void construct(); // default buffer of size 1
    size_t size() const; // return number of values stored in channel
    bool empty() const; // return if there are no values in the channel
    bool full() const; // return if channel buffer is full
    size_t capacity() const; // return maximum number of storable values
    size_t reserve() const; // return count of unused values in the buffer 
}

}
```

Templated communication mechanism to send data between concurrent code, with 
data stored in a statically sized buffer during transit. Only blocks on receive 
if no values are available in the internal buffer, and only blocks on `send()` 
if no room is left in the internal buffer.

`buffered_channel` also provides the following extra API to manage its 
internal buffer:
```
size_t size() const; // returns the count of values stored in the buffer
size_t reserve() const; // return count of unused values in the buffer
size_t capacity() const; // return the maximum number of storable values
bool empty() const; // returns true if the buffer is empty, else false
bool full() const; // returns true if the buffer is full, else false
```

[example_022 source](ex/src/example_022.cpp)
```
// example_022
#include <iostream>
#include "mce.hpp"
int main()
{
    mce::buffered_channel<int> ch1;
    mce::buffered_channel<int> ch2;
    ch1.construct(1); // can hold 1 value before forcing the sender to block
    ch1.construct(); // defaults to buffer size 1
    ch1.construct(0); // defaults to buffer size 1
    ch2.construct(30); // can hold 30 simultaneous values before forcing sender to block
    
    std::thread thd([](mce::buffered_channel<int> ch1, 
                   mce::buffered_channel<int> ch2)
    {
        int x;
        ch1.recv(x);
        std::cout << x << std::endl;
        ch2.send(0);
    }, ch1, ch2);
    
    ch1.send(42);
    int r;
    ch2.recv(r);
    thd.join();
    return 0;
}
```

Terminal output:
```
$ ./ex/example_022
42
$
```

### chan
Implemented in `mce/chan.hpp`, specialized or unique API:
```
namespace mce {
    
template <typename T>
class chan : public base_channel<T>,
             public channel_operators<T,chan<T>>
{
    // specialized or unique API: 

    // mechanism for constructing a chan<T> object using any object which 
    // implements base_channel<T>
    template <typename CHANNEL>
    void construct(CHANNEL&& ch) const;

    void construct() const; // construct unbuffered channel context 

    size_t context_type_info() const; // returns the wrapped channel's std::type_info

    // cast base_channel context to templated channel and return the result. 
    template <typename CHANNEL> CHANNEL cast() const;
};

}

```

Wrapper object representing an unbuffered channel by default. However, using `chan`'s `construct()` call the `chan` can be constructed using *any* object which inherits the `base_channel` API including `unbuffered_channel`, `buffered_channel`, or a user-made channel object.

[example_011 source](ex/src/example_011.cpp)
```
// example_011
#include <iostream>
#include "mce.hpp"
int main()
{
    mce::chan<int> ch1;
    mce::chan<int> ch2;
    ch1.construct(mce::buffered_channel<int>::make(1)); // is buffered
    ch2.construct(); // is unbuffered
    
    std::thread thd([](mce::chan<int> ch1, mce::chan<int> ch2)
    {
        int x;
        ch1.recv(x);
        std::cout << x << std::endl;
        ch2.send(5);
    }, ch1, ch2);
    
    ch1.send(14);
    int r;
    ch2.recv(r);
    std::cout << r << std::endl;
    thd.join();
    return 0;
}
```

Terminal output:
```
$ ./ex/example_011
14
5
$
```

The type information of the wrapped channel object can be returned using
`mce::chan<T>::context_type_info()`. IE, you can check if your `mce::chan<T>` 
represents a `mce::unbuffered_channel<T>` by comparing the return value of 
`mce::chan<T>::context_type_info()` with the `type_info()` returned from either calling 
`unbuffered_channel<T>::type_info()` or directly with the `typeid` operator (`typeid(unbuffered_channel<T>)`).

After determining the exact type wrapped in the `mce::chan<T>` object, you can
retrieve a copy of that channel with `mce::chan<T>::cast<CHANNEL>()`:

```
#include <typeinfo>
#include <mce/unbuffered_channel.hpp>
#include <mce/chan.hpp>

mce::unbuffered_channel<int> getUnbufferedChannel(mce::chan<int> ch)
{
    mce::unbuffered_channel<int> unb_ch;

    if(ch) // check if mce::chan<int> is initialized
    {
        if(ch.context_type_info() == unb_ch.type_info())
        {
            // extract a copy of the unbuffered_channel from chan wrapper
            unb_ch = ch.cast<mce::unbuffered_channel<int>>();
        }
    }
    return unb_ch;
}
```
