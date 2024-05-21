//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "timer.hpp"
#include "function_utility.hpp"
#include "scheduler.hpp"
#include "chan.hpp"

#include <thread>
#include <iostream>
#include <exception>

#include <gtest/gtest.h>

using namespace mce;

TEST(coroutine, deconstructor)
{
    mce::thunk t = [](){};
    mce::coroutine main(t);

    int c = 0;
    mce::thunk ct = [&c](){ ++c; };
   
    EXPECT_TRUE(c==0);
    {
        mce::coroutine c(ct);
    }
    EXPECT_TRUE(c==0);

    {
        mce::coroutine c(ct);
        c.run();
    }
    EXPECT_TRUE(c==1);
}

TEST(coroutine, run) 
{
    int c = 0;
    std::function<void(int)> f = [&c](int a){ c = a; };
    
    mce::coroutine c1(mce::make_thunk(f,3));

    EXPECT_TRUE(c == 0);
    c1.run();
    EXPECT_TRUE(c1.complete());
    EXPECT_TRUE(c == 3);
}

TEST(coroutine, recursion_and_coroutine_stacks)
{
    std::function<void(std::uint64_t)> f = [&](std::uint64_t cnt)
    {
        if(cnt>0)
        {
            // first stress the current stack
            f(cnt-1);

            std::uint64_t new_cnt = cnt - 1;

            // now create new stacks with n-1 recursions
            mce::thunk t = [&]{ f(new_cnt); };
            mce::coroutine c(t);

            while(!c.complete()) 
            { 
                c.run();
                mce::yield();
            }
        }
    };

    std::uint64_t count = 10;

    f(count);
}

TEST(coroutine, error_handling)
{
    bool test = false;
    bool test2 = false;
    bool test3 = false;
    bool test4 = false;
    mce::coroutine* parent_co = NULL;

    class co_test_error : public std::exception
    {
    public:
        virtual const char* what() const throw()
        {
            return "Arbitrary error";
        }
    } co_test_e;

    auto f = [&]
    {
        auto f2 = [&]
        {
            test = true;
            throw co_test_e;
            test2 = true;
        };

        parent_co = mce::this_coroutine();
        EXPECT_TRUE(parent_co!=NULL);

        mce::coroutine c2(f2);
        c2.run();

        test3 = true;
    };

    mce::coroutine c(f);

    try{ c.run(); }
    catch(...){ test4 = true; }

    EXPECT_TRUE(test);
    EXPECT_FALSE(test2);
    EXPECT_FALSE(test3);
    EXPECT_TRUE(test4);
    EXPECT_TRUE(parent_co!=NULL); // ensure this is not NULL
    EXPECT_TRUE(mce::this_coroutine()==NULL); // ensure this is NULL
}

TEST(coroutine, complete)
{
    uint64_t a = 5;
    uint64_t b = 10;
    uint64_t c;

    mce::thunk t = [&] () { c = a * b; };
    mce::coroutine coroutine (t);
    EXPECT_FALSE(coroutine.complete());

    coroutine.run();
    EXPECT_EQ(c, 50);
    EXPECT_TRUE(coroutine.complete());
}

TEST(coroutine, yield) 
{
    int c = 0;
    std::function<void(int,int)> f = [&c](int a, int b)
    { 
        c = a;
        mce::this_coroutine()->yield();
        c = b; 
    };

    mce::coroutine c1(mce::make_thunk(f,3,10));

    EXPECT_TRUE(c == 0);

    c1.run();
    EXPECT_TRUE(!c1.complete());
    EXPECT_TRUE(c == 3);

    c1.run();
    EXPECT_TRUE(c1.complete());
    EXPECT_TRUE(c == 10);
}

TEST(coroutine, make)
{
    bool ran = false;
    std::unique_ptr<mce::coroutine> c = mce::coroutine::make([](bool* r){ *r = true; }, &ran);
    EXPECT_FALSE(ran);
    c->run();
    EXPECT_TRUE(c->complete());
    EXPECT_TRUE(ran);
}

TEST(mce, in_coroutine)
{   
    EXPECT_FALSE(mce::in_coroutine());

    int c = 0;
    bool ran_tests = false;

    std::function<void(int,int)> f = [&c, &ran_tests](int a, int b)
    { 
        c = a;
        c = b; 

        EXPECT_TRUE(mce::in_coroutine());
        ran_tests = true;
    };

    mce::coroutine coroutine(mce::make_thunk(f,3,10));
    coroutine.run();
    
    EXPECT_TRUE(ran_tests);
    EXPECT_FALSE(mce::in_coroutine());
    EXPECT_EQ(c, 10);
}

TEST(mce, this_coroutine)
{   
    EXPECT_EQ(mce::this_coroutine(), nullptr);

    int c = 0;
    bool ran_tests = false;

    std::function<void(int)> f = [&c, &ran_tests](int a)
    { 
        c = a;

        EXPECT_NE(mce::this_coroutine(), nullptr);
        ran_tests = true;
    };

    mce::coroutine coroutine(mce::make_thunk(f,10));
    coroutine.run();
    
    EXPECT_TRUE(ran_tests);
    EXPECT_EQ(mce::this_coroutine(), nullptr);
    EXPECT_EQ(c, 10);
}

TEST(mce, yield)
{
    int a = 5;
    bool ran_tests = false;

    std::function<void(int, int, int)> f = [&a, &ran_tests] (int b, int c, int d)
    {
        a *= b;
        mce::yield();
        a *= c;
        mce::yield();
        a *= d;
        ran_tests = true;
    };

    mce::coroutine coroutine(mce::make_thunk(f, 5, 10, 50));

    coroutine.run();
    EXPECT_FALSE(ran_tests);
    coroutine.run();
    EXPECT_FALSE(ran_tests);
    coroutine.run();
    EXPECT_TRUE(ran_tests);
    EXPECT_EQ(a, 12500);
}
