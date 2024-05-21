# Mercury Concurrency Engine
![mercury](img/mercury.jpg)

## Table of Contents  
- [legal](#legal)
- [documentation](#documentation)
- [rationale](#rationale)
- [prerequisites](#prerequisites)
- [building the library](#building-the-library)
- [linking the library](#linking-the-library)
- [integrating the library](#integrating-the-library)
- [building unit tests](#building-unit-tests)
- [building examples](#building-examples)
- [continuous integration tests](#continuous-integration-tests)
- [bug reports](#bug-reports)
- [contributing](#contributing)

## Legal
![mercury_icon](img/mercury_icon_tiny.png)

Licensed under the Apache 2.0 License:

[LICENSE.txt](LICENSE.txt)

You may not use this file except in compliance with the License.

Software distributed under the License is distributed on an "AS IS" BASIS and WITHOUT WARRANTY, either expressed or implied.

## Documentation 
![mercury_icon](img/mercury_icon_tiny.png)

[High Level Cpp Coroutine Concurrency Summary](high_level_documentation.md)

[Summary of Concurrent Operations](concurrency_summary.md)

[Summary of Data Communication Operations](data_communication_summary.md)

[Doxygen Documentation](https://elektrobit.github.io/mercuryconcurrencyengine/)

### Generate Doxygen Documentation
`doxygen` documentation can be generated locally for this library. Install `doxygen` and `graphviz` packages then run `doxygen`:
```
cd /path/to/your/mce/repo/checkout
doxygen
```
The generated `doc/` directory should contain both `html` and `rtf` generated documentation. For `html`, open `index.html` by a browser to view the documentation. 

## Rationale
![mercury_icon](img/mercury_icon_tiny.png)

C++ concurrency is too complicated. Many modern languages implement similar functionality through simpler and/or more expressive mechanisms, often executed at a comparable, or *better*, speed.

1) Data Communication: It is too hard to share data between different places in code, threaded or otherwise. This library implements channel mechanisms for communicating data in a simple, threadsafe, and templated manner.

2) Concurrency: It is too hard to set up an efficient and easy-to-use concurrency model where `std::async` is insufficient. Similarly, coroutines are not being leveraged in most C++ applications as a generic way to improve code efficiency. This project implements coroutine mechanisms for concurrency that utilize both coroutines and a generic worker thread backend to efficiently execute tasks in parallel. 

This library attempts to address the above concerns by providing a concurrency model that:
- leverages coroutines for efficiency improvements
- implements simple multithreaded executors
- implements communication mechanisms between any combination of coroutines and threads
- is easy to use 
- is extremely fast 
- is very flexible
- is easy to integrate in existing code

### What is a coroutine?
Definition from [wikipedia](https://en.wikipedia.org/wiki/Coroutine):
```
Coroutines are computer program components that generalize subroutines for non-preemptive multitasking, 
by allowing execution to be suspended and resumed. Coroutines are well-suited for implementing familiar 
program components such as cooperative tasks, exceptions, event loops, iterators, infinite lists and pipes.
```

Coroutines are very useful tools in concurrency because they are both lightweight and extremely fast when context switching. In fact, unit testing in this project reveals some edgecases which are greater than ~10x faster compared to threads (in some extreme edgecases, MUCH faster than that). These properties allow for efficient concurrent algorithms which are otherwise very difficult to write cleanly.

This library makes heavy use of coroutines (provided by `Boost.Context` and `Boost.Coroutine2`) to implement its features.

### Why this instead of Boost.Fiber, libdill or C++20 coroutines?
There are a few reasons to use this instead of other projects like [Boost.Fiber](https://github.com/boostorg/fiber), [libdill](https://github.com/sustrik/libdill), or [c++20 coroutines](https://en.cppreference.com/w/cpp/language/coroutines).

First of all, each of the above frameworks has some very inconvenient drawbacks:

`Boost.Fiber` is more of a "base library", intended to be used in *other* libraries. It requires a significant investment from the user to integrate and use it. It requires a much higher degree of knowledge to begin implementation, with a higher burden of knowledge required for general developers, due to not having a built in solution for blocking code nor a convenient mechanism for communicating between standard system threads and coroutines. It even requires the user to consider writing their own scheduling algorithms.

`libdill`, a `c` library, does not implement its framework with multithreading in mind, presupposing that programs would be singlethreaded with coroutines providing concurrency. It's documentation does suggest that multithreading is possible but does not provide much assistance in making it work. No doubt this is ideal in many programs which desire to write their code in `c`. However, the lack of default multithreaded executors limits the usability and potential scope of the library for many applications (especially ones that don't want to write and debug their own!). Futhermore, because `libdill` is a `c` library, it eschews much of the convenience and simplicitly possible in a well designed `c++` library due to language limitations.

Finally, `c++20 coroutines` are... an intensely complicated topic. They are extremely fast and lightweight, but just as extremely low level. They are highly structured in a way that limits what can be done with them. Their requirements essentially require that any program that utilizes them must design their program around `c++20 coroutines` at some level rather than simply making use of them. Most of all, they are *anything* but easy to use. Frankly, I consider them, at least in their current form, a topic only approachable by `c++` experts, and could not be easily utilized by a broader team.

In comparison, this library attempts to address the various issues from the other frameworks:

#### This library's API is very simple 
The mechanisms in this library are designed for simplicity and usability first, instead of as a framework which must provide maximum design flexibility. Mechanisms make use of `c++` templates to simplify user boilerplate as much as possible.

Scheduling coroutines on backend worker threads is simple, fast, flexible, safe and setup by default. No extra work is required to efficiently schedule coroutine tasks in multithreaded pool of workers. Simply use `mce::parallel()` (to prefer multi cpu-core scheduling) or `mce::concurrent()` (to prefer scheduling on the current cpu if possible):
[example_009 source](ex/src/example_009.cpp)
```
#include <string>
#include <iostream>
#include <mce/mce.hpp>

void concurrent_foo(mce::chan<std::string> ch) {
    ch.send("hello");
}

int main() {
    mce::chan<std::string> ch = mce::chan<std::string>::make();
    mce::concurrent(concurrent_foo,ch);

    // interact with concurrent_foo by sending messages over channel object "ch"
    std::string s;
    ch.recv(s);

    std::cout << "received: " << s << std::endl;
    return 0;
}
```

Terminal output:
```
$ ./ex/example_009 
received: hello
$
```

I have found that writing scheduling algorithms is always a dangerous undertaking. Having scheduling and backend worker thread executors setup by default with a simple API makes coroutines accessible to programmers.

- `mce::concurrent()`: launch a coroutine, prefering to be launched on the current thread if possible (caller is running on a worker thread) and a background worker thread otherwise. That is, the launched coroutine is concurrent but not guaranteed to be parallel, and typically has the shortest communication latency. This is the recommended default scheduling algorithm.
- `mce::parallel()`: launch a coroutine distributed efficiently across 1 or more background worker threads based on scheduling load. The launched coroutine is concurrent and parallel.
- `mce::balance()`: launch a coroutine distributed efficiently across 1 or more background worker threads based on scheduling load. This is a best effort algorithm which prefers to schedule on the calling thread for best-case communication latency, but will schedule on other threads if CPU workload becomes too imbalanced. 

As a generalized rule:
- when CPU throughput is not a bottleneck: a single core is the most efficient (use `mce::concurrent()`)
- when CPU throughput is a bottleneck: multiple cores are the most efficient (use `mce::parallel()`)
- when CPU *may* become a bottleneck: prefer single core but fallback to multiple cores (use `mce::balance()`)

It should be noted that in many or most cases the bottleneck is actually not the CPU throughput. In fact, program bottlenecks are not the real reason for many uses of threads, the user may be leveraging threads for asynchronous behavior in general. In those circumstances, best results will probably be found with `mce::concurrent()`.

There are various mechanisms to control the configuration of the background threads (including disabling them, or specifying the count of threads), see [Building subsets of library features](#building-subsets-of-library-features) for more information.

##### Accessible Low level API:
The user can manually create and manage `mce::scheduler` objects to schedule and execute coroutines. If the user desires, they can even create custom `mce::coroutine` objects to manually manage.

See [summary of concurrent operations](concurrency_summary.md) for documentation on all of the above features.

#### Ease of Integration
All concurrent mechanisms in `mce` are designed to behave correctly whether they are being called from a standard thread (`std::thread`, generally a `pthread` under the hood) or being called from within a coroutine (`mce::coroutine`, a managed `boost::coroutines2::coroutine` under the hood). *This is not a trivial feature*, and not all competing `c++` frameworks attempt it. In particular, none that I know of correctly synchronize between `coroutines` and code running *outside* of `coroutines` like this library does by default.

*This makes it very easy to integrate this library into existing `c++` code*.

A `std::thread` calling a `mce::buffered_channel`'s `send()` will behave in a similar way as a `mce::coroutine` object (executing inside a `mce::scheduler`) calling `send()`, even though their blocking mechanisms are completely different underneath. 

List of objects and mechanisms which share this interoperability:
```
mce::unbuffered_channel
mce::buffered_channel
mce::chan
mce::mutex
mce::condition_variable
mce::concurrent()
mce::parallel()
mce::balance()
mce::await()
mce::timer()
mce::sleep()
```

#### High Level Communication Mechanisms 
A variety of flexible and easy to use communication mechanisms are provided out of the box to handle 99% of communication needs within a single process:
```
mce::chan // wrap any other channel (a synchronized queue-like object allowing sends and receives)
mce::unbuffered_channel // channel with minimal memory footprint that leverages coroutine context switching efficiency
mce::buffered_channel // channel with configurable memory footprint (buffer size)
mce::await() // block an operation without blocking the calling thread if called by a coroutine
mce::mutex // a replacement for `std::mutex` which works with coroutines and threads, usable with mce::condition_variable
mce::condition_variable // a replacement for `std::condition_variable` for use with `mce::mutex`
```

#### Trivial blocking operations
One of the inherent pitfalls of coroutines is that their blocking behavior is managed by the program instead of by the operating system. This means you can cause unexpected deadlock and innefficiency when trying to call any function that blocks a worker thread where coroutines are running. `Boost.Fiber` specifies the user should either implement non-blocking IO calls or to setup an asynchronous callback mechanism with `Boost.Asio` to deal with this limitation. 

I think this is far too much work to expect of the average programmer (OR an architect, who has many other important things to do) when dealing with something as complex as coroutine concurrency, with far too little gained in efficiency in the average case. 

Instead, `mce` provides a generic `mce::await()` function which will provide an `std::thread` and execute the blocking code there instead. When user function finishes, `mce::await()` will resume the `mce::coroutine` back to the original thread it was running on and return the value of asynchronously executed function. 

That is, there's no need to have to manage futures and promises when dealing with `mce::await()` calls, you just get the result when it completes:
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

It should be noted that in the above example the function passed to `mce::await()` accesses values on the coroutine's stack *by reference*. This is totally safe because when `mce::await()` is executing:
- the coroutine or thread calling it will be blocked, meaning there's no competition for accessing values on their stacks
- the coroutine or thread calling it will be guaranteed to keep their stack in memory

This means it's generally safe to read/write to memory in their stacks *by reference*. You can think of functions passed to `mce::await()` as "executing on the stack" of the caller, even though they are actually executing *somewhere else*.

If `mce::await()` is called outside of a coroutine running on a `mce::scheduler` OR `mce::await()` was called inside another call to `mce::await()`, the argument Callable is executed immediately on the current thread instead of on another thread. Otherwise, the calling coroutine will be blocked an the operation passed to `mce::await()` will be executed on another, dedicated thread inside another `mce::scheduler`.

Calls (or implicit calls) to `this_scheduler()`, `this_scheduler_ref()`, `this_threadpool()`, or `this_threadpool_ref()` made within a call to `mce::await()` will return the values they would have if they had been called outside of `mce::await()`. That is, code running in `mce::await()` will be able to schedule operations as if it was running in its original execution environment. IE, calls to `mce::concurrent()`, `mce::parallel()`, and `mce::balance()` will function "normally".

##### What about Networking or other Features?
A common feature in coroutine frameworks is the presence of ipv4/ipv6 support. Said frameworks want to provide a coroutine compatible API for said operations. IE, networking is, by design, a blocking task, and blocking tasks running in a coroutine scheduling environment can introduce latency or deadlock.

However, it should be pointed out that the real problem here is not that networking "functionality" creates problems for coroutines but the fact that networking *features* are *blocking*. As discussed in the previous section, blocking tasks can be passed to `mce::await()` for execution where they won't affect scheduling of any other coroutines. Additionally, because communication channels work in both coroutines and standard threads, the task executing in `mce::await()` (or any other `std::thread`) can communicate back to running coroutines using said channels. This allows the user to use *whatever* standard networking implementation they desire and route the results back into the coroutine environment when they are available. 

The main consideration the user may need to make when executing blocking networking operations in `mce:await()` calls is the number of default await worker threads they want available by default. `mce::await()` will spawn additional threads as necessary, but it tries to keep a preconfigured number of reusable worker threads in existence. If the user is consistently executing many blocking calls they can consider spawning more reusable worker threads to improve efficiency. This count of worker threads can be modified in this library's toplevel `CMakeLists.txt` by modifying the `MCEMINAWAITPROCS` variable.

## Prerequisites 
![mercury_icon](img/mercury_icon_tiny.png)

#### Environment
This project is designed for a POSIX linux environment.

#### Libraries
The following POSIX libraries are required:
- pthread // it is potentially possible that a different threading library is required depending on OS configuration

This library directly utilizes the following [boost](https://www.boost.org/) libraries:
- boost/coroutine2 
- boost/context 

As well as the following boost header only libraries (and dependencies):
- boost/circular_buffer 

It should be noted that the boost features are automatically downloaded and built from source for this project, as necessary.

#### Tools 
- c++11 compiler toolchain 
- cmake
- make
- python3
- wget 
- tar

## Building the library
![mercury_icon](img/mercury_icon_tiny.png)

If you want to build unit tests you will need to checkout submodules for the googletest framework:
```
git clone --recurse-submodules git@git.elektrobitautomotive.com:dennbla/cppconcurrency.git
```  

Building on linux:
```
cmake .
make mce
```

Which produces the static library:
```
libmce.a
```

### Boost Version
The library build `make` targets will download and build `boost` from source if the specified version of boost source code is not available. The version of boost can be configured by modifying the following variables in the [CMakeLists.txt](CMakeLists.txt):
```
set(MCE_BOOST_MAJOR_VERSION 1)
set(MCE_BOOST_MINOR_VERSION 85)
set(MCE_BOOST_PATCH_VERSION 0)
```

If the boost download url becomes outdated, consider either updating the `MCE_BOOST_DOWNLOAD_URL` `CMake` variable.

#### Pre-downloaded Boost
You can manually download and extract boost in the root of this repository to `boost/boost_major_minor_patch` (replace `major`, `minor`, and `patch` with appropriate version values and be sure to update the `CMakeLists.txt` `MCE_BOOST_MAJOR_VERSION`, `MCE_BOOST_MINOR_VERSION`, and `MCE_BOOST_PATCH_VERSION` variables).

Doing this will skip the `boost` download step.

#### Pre-built Boost
It is also possible to use linux symlinks to point to a *prebuilt* `boost` version. After making the symlink, you will need to create a file called `boost.built` in the project's root (with a command doing something like: `touch boost.built`).

WARNING: If you use a *prebuilt* version of `boost`, you *MUST* ensure that the following features are built:
```
context 
coroutine
thread
```

You can do this by passing the following arguments to `b2` when building `boost`:
```
--with-context --with-coroutine --with-thread
```

### Configuring library threadcounts
The following features can be configured `ON`/`OFF` enable or disable compile time defines which configure or disable default background threads:
```
# Define the count of threads in the background default threadpool accessed by 
# features like `mce::parallel()`. If `0` is set (the default), the library will 
# decide how many threads to spawn, preferring maximum CPU throughput.
set(MCEMAXPROCS 0)

# Define the minimum count of available threads for await tasks executed by 
# `mce::await()` (more will be temporarily spawned as necessary).
set(MCEMINAWAITPROCS 1)
```

The general design of a default compilation of this framework is to provide the following:
- N background worker threads (where N is equal to the number of CPU cores)
- 1 reusable background await thread (more will temporarily spawn as necessary)
- 1 background timer thread 

There will also be the main thread the process spawns on, so the total default threadcount after initialization is N+3.

#### Building the library with minimal threadcount
In many cases, having a minimal count of background worker threads is ideal. This is because many (most?) programs are not bottlenecked by CPU throughput, and will benefit instead from minimal coroutine communication latency. 

The `mce_minimal` `make` target is the equivalent of setting
```
set(MCEMAXPROCS 1)
```

Which limits the default threadpool to 1 worker thread. When heavy parallelization is not required, and the user only needs the efficiency gains and design simplicity of coroutines, this is a useful target to link against as this will enforce singlethreaded coroutine execution. 

Building on linux
```
cmake .
make mce_minimal
```

Which produces the static library:
```
libmce_minimal.a
```

Doing this will set the background executor to build with only 1 worker thread. Leaving all other options on their defaults, this results in your process launching with access to 4 default threads:
- main thread 
- coroutine executor background worker thread 
- await background worker thread (more will be spawned as necessary)
- timer background thread 

The 3 non-main threads will block and take no further resources until needed. This pattern is suprisingly useful, as the main thread can launch all other operations on the coroutine background worker thread using `mce::concurrent()`. If interprocess communication is necessary, the main thread can begin listening for messages at this point, otherwise it can block. 

A program designed for `mce` multithreaded execution (IE, explicit calls to `mce::parallel()` and `mce::balance()`) will still function in this environment. In fact, assuming the user is not manually using custom `mce::threadpool` objects, `mce::parallel()` and `mce::balance()` become nearly indistinguishable from `mce::concurrent()` when operating with a single executor thread, they "Just Work".

It should be noted that scheduling exclusively with `ccc::concurrent()` will provide similar behavior to compiling against `mce_minimal`, so if your program already is written with singlethreaded concurrency in mind, it may be unecessary to change link targets (though doing so will save a small amount of memory).

## Linking the library 
![mercury_icon](img/mercury_icon_tiny.png)

To link against and use this library you will need to add the boost root (IE, `boost/boost_1_85_0/`) and the `mce` `inc/` directory to your program's include flags.

Similarly you will need to instruct the linker about the boost lib and `mce` project root directories to find the relevant libraries (like `libmce.a`/`libmce_minimal.a`).

Given some shell environment variable named `MCE_ROOT` (which points to the `mce` project's root directory) here is a trivial Makefile illustrating how to link against the dependencies when using this library:
```
CC=g++
CFLAGS+="-I$MCE_ROOT/boost/boost_1_85_0 "
CFLAGS+="-I$MCE_ROOT/inc "
LDFLAGS+="-L$MCE_ROOT/boost/boost_1_85_0/stage/lib -L$MCE_ROOT "
LIBS=-lmce -lboost_coroutine -lboost_context -lpthread

yourprogram: yourprogram.cpp
	$(CC) $(LDFLAGS) $(CFLAGS) yourprogram.cpp $(LIBS) -o yourprogram

clean: 
	rm yourprogram
```

You can then include `#include <mce/mce.hpp>` (or any other header in `inc/mce/`). Example yourprogram.cpp:
[example_009 source](ex/src/example_009.cpp)
```
#include <string>
#include <iostream>
#include <mce/mce.hpp>

void concurrent_foo(mce::chan<std::string> ch) {
    ch.send("hello");
}

int main() {
    mce::chan<std::string> ch = mce::chan<std::string>::make();
    mce::concurrent(concurrent_foo,ch);

    // interact with concurrent_foo by sending messages over channel object "ch"
    std::string s;
    ch.recv(s);

    std::cout << "received: " << s << std::endl;
    return 0;
}
```

## Integrating the Library
![mercury_icon](img/mercury_icon_tiny.png)

Integrating any framework into an existing codebase is an art unto itself. However, this library is simpler to integrate compared to others of its kind because most code will function as normal when running in a coroutine or can be easily wrapped. For any code you wish to migrate to this framework, the following is suggested:

### Feature Checklist
Based on your program requirements, specify values in the `CMakeLists.txt` before building, if necessary:
- Background Executor Threadcount: control with `MCEMAXPROCS`
- Background Await Threadcount: control with `MCEMINAWAITPROCS`

### Integration Checklist
For all code that needs to be migrated to this library, complete the following checklist:
- replace all relevant usage of `std::thread`s or `pthread`s with `mce::concurrent()`/`mce::parallel()` etc. 
- replace all relevant usage of `std::mutex` or `pthread_mutex_t` with `mce::mutex` 
- replace all relevant usage of `std::condition_variable` or `pthread_cond_t` with `mce::condition_variable`
- wrap all other blocking or system calls with `mce::await()`
- implement calls to `mce::threadpool::suspend()`/`mce::threadpool::resume()`/`mce::threadpool::halt()` and/or `mce::scheduler::run()`/`mce::scheduler::suspend()`/`mce::scheduler::resume()`/`mce::scheduler::halt()` to properly pause/unpause/stop coroutine execution during process lifecycle changes. The default threadpool `mce::default_threadpool()` only needs `suspend()`/`resume()` to be called (`run()`/`halt()` are called by the library in this case).

If the user encounters deadlock, that is an indication that some blocking code was not properly addressed. Pay careful attention to the behavior of internal library code, and consider wrapping its blocking calls with `mce::await()`. If some code proves particularly difficult to wrap with `mce::await()`, consider leaving that code running on a separate system thread.

Assuming the user has relevant stress testing implemented the above should be sufficient to demonstrate categorical improvements to user code efficiency. 

### Additional Integration Improvements Checklist
Here are other efficiency improvement steps that can be taken when integrating this library (ordered from most to least important):
- replace interthread message passing and communication with channels. IE, channels are generally faster than raw mutex/conditions.
- if many `mce::await()` calls are being made, rebuild this framework with `MCEMINAWAITPROCS` set greater than `1`, as needed, so fewer threads need to be regularly spawned & destroyed.
- replace any slow timer usage (such as interprocess timers) with calls to `mce::timer()`/`mce::default_timer_service()->timer()`
- rewrite any algorithms that need to be concurrent, but not parallel, with `mce::concurrent()` or `mce::this_scheduler()->schedule()` to reduce communication latency introduced by unnecessarily running code on separate CPU cores.
- rewrite complicated asynchronous algorithms to benefit from direct coroutine channel communication, rather than abstracted through many levels of boilerplate. This will improve code clarity and may improve efficiency as well. IE, it's much easier to understand and debug two functions talking through a channel than a complicated array of asynchronous callbacks and state checks

## Building unit tests
![mercury_icon](img/mercury_icon_tiny.png)

From repository toplevel
```
cmake .  
make mce_ut  
```

run unit tests:
```
./tst/mce_ut
```

Tests are written using gtest and therefore can filter which tests will actually execute using `--gtest_filter="your regular expression here"` as an argument provided to `mce_ut`. 

## Building examples
![mercury_icon](img/mercury_icon_tiny.png)

From repository toplevel
```
cmake .  
make mce_ex
```

run unit examples (replace NNN with specific example number):
```
./ex/example_NNN
```

Below is the code for a standalone program from example code included in `ex/src`.

[example_002 source](ex/src/example_002.cpp)
```
// example_002
#include <iostream>
#include "mce.hpp"

int main()
{
  const int max = 10;
  auto ch = mce::chan<int>::make();
  auto done_ch = mce::chan<bool>::make();

  auto sender = [](int max, mce::chan<int> ch)
  { 
      for(int i=0; i<max; ++i)
      { 
          ch.send(i); 
      }
      ch.close();
  };

  auto receiver = [](int max, mce::chan<int> ch, mce::chan<bool> done_ch)
  { 
      for(auto& i : ch)
      {
          std::cout << i << std::endl;
      }
      done_ch.send(true); 
  };

  mce::parallel(sender, max, ch);
  mce::parallel(receiver, max, ch, done_ch);
  bool r;
  done_ch.recv(r);
  return 0;
}
```

#### terminal output
```
$ ./ex/example_002
0
1
2
3
4
5
6
7
8
9
```

## Continuous Integration Tests
![mercury_icon](img/mercury_icon_tiny.png)

The python script `continuous_integration.py` is to be run with `python3 ./script/continuous_integration.py`. It builds and runs all enabled unit tests and examples, to generally verify everything is working. Running this may take a long time. It may also heavily tax your system's cpu resources.

Continuous integration tests require both `gcc/g++` and `clang/clang++` toolchains to be installed on the system in their normal locations (`/usr/bin/`). Testing attempts to be fairly exhaustive.

## Bug Reports
![mercury_icon](img/mercury_icon_tiny.png)

If you are having issues with the code that is not solved by the help provided in the documentation:

[High Level Cpp Coroutine Concurrency Summary](high_level_documentation.md)

[Summary of Concurrent Operations](concurrency_summary.md)

[Summary of Data Communication Operations](data_communication_summary.md)

Then you are welcome to report an issue in the Issues tab. In your issue please include:
1. A clear, well written explanation of what you are trying to accomplish
2. A clear, well written explanation of the error
3. All necessary snippets of code that are failing
4. A copy, verbatim, of any error text reported by the compiler or executing environment
5. Information about your executing hardware: processor model, processor specs, RAM specs, etc.
6. Information about your executing environment: OS & OS version, build environment (ie, gcc or clang, cmake or gradle, etc. along with version information for said tools), build settings (the actual arguments given to each tool), etc.


## Contributing 
![mercury_icon](img/mercury_icon_tiny.png)

Contributions to this project are welcome, [please read this primer for instructions on how to contribute](how_to_contribute.md)
