//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
/**
@file print.hpp 
Synchronized print functionality for printing during development of this library.
*/
#ifndef __CPLUSPLUS_COROUTINE_CONCURRENCY_DEVELOPMENT_PRINT__
#define __CPLUSPLUS_COROUTINE_CONCURRENCY_DEVELOPMENT_PRINT__

#include <mutex>
#include <thread>
#include <sstream>
#include <iostream>

#include "atomic.hpp"

namespace ccc {
namespace dev {
namespace detail {

struct dev_print_locals 
{
    static ccc::unique_spinlock acquire_lock();
};

inline void stream(std::stringstream&) { }

template <typename A, typename... As>
void stream(std::stringstream& ss, A&& a, As&&... as) 
{
    ss << std::forward<A>(a);
    stream(ss, std::forward<As>(as)...);
}

}

template <typename... As>
void print(As&&... as) 
{
    std::stringstream ss;
    ss << "[thread 0x" 
       << std::hex 
       << std::this_thread::get_id()
       << std::dec 
       << "] ";
    detail::stream(ss, std::forward<As>(as)...);

    auto lk = detail::dev_print_locals::acquire_lock();
    std::cout << ss.str() << std::endl;
}

}
}

#endif
