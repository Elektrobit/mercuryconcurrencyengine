# Summary of Concurrent Operations 
## A word about channels
![mercury_icon](img/mercury_icon_tiny.png)

All `mce` channel objects correctly communicate data between coroutines, threads, or any combination of the two in a threadsafe/coroutinesafe way. These objects are HIGHLY RECOMMENDED as the primary method of communication between concurrent code, because they are extremely simple in practice and cover most communication usecases. See [Data Communication Summary](data_communication_summary.md). More examples of channel usage are available throughout the documentation, including below in this file.

If an edgecase occurs where mutexes & condition variables would be the preferred solution, see `mutex` and `condition_variable` in the `united
synchronization primatives` section of `High Level Concurrency`.

Note that all channels are automatically included as part of `mce/mce.hpp`.

## Concurrency Table of Contents
![mercury_icon](img/mercury_icon_tiny.png)

All concurrent code is included with the header `mce/mce.hpp`
- [Concurrency Design](#concurrency-design)
- [High Level Concurrency](#high-level-concurrency)
- [Low Level Concurrency](#low-level-concurrency)
- [Concurrent Lifecycle Management](#concurrent-lifecycle-management)

---
## Concurrency Design
![mercury_icon](img/mercury_icon_tiny.png)

It should be noted that concurrency and scheduling algorithms are always an imperfect art form. There is no "one scheduling algorithm to rule them all", because the proper scheduling algorithm is determined by the actual design and needs of a program. For instance, most of the time the best scheduling algorithm... is to not schedule anything (just execute right there)! However, where this is not possible or preferrable, there are a there are a few categories of thinking that can help when programming a general "best effort" program which uses concurrency.

This first is to identify whether a piece of code needs concurrency:
- for CPU throughput 
- or for asynchronous behavior generally 

In the first case, a simple way to break a program down is to identify the "parts" of the program which need to do categorically different things. That is, if you can entirely separate pieces of the program which can act independently (for instance, into separate service objects), then those separate components are good candidates for being scheduled in parallel (with `mce::parallel()`, `mce::threadpool::schedule()`, or `mce::default_threadpool()->schedule()`).

However, it should be noted that just because something can be parallelized, doesn't mean benefits will outweigh the negatives. It is easy to seriously underestimate how efficient coroutine scheduling is. As such, it may be best to schedule with `mce::concurrent()` instead of `mce::parallel()` until testing reveals the need for parallelization.

In the second case, you can further break down code's concurrency needs into:
- wants fast communication speed between code which cannot operate synchronously
- the operation is blocking and needs to be asynchronously resumed

If communication speed is the need, child tasks spawned from parent tasks that where themselves scheduled in parallel (on different threads) can often (but not *always*), be trivially scheduled with `mce::concurrent()`, which will give the fastest communication speed. Assuming the program is operating in an environment where it's resources are not being pushed to its limit (hopefully most of the time :-) ) then this will generally give you what you want.

It should be noted, that a program which *only* schedules with `mce::concurrent()` will often be good enough, and probably exceptionally performant. Because of this, mass usage of `mce::concurrent()` is recommended for most programs, as the resulting performance should be quite good. If some operation is noted to be especially CPU intensive, it can always be explicitly scheduled with `mce::parallel()`.

Alternatively, if some operation is blocking or otherwise asynchronous, alternative concurrent solutions become useful:
- `mce::await()`: block the caller until the launched task completes
- `mce::timer()`: block the caller until a timeout occurs and execute a callback
- `mce::sleep()`: block the caller until a timeout occurs

There are many edgecases where the above is insufficient, but I believe they are good guidelines for writing code that behaves well in most circumstances.

## High Level Concurrency
![mercury_icon](img/mercury_icon_tiny.png)

- [concurrent](#concurrent)
- [parallel](#parallel)
- [balance](#balance)
- [custom scheduling](#custom-scheduling)
- [yield](#yield)
- [await](#await)
- [timer](#timer)
- [sleep](#sleep)
- [united synchronization primatives](#united-synchronization-primatives)

---
### schedule 
`mce::scheduler::schedule()` is also addressed in the `mce::scheduler` section. However, because of its importance to higher level API, it is examined first here:
```
namespace mce {

struct scheduler {
    /**
     @brief Schedule allocated coroutines

     Arguments to this function can be:
     - an allocated `std::unique_ptr<mce::coroutine>`
     - an iterable container of allocated `std::unique_ptr<mce::coroutine>`s

     After the above argument types, any remaining arguments can be:
     - a Callable (followed by any arguments for the Callable) 

     The user can manually construct allocated 
     `std::unique_ptr<mce::coroutine>`s with `mce::coroutine::make()`

     Multi-argument schedule()s hold the scheduler's lock throughout (they are 
     simultaneously scheduled).

     @param a the first argument
     @param as any remaining arguments
     */
    template <typename A, typename... As>
    void schedule(A&& a, As&&... as);
};

}
```

`mce::scheduler::schedule()` is an extremely flexible templated mechanism. Higher level operations pass their arguments directly to it, so all features and behavior are inherited by those mechanisms (`mce::threadpool::schedule()`, `mce::default_threadpool()->schedule()`, `mce::concurrent()`, `mce::parallel()`, `mce::balance()`).

---
### concurrent
```
/// Schedule a coroutine(s), preferring best communication latency
template <typename... As>
void mce::concurrent(As&&... args)
```
`mce::concurrent()`  passes it's arguments to `mce::scheduler::schedule()` as-is. Executes its arguments as a concurrent coroutine(s). It attempts to intelligently schedule coroutines using the following algorithm:
- the `mce::scheduler` for the current thread (if running inside a `mce::scheduler`), or 
- a default `mce::scheduler` (running in the `mce::default_threadpool()`) as a final fallback.

This is useful shorthand for writing a concurrent algorithm which gains more potential communication efficiency from faster context switching than from optimal task distribution across available CPU cores.

That is, if the following are true:
1. `mce::concurrent()` is being called from within a coroutine
2. the function being scheduled as a coroutine will need to communicate quickly or frequently with other coroutines scheduled on the current thread

Then `mce::concurrent()` may provide better performance than `mce::parallel()` because the arguments will attempt to be scheduled on the current thread, as opposed to potentially scheduling on and synchronizing with a *different* thread, which can add significant overhead when the coroutines running on different threads need to communicate.

---
### parallel 
```
/// Schedule a coroutine(s), preferring max CPU throughput
template <typename... As>
void mce::parallel(As&&... args);
```
`mce::parallel()` passes it's arguments to `mce::scheduler::schedule()` as-is. Executes its arguments as a concurrent coroutine(s). It attempts to intelligently schedule coroutines using the following algorithm:
- on the `mce::threadpool` assigned to the calling thread (in the case that the code is already executing in a `mce::threadpool`), or 
- the `mce::scheduler` for the current thread (if running inside a `mce::scheduler`), or 
- the default `mce:::threadpool` as a final fallback (acquired by calling `mce::default_threadpool()`, see low level concurrency for more information). 

`mce::parallel()` is useful when it is known that operations should be parallelized to a high degree. The most common case this will occur is if there is a highly CPU intensive operation(s) that need to be spawned sometime after program startup. 

NOTE: Even when properly distributed across CPU cores, CPU intensive operations which run for a long time will still block their system thread. If long running CPU intensive operations are causing problems, insert calls to `mce::yield()` to suspend them regularly and let other code run.

[example_014 source](ex/src/example_014.cpp)
```
// example_014 
#include <iostream>
#include "mce.hpp"

void print_threadpool_address()
{
    // just for synchronizing prints
    static mce::mutex mtx;
    std::unique_lock<mce::mutex> lk(mtx);
    std::cout << "threadpool address[" 
              << &(mce::this_threadpool())
              << "]"
              << std::endl;
};

void some_function(mce::chan<int> done_ch)
{
    // should print tp address
    print_threadpool_address();
    done_ch.send(0);
}

int main()
{
    mce::chan<int> done_ch = mce::chan<int>::make();
    auto tp = mce::threadpool::make();

    std::cout << "tp address[" << tp.get() << "]" << std::endl;
    std::cout << "default threadpool address[" 
              << &(mce::default_threadpool())
              << "]" 
              << std::endl;
    
    // executes on the current threadpool
    tp->schedule(some_function, done_ch);
    tp->schedule([=]
    {   
        // executes on the current threadpool, not the default
        mce::parallel(some_function, done_ch);
    });
    
    // execute on the default threadpool, not our custom threadpool
    mce::parallel(some_function, done_ch);

    int r;
    done_ch.recv(r);
    done_ch.recv(r);
    done_ch.recv(r);
    tp->halt();
    return 0;
}
```

Terminal output:
```
$ ./ex/example_014
tp address[0x5572d91e4440]
default threadpool address[0x5572d91e7760]
threadpool address[0x5572d91e7760]
threadpool address[0x5572d91e4440]
threadpool address[0x5572d91e4440]
```

---
### balance 
```
/// Schedule a coroutine(s), balancing CPU and communication needs
template <double RATIO_LIMIT=2.0, typename F, typename... As>
void balance(As&&... args);

/*
 Return the balance ratio, set by compiler define: mceBALANCERATIO.

 This value can be set in the toplevel CMakeLists.txt
 */
double balance_ratio();
```

`mce::balance` passes it's arguments to `mce::scheduler::schedule()` as-is. Executes its arguments as a concurrent coroutine(s). It attempts to intelligently schedule coroutines using the following algorithm:
- If not executing on a threadpool, schedules with `mce::default_threadpool()`
- If executing on a threadpool, checks if the threadpool has a workload imbalance greater than `mce::balance_ratio()` (busiest_worker_load / least_busy_worker_load >= `mce::balance_ratio()`)
- If the workload ratio is found to be greater than the `mce::balance_ratio()`, the function is scheduled on the least busy worker 
- Otherwise the function is scheduled on the current thread's `mce::scheduler`

This scheduling operation is the slowest of the high level algorithms. However, this slowness only occurs when `mce::balance()` is called; once a coroutine is scheduled it operates at the same speed as other coroutines. As long as the primary bottleneck is not *launching* new coroutines, usage of this scheduling algorithm should not be a problem.

The operating theory of this algorithm is that if rebalancing occurs only when it is truly necessary, then the newly scheduled coroutines will themselves schedule additional operations on their current thread (instead of the thread of their parent coroutine), allowing the workload to balance naturally. Manual rebalancing will recur until the scheduled coroutines are naturally scheduling additional coroutines on their current threads in roughly equal amounts. 

`mce::balance()` may provide better performance than `mce::concurrent()` or `mce::parallel()` when both evenly distributed CPU usage and best-case communication latency are valued. An example of this may be in a program which runs for a long time, and may need to occasionally rebalance scheduling loads when scheduling new coroutines. If the developer determines this to be the case after performance analysis, they can generally replace usages of `mce::concurrent()` in their code with `mce::balance()`, and further tweak their code if performance becomes an issue.

---
### custom scheduling
The user can implement their own scheduling algorithm on a `mce::threadpool` by calling `mce::threadpool::workers()` which returns a `std::vector<std::shared_ptr<mce::scheduler>>` of schedulers running on worker threads managed by the `mce::threadpool` or by calling `mce::scheduler& mce::threadpool::worker(size_t)` with a specific index to retrieve a given worker's scheduler. The user can then choose when and how to call `mce::scheduler::schedule()` to launch new coroutines.

A *potential* usecase for this is to guarantee that certain high level, root operations execute on different threads, such as launching various services during program startup. The scheduling algorithm `ccc::parallel()` generally attempts to accomplish this. However, `ccc::parallel()` do not have any way to distinguish between tasks passed to it, and may sometimes require user intervention to handle edgecases.

For instance, if a program's `main()` is launching distinct services at startup which need to be evenly distributed across threads, and those services *themselves* schedule on their associated `mce::threadpool()` (the `mce::default_threadpool()`), the `mce::threadpool()` may accidentally schedule two root services on the same thread because `main()` is in competition with a running coroutine for access to `mce::threadpool::schedule()`. 

Instead, by directly utilizing the vector of `mce::scheduler`s returned from a call to `mce::threadpool::workers()`, the developer can ensure each coroutine is scheduled on the correct thread.

---
### yield 
`void mce::yield()`

Calls `mce::co::yield()` if called from a running coroutine, otherwise nothing occurs. Calling this will allow other coroutines to run on the current thread because it will pause execution of the calling concurrent context.

Most usage of this operation will be internal to this framework. However, a user may sometimes need to call this operation directly, because it can be used to temporarily interrupt *long* running calls that are executing many operations continuously. Yielding out of such calls periodically will allow other coroutines to run. 

Another example where explicitly calling `yield()` is useful is when implementing non-blocking calls, where a coroutine can `yield()` as soon as an operation fails before trying again (In fact, it is often best to `yield()` after any non-blocking call, whether it succeeds or not to ensure the coroutine eventually relinquishes control to another coroutine).

---
### await
```
namespace mce {

// configurable value to specify the minimum number of threads that are kept
// perpetually in the await workerpool, it can be set in CMakeLists.txt. Increasing
// this value can minimize latency when many calls to mce::await() are being made.
#define MCEMINAWAITPROCS

/*
 @brief safely block caller while function executes and return the result 
 */
template <typename F, typename... As> 
F_return_type await(F&& function, A&&... args);

/*
 @return true if executing on an await() managed thread, else false
 */
bool is_await();

/*
 @return the count of active await worker threads
 */
size_t await_count();

}
```
`mce::await()` allows coroutines running on a `mce::scheduler` to execute operating system blocking calls on a separate, dedicated thread, then resume operations back to the original thread and/or coroutine and seamlessly return the encapsulated function's return value. 

This behavior allows other coroutines to execute while `mce::await()` is blocked, providing a simple mechanism to fix complex performance problems.

`mce::await()`'s behavior is slightly different depending on if it is called within a coroutine running in a `mce::scheduler` or not. In the first case, the special behavior of executing the argument operation on a managed worker thread will occur, and the operation will be executed on a `mce::scheduler` running on that thread. In the second case, the procedure passed to `mce::await()` will be invoked immediately, on the calling thread. This is because standard threads are designed to handle blocking operations correctly. 

It should be noted, that code executing inside a call to `mce::await()` will return the `std::shared_ptr<mce::scheduler>` associated with the *caller* of `mce::await()` when `mce::in_scheduler()`, `mce::this_scheduler()`, or `mce::this_scheduler_ref()` are invoked. Similarly, `mce::this_threadpool()` will return the `mce::shared_ptr<mce::threadpool>` associated with the calling thread. This protects the user by allowing scheduling operations (such as `mce::concurrent()` and `mce::parallel()`) to operate as if they were being called in the coroutine's original environment.

If `mce::await()` is executed outside of a coroutine (`mce::coroutine`) then `mce::await()` will call its arguments on the current thread instead of executing on a worker thread. Similarly, a call to `mce::await()` within a task executing on an await worker thread will execute immediately. Otherwise `mce::await()` will attempt to execute on a thread in a pre-cached pool of worker threads. 

The minimum count of background await worker threads is specified when compiling this library with compiler define `MCEMINAWAITPROCS`. If said define is not provided, it defaults to 1. 

If no await worker thread is available a new, temporary worker thread will be created to execute the call.

If the argument function's return type is `void` then `mce::await()` will return an `int` value of `0`.

[example_001 source](ex/src/example_001.cpp)
```
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include "mce.hpp"

void read_file_content(std::string fname, mce::chan<int> done_ch) {
    std::string fileContent;

    // create a function to execute in mce::await(). This function can *safely* 
    // access values on the coroutine's stack because the calling coroutine will
    // be blocked while mce::await() is running.
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

    // If called from within a coroutine mce::await() will execute a function 
    // (with any function arguments) on a separate std::thread (blocking the 
    // caller of await() in a coroutine-safe way), but the return value will be 
    // provided to the caller of mce::await() when it returns.
    //
    // If mce::await() is instead executed on a normal thread, the function will be 
    // executed on that calling thread immediately. To the user's code, both 
    // cases will feel similar because they provide the same execution 
    // guarantees, blocking the caller while mce::await() is running.
    //
    // IE, mce::await() is primarily useful in a coroutine context to facilitate 
    // simple await() blocking, but in practice you can use it everwhere and the 
    // code should function similarly. 
    
    if(mce::await(read_file)) { // wait for boolean return value of read_file function
        std::cout << "file successfully read" << std::endl; 
    } else { 
        std::cout << "failed to read file" << std::endl; 
    }

    // print the file content 
    std::cout << "fileContent: " << fileContent << std::endl;
    done_ch.send(0);
}

int main(int argc, char** argv) {
    std::string fname("my_filename.txt");
    std::ofstream file(fname, std::ios_base::trunc);

    // ensure there is a file to read
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


---
### timer 
```
// Launch a timer which will execute the timeout_handler at timeout 
template <typename F, typename... As>
mce::timer_id mce::timer(mce::time_unit u, std::uint64_t count, F&& function, As&&...);

template <typename F, typename... As>
mce::timer_id mce::timer(mce::duration d, F&& function, As&&...);

template <typename F, typename... As>
mce::timer_id mce::timer(mce::time_point timeout, F&& function, As&&...);

// remove any running timer with a given timer_id, return true if successful, else return false
bool mce::remove_timer(mce::timer_id id); 

// return a count of running timers
size_t mce::count_timers(); 
```
These timer calls execute their callbacks on a threadpool with `mce::concurrent()`, protecting the `default_timer_service()` thread from blocking on user callbacks.

`mce::timer()` calls accept any function `F` and any number of arguments
`As...`. Given arguments `As...` will be bound to `F` and called together 
when the timer times out.

`mce::time_unit` is an enumeration defined thus:
```
enum time_unit 
{
    hour,
    minute,
    second,
    millisecond,
    microsecond,
    nanosecond
};
```

And the `mce::duration` and `mce::time_point` types are defined thus:
```
typedef std::chrono::steady_clock::time_point time_point; 
typedef std::chrono::steady_clock::duration duration;
```

Utility functions exist that can help create/manipulate these values:
```
// return the current time as a mce::time_point
mce::time_point mce::current_time(); 

// return a mce::duration represent count instances of the time_unit
mce::duration mce::get_duration(mce::time_unit u, size_t count); 

// returns difference (p0-p1) as the given time_unit
size_t mce::get_difference(mce::time_unit u, mce::time_point p0, mce::time_point p1); 
```


---
### sleep
```
void mce::sleep(mce::time_unit u, std::uint64_t count);
void mce::sleep(mce::duration d);
```
These function for both coroutines and threads. They implement blocking sleep 
behavior that allows other coroutines/threads to run.


---
### united synchronization primatives
```
class mce::mutex
class mce::unitex_condition_variable
```
`mce::mutex` and `mce::condition_variable` function with nearly identical API to c++11 `std::mutex` and `std::condition_variable` (with the exception that it accepts an `std::unique_lock<mce::mutex>` instead of `std::unique_lock<std::mutex>` and `mce::condition_variable` uses `std::chrono::steady_clock` specifically). 

The behavioral difference is that these objects block and synchronize correctly with any mixture of operating system threads and `mce::coroutine`s.

These objects are useful when integrating this library into existing codebases. The user can replace usage of `std::` versions with these `mce::` versions and launch their code with `mce::parallel()`/`mce::concurrent()` instead of `std::thread()`/`pthread` in order to improve program efficiency by leveraging coroutine context switching. 

In general, atomic operations implemented with these primitives will be slower than channels. This is because channels directly use `mce::spinlock`s (instead of mutexes) and `mce::scheduler::parkable` (instead of conditions) and only block the caller when truly necessary. In comparison, `mce::mutex` and `mce::condition_variable` operations may park the caller when spinlocking would be ideal. Additionally, the united primitives have extra features which add overhead to an algorithm which uses them rather than the simpler types. 

However, it should be noted that unit testing shows that usage of these primitives to implement some concurrency-safe message queue may be *sometimes* preferable to standard channels. Specifically in situations when *many* system threads (not coroutines!) are attempting to access some concurrency-safe API then usage of `mce::mutex` provides better performance because it blocks the caller with a `mce::scheduler::parkable` when `mce::mutex::lock()` cannot immediately acquire the mutex, reducing lock contention.

As always, prefer measurement and profiling of program behavior when deciding if such an optimization is necessary.

[example_018 source](ex/src/example_018.cpp)
```
// example_018
#include <iostream>
#include <mutex> // for std::unique_lock
#include "mce.hpp"

int main()
{
    mce::mutex mtx;
    mce::condition_variable cv;
    bool flag = false;
    int i = 0;
    
    std::unique_lock<mce::mutex> lk(mtx);
    
    std::thread t([&]
    {
        {
            std::unique_lock<mce::mutex> lk(mtx);
            flag = true;
            i = 1;
        }
        cv.notify_one();
    });
    
    std::cout << "i: " << i << std::endl;
    
    while(!flag){ cv.wait(lk); };
    
    std::cout << "i: " << i << std::endl;
    
    t.join();
    return 0;
}
```

Terminal output:
```
$ ./ex/example_018
i: 0
i: 1
```

## Low Level Concurrency
![mercury_icon](img/mercury_icon_tiny.png)

- [coroutine](#coroutine)
- [spinlock](#spinlock)
- [scheduler](#scheduler) 
- [parkable](#parkable) 
- [park](#park) 
- [threadpool](#threadpool)
- [timer service](#timer-service)

---
### coroutine
```
namespace mce {

struct coroutine {
    /**
     @brief construct an allocated coroutine from a Callable and arguments
     @param cb a Callable
     @param as arguments to pass to Callable
     @return an allocated coroutine
     */
    template <typename Callable, typename... As>
    static std::unique_ptr<coroutine> make(Callable&& cb, As&&... as);

    /// construct a coroutine from a thunk
    template <typename THUNK> 
    mce::coroutine(THUNK&& th);

    /// construct a coroutine from a stack allocator and thunk
    template <typename StackAllocator, typename THUNK>
    coroutine(StackAllocator&& sa, THUNK&& th);

    coroutine(mce::coroutine&& rhs);
    coroutine& co::operator=(mce::coroutine&& rhs);

    /// Execute until thunk completes or yield() is called. If thunk is complete this function returns immediately.
    void run(); 

    /// Pause execution of the coroutine and return to run() caller.
    void yield(); 

    /// Returns true if thunk is complete, else false since the coroutine yielded early.
    bool complete();  
};

/// returns true if caller is a running coroutine, else false
bool in_coroutine();

/// returns a pointer to the coroutine running on the current thread
coroutine* this_coroutine();

/// Yield out of the current coroutine. No effect if running in a raw thread.
void yield();

}
```
`mce::coroutine` is the lowest level concurrent object managed by this library. It is a 
wrapper for the `boost::coroutines2::coroutine` object with some extra work 
done to expose the functionality into an external API. This is the object 
that all concurrent code is running inside of. 

`mce::coroutine`'s templated constructors expect a Callable (either a function pointer or 
an object (potentially a lambda) with an operator() method that accepts no 
arguments). If the StackAllocator constructor is used it will allocate the 
coroutine stack using that object instead of the default allocator.

---
### spinlock
```
class mce::spinlock;
void mce::spinlock::lock(); /// acquire the lock
bool mce::spinlock::try_lock(); /// attempt to acquire the lock
void mce::spinlock::unlock(); /// release the lock
```

`mce::spinlock` is the underlying atomic mechanism used by this library. Calls to `mce::spinlock::lock()` will cause the calling thread to continuously attempt to acquire the spinlock (does *NOT* wait on a condition). 

It is an error to attempt to call `mce::spinlock::lock()` from a coroutine when the associated operation is not guaranteed to be non-blocking. That is, all usage of `mce::spinlock` should be written in such a way that it is guaranteed to unlock in a small amount of time. Failing to implement this way can cause deadlock. 

It *is* possible (though *highly* discouraged) to call `mce::spinlock::try_lock()` continuously by a coroutine like thus:
```
while(!lk.try_lock()) 
{
    mce::yield(); // allow other coroutines to run
}
```

In such a scenario it is (almost certainly) better to block the coroutine using a `mce::scheduler::parkable` and unblock the coroutine with some other code in the future, because parking causes coroutines to stop using CPU and is very fast.

---
### scheduler
```
namespace mce {

struct lifecycle {
    enum state 
    {
        ready, /// initial state after construction or after resume() is called
        running, /// run() has been called and is executing coroutines
        suspended, /// temporarily halted
        halted /// permanently halted by a call to halt() 
    };
};

struct scheduler {
    // see dedicated section explaining this object
    struct park 
    {
        struct continuation;
        static void suspend(continuation& c);
    };

    // see dedicated section explaining this object
    struct parkable;

    // return the scheduler's state
    lifecycle::state get_state();

    // allocate and initialize a scheduler
    static std::shared_ptr<scheduler> make(); 

    // Block caller and start executing coroutines scheduled on the scheduler.
    // 
    // Returns `true` if suspend() is called and `false` if halt() is called, 
    /// allowing run() to be called in a loop:
    //
    // while(my_scheduler->run())
    // {
    //    // do other things after suspend()
    // }
    //
    // // do other things after halt()
    //
    //
    // Additional calls to `run()` while the scheduler is suspended will 
    // block the caller of `run()` in such a way that it executes no coroutines.
    // The caller will remain blocked until either `resume()` or `halt()` is 
    // called.
    bool run(); 

    // Cause current caller of run() to return early with the value `true`.
    // Subsequent calls to `run()` will block waiting until halt() or resume() is 
    // called. During this time, no coroutines will be executed.
    // 
    // Useful for putting schedulers "to sleep" in programs which have lifecycle 
    // management.
    bool suspend(); 

    // Resume suspended evaluation of coroutines on the scheduler (future or 
    // current calls to run() on the suspended scheduler will resume coroutine 
    // evaluation)
    bool resume(); 

    // Forever halt running coroutines and synchronize with exitting run(), which 
    // will return `false` to its caller
    void halt(); 

    /**
     @brief Schedule allocated coroutines

     Arguments to this function can be:
     - an allocated `std::unique_ptr<mce::coroutine>`
     - an iterable container of allocated `std::unique_ptr<mce::coroutine>`s

     After the above argument types, any remaining arguments can be:
     - a Callable (followed by any arguments for the Callable) 

     The user can manually construct allocated 
     `std::unique_ptr<mce::coroutine>`s with `mce::coroutine::make()`

     Multi-argument schedule()s hold the scheduler's lock throughout (they are 
     simultaneously scheduled).

     @param a the first argument
     @param as any remaining arguments
     */
    template <typename A, typename... As>
    void schedule(A&& a, As&&... as);

    /// an object which represents the scheduler's workload
    struct measurement 
    {
        measurement();
        measurement(const measurement& rhs);
        measurement(measurement&& rhs);
        measurement(size_t enqueued, size_t scheduled);

        measurement& operator=(const measurement& rhs);
        measurement& operator=(measurement&& rhs);

        bool operator ==(const measurement& rhs);
        bool operator <(const measurement& rhs);
        bool operator <=(const measurement& rhs);

        /// convert to a directly comparable fundamental type
        operator size_t() const;
        
        /// count of all scheduled coroutines, blocked or enqueued
        size_t scheduled() const;

        /// count of coroutines actively enqueued for execution
        size_t enqueued() const;
        
        /// count of coroutines blocked on the scheduler
        size_t blocked() const;
    };

    /// return a value representing the current scheduling load
    measurement measure();

    /// return a copy of this scheduler's shared pointer by conversion
    operator std::shared_ptr<scheduler>();
};

/// returns true if calling code is executing inside a scheduler, else false
bool in_scheduler();

/**
 Return scheduler that this code is running inside of. Retaining a copy of this 
 scheduler can be used to keep the scheduler in scope during blocking operations.
 */
scheduler& this_scheduler(); 

}
```
`mce::scheduler` is the object responsible for scheduling and executing `mce::coroutine`s on an individual operating system thread. It can be used on any arbitrary thread, including the main thread. Its public api is both threadsafe and coroutine-safe (the API can be called by `mce::coroutine`s already running on the `mce::scheduler`). It is even safe to run a `mce::scheduler` inside a `mce::coroutine` running on another `mce::scheduler`!

[example_019 source](ex/src/example_019.cpp)
```
// example_019
#include <iostream>
#include "mce.hpp"

int main()
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();                                   
    mce::chan<int> done_ch = mce::chan<int>::make(); 
    
    auto recv_function = [](mce::chan<int> done_ch)
    {                                                                              
        int r;
        done_ch.recv(r);
        std::cout << "recv done" << std::endl;
        mce::this_scheduler().halt();                                              
    };                

    auto send_function = [&](mce::chan<int> done_ch)
    { 
        mce::this_scheduler().schedule(recv_function, done_ch);
        std::cout << "send done" << std::endl;
        done_ch.send(0);
    };

    cs->schedule(send_function, done_ch); // schedule send_function for exection
    cs->run(); // execute coroutines on the current thread until halt() is called
    return 0;
}
```

Terminal output:
```
$ ./ex/example_019
send done
recv done
```

---
### parkable 
```
struct mce::scheduler::parkable;

// constructor
mce::scheduler::parkable();

// Block the calling coroutine or system thread until `unpark()` is called. 
// Argument lk is unlocked before blocking.
template <typename LOCK>
void mce::scheduler::parkable::park(LOCK& lk);

// Unblock and reschedule the blocked operation.
// Argument lk is locked before unblocking .
template <typename LOCK>
void mce::scheduler::parkable::unpark(LOCK& lk);
```

`mce::scheduler::parkable` is a special struct that is deeply integrated with the `mce::scheduler` object. It is responsible for blocking (all operations ceasing) and unblocking (resuming operations) of a calling `mce::coroutine` OR regular system thread. All blocking operations (that do not busy wait) implemented by this library utilize this object.

For best results, `mce::scheduler::parkable` objects should be used by `mce::coroutine`s scheduled on a `mce::scheduler` or on a non-coroutine thread. Technically speaking, this operation will function with a raw `mce::coroutine` *not* running in a scheduler, but calling `park()` in this situation will just cause `mce::coroutine::resume()` to immediately `mce::coroutine::yield()` until `unpark()` is called.

`mce::scheduler::parkable::park()` accepts any object capable of calling `lock()` or `unlock()`. `park()` assumes its argument object is already locked. `park()` will `unlock()` its argument lock just before blocking the caller. `unpark()` will `relock()` `park()`'s argument when it is reschuled (`unpark()`'s argument is used to synchronize with the resumed caller or `park()`).

The side effects of locking and unlocking need to be clearly understood by the implementor, to avoid causing deadlock in coroutines. Typical usage is to pass a locked `std::unique_lock<mce::spinlock>` to `park()`, but other usages and types are acceptable.

When a `mce::coroutine` running in a `mce::scheduler` blocks with a `mce::scheduler::parkable`, the `mce::scheduler` assigns the `mce::coroutine`'s `unique_ptr` to the `parkable`. If the `parkable` is on the `mce::coroutine`'s stack (either directly or as a shared pointer) this creates a chain of circular memory where the `mce::coroutine`'s allocated stack hold the parkable in memory, and the `parkable` holds the `mce::coroutine`'s stack in memory. When a `parkable` is `unpark()`ed, the `mce::coroutine`'s `unique_ptr` is returned to the `mce::scheduler`, allowing it's lifecycle to continue.

This is an *IMPORTANT* detail, if a `mce::coroutine` is never `unpark()`ed, it is effectively a *memory leak*. This means the user must properly `close()` channels or otherwise ensure `mce::coroutine`s are not blocked when operations cease. 

It is technically possible to use a `parkable` allocated somewhere outside of the `mce::coroutine`'s stack, though it is generally unnecessary to do so, as it can generally be created on a the parked coroutine or thread's stack.

---
### park 
```
struct mce::scheduler::park 
{
    struct continuation
    {
        // pointer to coroutine storage location
        std::unique_ptr<mce::coroutine>* coroutine; 

        // memory to source scheduler storage location
        std::weak_ptr<mce::scheduler>* source;

        // arbitrary memory
        void* memory; 

        // cleanup procedure executed after acquiring coroutine
        void(*cleanup)(void*); 
    };

    /*
     @brief the fundamental operation required for blocking a coroutine running in a scheduler

     Direct usage of this is not recommended unless it is truly required, prefer
     scheduler::parkable instead.

     It is an error if this is called outside of a coroutine running in a 
     scheduler.
    */
    static void suspend(continuation& c);
};
```

This object is the lowest level coroutine blocking mechanism. It is utilized by the higher level `mce::scheduler::parkable` object. In comparison, this is a more basic type only for blocking coroutines, and requires more manual work because of its more generic nature. However, this type is exposed in case it is required by user code.

All higher level coroutine blocking mechanics are built on this structure. A mutable reference to a `park::continuation` is passed to `park::suspend()`, which will register the continuation with the `scheduler` running on the current thread and `yield()` control to said `scheduler`, suspending execution of the calling coroutine.

When the `scheduler` resumes control, it will assign the just running coroutine to the dereferenced `std::unique_ptr<mce::coroutine>` `coroutine`. It will then assign its weak_ptr to the dereferenced `std::weak_ptr<scheduler>` `source`, allowing the owner of the continuation to re-schedule the coroutine on the source scheduler. 

After passing the coroutine to its destination, the specified `cleanup()` method will be called with `memory`. This can be any operation, accepting any data as an argument. A common usecase is to unlock an atomic lock object.

At this point, control of the given `coroutine` completely leaves the `scheduler`, it is up to the destination code to decide what to do with the `coroutine`.

---
### threadpool
```
namespace mce {

struct threadpool 
{
    /**
     @brief construct an allocated threadpool with a count of worker threads
     */
    static std::shared_ptr<threadpool> make(size_t worker_count = 0);

    /// halt(), join() and delete all workers
    ~threadpool();

    /// return the workers' state
    lifecycle::state get_state();

    /// suspend all workers, returning true if all workers suspend() == true, else false
    bool suspend();

    /// resume all workers
    void resume();

    /// halt all workers
    void halt();

    /// return the count of worker threads
    size_t size();

    /// access the scheduler for a worker at a given index
    scheduler& worker(size_t idx);

    /// return the least busy worker scheduler at time of call
    scheduler& worker();

    /**
     This operation is more expensive than calling worker(). It should be
     unnecessary in most cases to call this method, but it is provided in case
     some sort of high level scheduler management is desired by the user and 
     access to all of `std::vector`'s utility functionality would be beneficial.

     @return the schedulers for all running worker threads
     */
    std::vector<std::shared_ptr<scheduler>> workers();

    /// return a copy of this threadpool's shared pointer by conversion
    operator std::shared_ptr<threadpool>();
};

// returns true if calling thread is a threadpool worker
bool in_threadpool();

// returns the threadpool for the calling threadpool worker thread
threadpool& this_threadpool(); 

/// return true if default_threadpool() can be safely called, else false
bool default_threadpool_enabled();

// configurable value to specify the number of threads in the default workerpool, it can be set in CMakeLists.txt
#define MCEMAXPROCS 

// return the process-wide default threadpool used by higher level parallel mechanisms
threadpool& default_threadpool();

}
```

Creating a `mce::threadpool` object will launch 1 or more operating system 
worker threads with running `mce::scheduler`s. If no number of workers are 
specified, this function internally decides how many workers to allocate for 
best performance. This provides fine tuned control over how many threads are 
available to a subset of coroutines to execute on. 

When a `mce::threadpool` is destroyed, the `mce::scheduler`s on its managed 
threads are halted and the worker threads joined.

`mce::threadpool::schedule` schedules a function to be executed as a coroutine 
on the threadpool. A pointer to the threadpool associated with the current 
thread (either user created or default) can be retrieved with 
`mce::this_threadpool()`.

[example_020 source](ex/src/example_020.cpp)
```
// example_020  
#include <iostream>
#include "mce.hpp"

int main()
{
    mce::chan<int> ch = mce::chan<int>::make();
    mce::chan<int> done_ch = mce::chan<int>::make();
    
    auto f = [](mce::chan<int> done_ch, mce::chan<int> ch)
    {
        // schedule coroutine by indirectly using the threadpool
        mce::this_threadpool().worker().schedule([ch]{ ch.send(0); });
        
        int x;
        ch.recv(x);
        
        mce::this_threadpool().worker().schedule([=]{ done_ch.send(0); });
    };
    
    // start a threadpool with 2 worker threads
    std::shared_ptr<mce::threadpool> tp = mce::threadpool::make(2);
    
    // schedule coroutine by directly using the threadpool
    tp->schedule(f, done_ch, ch);
   
    int r;
    done_ch.recv(r);
    tp->halt();
    std::cout << "received done confirmation" << std::endl;
    return 0;
}
```

Terminal output:
```
$ ./ex/example_020
received done confirmation
```

A process-wide default `mce::threadpool` can be generated/accessed as necessary via `mce::default_threadpool()`. It is the `mce::threadpool()` accessed by calls to `mce::parallel` and other similar procedures. As such it is important in programs which can pause and resume their operations that `suspend()` and `resume()` be called during the appropriate process-wide lifecycle state change functions. 

NOTE: If a `scheduler` managed by a `threadpool` has its `suspend()`/`resume()`/`halt()` methods called, it will *actually* call the `threadpool`'s implementations of the same methods!

The thread worker count of `mce::default_threadpool()` can be specified when compiling your software by modifying CMakeLists.txt variable `MCEMAXPROCS`. If `MCEMAXPROCS` is left undefined, the library will be compiled with an internally determined worker count (which aims to achieve peak CPU throughput).

---
### timer service 
Timer types & utility functions:
```
enum time_unit 
{
    hour,
    minute,
    second,
    millisecond,
    microsecond,
    nanosecond
};

typedef std::chrono::steady_clock::time_point time_point; 
typedef std::chrono::steady_clock::duration duration;

struct timer_id;

mce::duration get_duration(mce::time_unit u, size_t count);
size_t get_time_point_difference(mce::time_unit u, mce::time_point p0, mce::time_point p1);
mce::time_point current_time();
```

`timer_service` functions:
```
mce::timer_service::timer_service(); 

// Shutdown and join with asynchronous timer service if shutdown() has 
// not been previously called.
mce::timer_service::~timer_service(); 

void mce::timer_service::start(); // start timer service on current thread
void mce::timer_service::ready(); // blocks until service is running
void mce::timer_service::shutdown(); // inform service to shutdown and join with service

// returns unsigned integer size_t representing the id of the
// created timer. Pass this id to remove() to delete the timer (if it 
// has not timed out).

// start a timer 
template <typename THUNK>
timer_id timer(const mce::time_point& timeout, THUNK&& timeout_handler);
template <typename THUNK>
timer_id timer(const mce::duration& d, THUNK&& timeout_handler);
template <typename THUNK>
timer_id timer(const time_unit u, size_t count, THUNK&& timeout_handler);

// return true if timer is running, else false
bool mce::timer_service::running(mce::timer_id id); 

// Remove a running timer. Returns true if timer was found and removed, 
// else returns false.
bool mce::timer_service::remove(mce::timer_id id);

// Remove all pending timers
void mce::timer_service::clear();

// Returns a pointer to default timer service, which is guaranteed to be running on 
// another thread by the time this call returns. It is recommended that the user NOT
// use this service directly, instead using high level mce::timer()/mce::sleep()
// operations
mce:timer_service& mce::default_timer_service(); 
```
A tiny asynchronous timer service implementation. This service is not 
designed to work inside of coroutines and is unsafe to do so, it will almost 
certainly cause deadlock. To safely interact with coroutines, extra work 
must be done so that the timeout_handler executes threadsafe code (which can 
include rescheduling a coroutine or notifying a coroutine via a channel).

Start the service:
```
mce::timer_service my_timer_service;
std::thread thd([&]{ my_timer_service.start(); });
my_timer_service.ready(); // block until started
```

Usage is as simple as:
```
mce::timer_id tid = my_timer_service.timer(mce::time_unit::microsecond, 
                                           microsecs_till_timeout, 
                                           my_timeout_handler);
```

The timer can be synchronously removed (if it is not already executing) with:
```
my_timer_service.remove(tid);
```

## Concurrent Lifecycle Management
![mercury_icon](img/mercury_icon_tiny.png)

See `suspend()` and `resume()` procedures in the following sections for how to
pause/unpause execution of coroutines:
- [scheduler](#scheduler) 
- [threadpool](#threadpool)

Execution of `mce::coroutine`s scheduled on `mce::scheduler`s are started with calls to `run()` and permanently halted with `halt()`.

`mce::threadpool`s automatically call `run()` on their internally managed threads when they are created. However, the user must call `halt()` on the `mce::threadpool` to permanently halt its worker threads. The exception to this is the default threadpool (`mce::default_threadpool()`) which will call `halt()` when the process ends. 

The user is responsible for calling `threadpool::suspend()`/`threadpool::resume()` on *all* `threadpool`s as necessary (including on the `mce::default_threadpool()`!). `scheduler::suspend()`/`scheduler::resume()` will also need to be called for any manually managed `mce::scheduler` instances (IE, those not created by a `threadpool`). Call `suspend()` when `coroutine` execution must temporarily cease, and call `resume()` when execution should resume.

Use this information when implementing process lifecycle procedures related to
process state like: init/wakeup/run/sleep/shutdown.

For example, `mce::default_threadpool()` returns the default `mce::threadpool` used by most high level features (like `mce::parallel()`), and will need to be
`suspend()`ed/`resume()`ed by the user when the process is supposed to sleep/wakeup.
