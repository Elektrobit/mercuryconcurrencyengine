//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#ifndef __CPLUSPLUS_COROUTINE_CONCURRENCY_UNIT_TEST_FUNCTION_POINTER__
#define __CPLUSPLUS_COROUTINE_CONCURRENCY_UNIT_TEST_FUNCTION_POINTER__

#include "chan.hpp"

struct fptr
{
    static mce::chan<int> ch_;
    static int x_;

    inline static int f()
    { 
        int x2 = x_*2;
        ch_.send(x2);
        return x2; 
    }

    inline static int f2(int x)
    { 
        int xx = x_*x;
        ch_.send(xx);
        return xx;
    }
};

struct fptr_void
{
    static mce::chan<int> ch_;
    static int x_;

    inline static void f()
    { 
        int x2 = x_*2;
        ch_.send(x2);
    }

    inline static void f2(int x)
    { 
        int xx = x_*x;
        ch_.send(xx);
    }
};

void reset_fptr_vals();

#endif
