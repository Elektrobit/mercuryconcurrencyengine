# High Level Cpp Coroutine Concurrency Summary
## Table of Contents
![mercury_icon](img/mercury_icon_tiny.png)

- [Running code as a concurrent coroutine](#running-code-as-a-concurrent-coroutine)
- [Channels explained](#channels-explained)
- [Simplifying channels with chan](#simplifying-channels-with-chan)
- [Channel iterators](#channel-iterators)
- [Using coroutines and channels to generate asynchronous futures](#using-coroutines-and-channels-to-generate-asynchronous-futures)
- [Running coroutines blocking on other coroutines](#running-coroutines-blocking-on-other-coroutines)
- [Running coroutines with blocking calls](#running-coroutines-with-blocking-calls)
- [Behind the scenes](#behind-the-scenes)


## Running code as a concurrent coroutine 
![mercury_icon](img/mercury_icon_tiny.png)

The functions `mce::concurrent()`, `mce::parallel()` and `mce::balance()` will launch a given function or Callable (and any arguments) as a concurrently executing coroutine. Data can be communicated between coroutines and threads via channels (`mce::chan`, `mce::unbuffered_channel`, `mce::buffered_channel`) and 
their `send()` and `recv()` methods.

A trivial example:

[example_005 source](ex/src/example_005.cpp)
```
// example_005
#include <iostream>
#include <string>
#include "mce.hpp"

void my_function(int arg0, std::string arg1, mce::chan<int> done_ch)
{
    std::cout << "arg0: "
              << arg0
              << "; arg1: "
              << arg1 
              << std::endl;

    done_ch.send(0);
}

int main(int argc, char** argv)
{
    mce::chan<int> done_ch = mce::chan<int>::make();

    // execute functions as a parallel, concurrent coroutine using the parallel() call
    mce::parallel(my_function, 3, "hello world", done_ch);

    // wait for a coroutine to send a value over done_ch before returning
    int r;
    done_ch.recv(r);
    return 0;
}
```

Terminal output:
```
$ ./ex/example_005
arg0: 3; arg1: hello world
```

See [concurrency summary](concurrency_summary.md) for more information

## Channels explained
![mercury_icon](img/mercury_icon_tiny.png)

`mce::unbuffered_channel`s, `mce::buffered_channel`s and are synchronized communication mechanisms for sending 
values between places in code (running on any combination of normal threads or 
coroutines).

All channels must have their `construct()` function called OR assigned their 
internals from another channel before they can function (a new channel with constructed context can be generated with a call to a channel's static `make()` procedure). This is because 
channels are merely *interfaces* to shared data internals. 

`mce::unbuffered_channel`s communicate data and force both the sender and 
recipient to block until both are ready. This means that the sender or 
receiver will always block if there is no complementary blocking receiver or 
sender.

`mce::buffered_channel`s communicate data asynchronously. Send operations 
only block when no room remains in the internal data container. Receive 
operations only block when no values remain unclaimed in the internal data 
container.

Both variants use `send()` operations to send data into the channel
and `recv()` operations to retrieve data from the channel.

*Warning: It is best to make copies of channels, so the shared internals do not unexpectedly go out of scope.*


[example_006 source](ex/src/example_006.cpp)
```
// example_006
#include <iostream>
#include "mce.hpp"

void func(mce::buffered_channel<std::string> out_ch)
{
    out_ch.send("hello");
}

void func2(mce::buffered_channel<std::string> in_ch, mce::unbuffered_channel<int> done_ch)
{
    std::string s;
    in_ch.send(s);
    std::cout << "func said " << s << std::endl;
    done_ch.send(0);
}

int main(int argc, char** argv)
{
    // has an internal container that can hold 3 simultaneous values
    auto ch = mce::buffered_channel<std::string>::make(3);
    
    // has no internal container for values
    auto done_ch = mce::unbuffered_channel<int>::make(); 

    mce::parallel(func, ch);
    mce::parallel(func2, ch, done_ch);

    int r;
    done_ch.recv(r);
    return 0;
}
```

Terminal output:
```
$ ./ex/example_006
func said hello
```

As a note, all channels support the function `void close()`, which will cause 
all current and future communication operations (such as `send()` and `recv()`) 
to fail. It will also unblock coroutines and threads using the channel.

`mce::unbuffered_channel`s and mce::`buffered_channel`s also have other functions such as 
`try_recv()` in addition to their standard methods. See 
[data communication summary](data_communication_summary.md) for more information.


## Simplifying channels with chan
![mercury_icon](img/mercury_icon_tiny.png)

The `mce::chan` object is a special wrapper object which can represent any channel which implements `mce:base_channel`, such as an `mce::unbuffered_channel` and `mce::buffered_channel`. Because of this, it only directly gives access to API provided by (or implementable with said API) `mce::base_channel`. 

If you don't want subsequent functions to bother about what kind of channel
they are using or simply want more readable code use `mce::chan`.

[example_007 source](ex/src/example_007.cpp)
```
// example_007
#include <iostream>
#include <string>
#include "mce.hpp"

int main()
{
    // is a buffered channel
    mce::chan<std::string> buf_ch = mce::chan<std::string>::make(mce::buffered_channel<std::string>::make(3));
    
    // no channel provided, defaults to unbuffered_channel
    mce::chan<std::string> unbuf_ch = mce::chan<std::string>::make(); 
    
    mce::chan<int> done_ch = mce::chan<int>::make();
    
    auto my_function = [](mce::chan<std::string> ch1, 
                          mce::chan<std::string> ch2, 
			  mce::chan<int> done_ch)
    {
        std::string r1;
        std::string r2;
        ch1.recv(r1);
        ch2.recv(r2);
        std::cout << "ch1: " << r1 << "; ch2: " << r2 << std::endl;
        done_ch.send(0);
    };
    
    mce::parallel(my_function, buf_ch, unbuf_ch, done_ch);
    
    buf_ch.send("hello");
    unbuf_ch.send("world");
    int r;
    done_ch.recv(r);
    
    return 0;
}
```

Terminal output:
```
$ ./ex/example_007
ch1: hello; ch2: world
```

## Channel iterators 
![mercury_icon](img/mercury_icon_tiny.png)

All channels provide iterator support with `begin()` and `end()`, allowing usage 
in range-for loops. This behavior is very useful because channel iterators are == 
`end()` when the channel is closed, meaning you don't have to watch for 
operations to begin failing.

Iterators generated from parent type `base_channel<TYPE>` are 
`base_channel<TYPE>::iterator`. This pattern follows for the other channel types as 
well (IE `unbuffered_channel<TYPE>` has `unbuffered_channel<TYPE>::iterator`).

[example_008 source](ex/src/example_008.cpp)
```
// example_008
#include <iostream>
#include <string>
#include "mce.hpp"

using namespace mce;

int main()
{
    mce::chan<std::string> ch = mce::chan<std::string>::make();
    mce::chan<bool> done_ch = mce::chan<bool>::make();
    
    auto sender = [=]
    {
        for(size_t cnt = 0; cnt<10; ++cnt)
        {
            ch.send(std::string("hello") + std::to_string(cnt));
        }
        ch.close();
    };
    
    auto receiver = [=]
    {
        for(auto& s : ch)
        {
            std::cout << s << std::endl;
        }
        done_ch.send(true);
    };
    
    mce::parallel(sender);
    mce::parallel(receiver);
    bool r;
    done_ch.recv(r);
    return 0;
}
```

Should print:
```
$ ./ex
hello0
hello1
hello2
hello3
hello4
hello5
hello6
hello7
hello8
hello9
$
```


## Using coroutines and channels to generate asynchronous futures
![mercury_icon](img/mercury_icon_tiny.png)

Combining the `mce::concurrent` function with channels allows trivial implementation of futures (where the channel is analogous to a `std::future` object provided when `std::async()` is called):

[example_010 source](ex/src/example_010.cpp)
```
// example_010
#include <iostream>
#include "mce.hpp"

const int the_number = 3;

int main()
{
    // this channel behaves like an std::future
    mce::chan<int> ret_ch = mce::chan<int>::make(); 
    
    mce::parallel([ret_ch]{ ret_ch.send(the_number); });
   
    int r;
    ret_ch.recv(r);
    std::cout << "The number: " << r << std::endl;
    return 0;
}
```

Terminal output:
```
$ ./ex/example_010
The number: 3
```

## Running coroutines blocking on other coroutines 
![mercury_icon](img/mercury_icon_tiny.png)

Coroutines that detect they are blocked will suspend execution by communicating 
their blocked state and yielding to their calling context. Normally the calling 
context within this framework is a `mce::scheduler` object which is responsible 
for efficiently executing the next coroutine. 

Unit testing shows that coroutine yield context switching is significantly faster 
than OS driven condition synchronization. Assuming that useful work is being done 
in at least one scheduler the overhead cost for this operation is minimal.

This example can be run alongside a process observation task (like taskmanager 
on windows or top on linux) to see that the process is not using cpu:
```
// human_only_example_001
#include <iostream>
#include <string>
#include "mce.hpp"

int main(int argc, char** argv)
{
    mce::chan<int> test_ch = mce::chan<int>::make();

    std::string inp;

    // CPU usage should be 0 because we are using OS level blocking when no 
    // tasks are available
    std::cout << "wait for user input: ";
    std::cin >> inp;

    // spawn task that will block forever
    mce::parallel([&]{ int x; test_ch.recv(x);});

    std::cout << "wait for user input2: ";
    std::cin >> inp;

    return 0;
}
```


## Running coroutines with blocking calls
![mercury_icon](img/mercury_icon_tiny.png)

A consequence of true blocking behavior is that by definition it blocks the 
entire thread. In the case of a thread running multiple coroutines this will 
therefore block all other coroutines from running, causing extreme delay (or 
deadlock if any coroutines require interaction from another coroutine on 
the blocked thread). 

This is a common pain point for tasks that need to do blocking operations, 
that is, to interact with the operating system or other processes in a way 
that causes the caller to block for an indefinite amount of time. 

Instead, `await()` blocks a coroutine (in a way that allows other coroutines to
execute in the meantime) and to execute the function passed to `await()` on 
another (non-threadpool) thread running a scheduler without blocking other 
coroutines.

Here is a working example:

[example_012 source](ex/src/example_012.cpp)
```
// example_012
#include <thread>
#include <iostream>
#include "mce.hpp"

void my_function(mce::buffered_channel<int> done_ch)
{
    mce::thunk t = []
    {
        std::cout << "executing on thread " 
                  << std::this_thread::get_id() 
                  << std::endl;
    };

    t();

    // launch a new thread, block the current coroutine or thread, execute t 
    // on the new thread, then unblock the calling thread with the result
    mce::await(t);

    t();

    done_ch.send(0);
}

int main(int argc, char** argv)
{
    mce::buffered_channel<int> done_ch;
    done_ch.construct();

    // execute functions as coroutines with the concurrent() call
    mce::parallel(my_function, done_ch);

    // wait for a coroutine to send a value over done_ch before returning
    int x; 
    done_ch.recv(x);
    return 0;
}
```

Terminal output:
``` 
$ ./ex/example_012
executing on thread 139945936029440
executing on thread 139945902323456
executing on thread 139945936029440
```

Any function or lambda passed to `await()` can reference local variables safely.
This is because local variables exist in a context which is guaranteed to be 
blocked while the function passed to `await()` is running.

[example_001 source](ex/src/example_001.cpp)
```
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <mce/mce.hpp>

void read_file_content(std::string fname, mce::chan<int> done_ch) {
    std::string fileContent;

    // create a function to execute in mce::await() that accesses the caller's stack 
    auto read_file = [&] {
        std::ifstream file(fname); // open file

        if(file.is_open()) { 
            std::ostringstream ss;
            ss << file.rdbuf(); // extract file contents
            fileContent = ss.str();
            return true; // await() will return what the input function returns
        } else { 
            return false; 
        }
    };

    // wait for boolean return value of read_file function
    if(mce::await(read_file)) { 
        std::cout << "file successfully read" << std::endl; 
    } else { 
        std::cout << "failed to read file" << std::endl; 
    }

    std::cout << "fileContent: " << fileContent << std::endl;
    done_ch.send(0);
}

int main(int argc, char** argv) {
    // this is test setup, just ensuring there is a file to read
    std::string fname("my_filename.txt");
    std::ofstream file(fname, std::ios_base::trunc);

    if(file) {
        file << "hello world!";
    }

    file.close();

    // launch asynchronous coroutine to read the file content 
    auto done_ch = mce::chan<int>::make(); 
    mce::parallel(read_file_content, fname, done_ch);

    // wait for coroutine to finish before the program exits
    int r;
    done_ch.recv(r);
    return 0;
}
```

Terminal output:
```
$ ./ex/example_001
file successfully read
fileContent: hello world!
$
```

## Using timers
The `mce` library provides is an object called `mce::timer_service` in `timer.hpp` (see "timer service" in [Summary of Concurrent Operations](concurrency_summary.md) for more information on this object), which can scheduler timers. This object requires its own system thread to run on. Unless disabled by compiler define MCE_DISABLE_DEFAULT_TIMER_THREADS, this frameworks provides a default `mce::timer_service` running on a background worker thread  accessible via function `std::shared_pointer<mce::timer_service> mce::default_timer_service()`.

It should be noted that the toplevel `mce::timer()` operation launch timers whose timeouts occur *as a coroutine* on the same `mce::scheduler` of the system thread the `mce::timer()` was called on. This is behavior causes timeouts to be "asynchronous" but they execute on the same thread that launched them, instead of on a dedicated timer_service thread.

To alert coroutines on timeout with a `mce::timer_service` the user can use channels:
```
#include <mce/scheduler.hpp>
#include <mce/io.hpp>
#include <mce/timer.hpp>

my_value my_coroutine() {
    auto ch = mce::chan<int>::make();
    mce::default_timer_service()->timer(mce::time_unit::millisecond, 500, [=]{ ch.send(0);});
    int done;
    ch.recv(done); // block until timeout

    //... continue operations
}
```

The above business logic is similar to how `mce::sleep()` calls operate.

## Behind the scenes
![mercury_icon](img/mercury_icon_tiny.png)

### A note on reference bindings
Throughout this library various functions will take other functions and 
an optional set of arguments which are internally bound together using 
`std::bind()`. Example functions which have this behavior:
```
template <typename F, typename... A> void mce::concurrent(F&&, A&&...);
template <typename F, typename... A> void mce::await(F&&, A&&...);
template <typename F, typename... As> mce::timer_id mce::timer(mce::time_unit u, std::uint64_t count, F&& f, As&&...);
template <typename F, typename... As> mce::timer_id mce::timer(mce::duration d, F&& f, As&&...);
template <typename F, typename... As> mce::timer_id mce::timer(mce::time_point timeout, F&& f, As&&...);
```

When this happens the *ONLY* way to pass an argument by reference is to 
use `std::ref` or `std::cref`. This limitation is imposed by c++ `std::bind`.

[example_003 source](ex/src/example_003.cpp)
```
// example_003
#include <iostream>
#include "mce.hpp"

void modify_reference(int& i, int val, mce::chan<bool> done_ch)
{ 
    i = val; 
    done_ch.send(true);
}

int main()
{
    int i=0;
    auto done_ch = mce::chan<bool>::make();
    mce::parallel(modify_reference, std::ref(i), 2, done_ch);
    // ensure modify_reference is complete before reading i
    bool r;
    done_ch.recv(r);
    std::cout << "i: " << i << std::endl;
    return 0;
}
```

Terminal output:
```
$ ./ex/example_003
i: 2
```

An easier solution is to use lambda reference captures:

[example_004 source](ex/src/example_004.cpp)
```
// example_004
#include <iostream>
#include "mce.hpp"

int main()
{
    int i=0;
    auto done_ch = mce::chan<bool>::make();
    auto modify_reference = [=,&i](int val){ i = val; done_ch.send(true); };
    mce::parallel(modify_reference, 2);
    bool r;
    done_ch.recv(r);
    std::cout << "i: " << i << std::endl;
    return 0;
}
```

Terminal output:
``` 
$ ./ex/example_004
i: 2
```

However, it should be noted that modifying by reference on different threads without synchronization is often an error and can cause the program to crash. This can be avoided by coroutines running on the *same* thread, but it is generally better to avoid the situation entirely and communicate instead through channels.

### Thunks!
This project makes extensive use of thunks, also known as nullary lambdas. 
Lambdas are anonymous functions, which is a complicated way of saying they are 
functions we can define and manage *inside other functions*. A thunk is a function 
which takes no arguments and returns no value. While this sounds useless, lambdas can
"capture context", allowing them to take references or make copies of data available 
in the current scope while they are being constructed:

[example_013 source](ex/src/example_013.cpp)
```
// example_013
#include <functional>
#include <iostream>

int main(int argc, char** arv)
{
    int c=0;
    std::function<void()> my_thunk = [&]{ ++c; };

    std::cout << "c: " << c << std::endl; // prints 0
    my_thunk();
    std::cout << "c: " << c << std::endl; // prints 1
    return 0;
}
```

Terminal output:
```
$ ./ex/example_013
c: 0
c: 1
```

Using the ability to capture context we can do some incredible things. As a
usability improvement mce provides some helper types and functions in 
"thunk.hpp" should the user desire to implement features similar to this 
library. 

```
#include <mce/thunk.hpp>
#include <iostream>

void some_func()
{
    mce::thunk my_thunk = [&]{ ... };

    // bind an arbitrary function to some arguments
    mce::thunk my_thunk2 = mce::make_thunk(some_function, arg0, arg1, ..., argn);

    my_thunk();
    my_thunk2();
}
```



### Segmented Coroutine Stacks 
At the moment, coroutines are created with whatever default stack allocator the
boost coroutine2 library provides (which itself is determined by what is 
available from boost context). A more optimal solution would probably be using 
"segmented_stack" allocators. However, segmented_stack objects require that 
boost context be compiled with specific flags, which are not guaranteed to be 
set. 

A potential enhancement might be the ability to initialize threadpools to a 
specific allocator type, allowing the default_threadpool to be enhanced as 
available. Of note, I believe the default allocator *is* a segmented stack 
allocator when it is available, so the current implementation would use that 
by default when compiled with the boost context library with segmented stacks 
enabled.

As it stands, the default behavior for coroutines launched via concurrent() potentially take more memory than necessary.
