//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#ifndef __CPLUSPLUS_COROUTINE_CONCURRENCY_UNIT_TEST_LOGGING__
#define __CPLUSPLUS_COROUTINE_CONCURRENCY_UNIT_TEST_LOGGING__ 

#include <iostream>

namespace ut {

struct logging_control 
{
    logging_control() = delete;

    inline static bool& enabled()
    {
        static bool enabled = false;
        return enabled;
    }

    inline static std::basic_ostream<char>*& stream()
    {
        static std::basic_ostream<char>* bos = &(std::cout);
        return bos;
    }
};


inline void log()
{
    if(logging_control::enabled())
    {
        *(logging_control::stream()) << std::endl;
    }
}

template <typename A, typename... As>
void log(A&& a, As&&... as)
{
    if(logging_control::enabled())
    {
        *(logging_control::stream()) << std::forward<A>(a);
        log(std::forward<As>(as)...);
    }
}

}

#endif
