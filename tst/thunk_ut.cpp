//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "function_utility.hpp"

#include <functional>

#include <gtest/gtest.h>

TEST(thunk, make_thunk) 
{
    int c = 0;

    // void case;
    auto f = [&c](int a)
    { 
        c = a; 
        return c;
    };

    mce::thunk t1 = mce::make_thunk(f,3);
    mce::thunk t2 = mce::make_thunk(f,5);
    mce::thunk t3 = mce::make_thunk(f,0);

    EXPECT_TRUE(c == 0);
    t1();
    EXPECT_TRUE(c == 3);
    t2();
    EXPECT_TRUE(c == 5);
    t3();
    EXPECT_TRUE(c == 0);
}

TEST(thunk, make_thunk_void_specialization) 
{
    int c = 0;

    // void case;
    std::function<void(int)> f = [&c](int a){ c = a; };

    mce::thunk t1 = mce::make_thunk(f,3);
    mce::thunk t2 = mce::make_thunk(f,5);
    mce::thunk t3 = mce::make_thunk(f,0);

    EXPECT_TRUE(c == 0);
    t1();
    EXPECT_TRUE(c == 3);
    t2();
    EXPECT_TRUE(c == 5);
    t3();
    EXPECT_TRUE(c == 0);
}
