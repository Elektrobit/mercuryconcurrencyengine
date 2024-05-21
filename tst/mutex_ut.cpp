//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "function_utility.hpp"
#include "scheduler.hpp"
#include "mutex.hpp"

#include <mutex>
#include <thread>
#include <chrono>
#include <queue>

#include <gtest/gtest.h>

//Test Helper Function to Determine lock status
bool is_locked(mce::mutex& m)
{
   bool got_lock = m.try_lock();
   if(got_lock) { m.unlock(); }
   return !got_lock;
} 

TEST(mutex, mutex_try_lock) 
{
    mce::mutex mut;
    int v = 0;
    bool b = false;

    mut.lock();

    mce::thunk t = [&mut,&b,&v]
    {
        while(!mut.try_lock())
        {
            b = true;
            mce::yield();
        }
        b = false;
        v = 6;
        mut.unlock();
    };

    mce::coroutine c(t);

    EXPECT_TRUE(b == false);
    EXPECT_TRUE(v == 0);

    c.run();
    EXPECT_TRUE(!c.complete());
    EXPECT_TRUE(b == true);
    EXPECT_TRUE(v == 0);

    c.run();
    EXPECT_TRUE(!c.complete());
    EXPECT_TRUE(b == true);
    EXPECT_TRUE(v == 0);

    mut.unlock();
    c.run();
    EXPECT_TRUE(c.complete());
    EXPECT_TRUE(b == false);
    EXPECT_TRUE(v == 6);
}

TEST(mutex, mutex_lock) 
{
    mce::mutex mut;
    int v = 0;

    mce::thunk t0 = [&]
    {
        mut.lock();
        mce::yield();
        v = 17;
        mut.unlock();
    };

    mce::thunk t1 = [&]
    {
        mut.lock();
        v = 3;
        mut.unlock();
    };

    mce::thunk t2 = [&]
    { 
        std::unique_lock<mce::mutex> lk(mut); 
        v = 2;
    };

    mce::coroutine c0(t0);
    mce::coroutine c1(t1);
    mce::coroutine c2(t2);

    EXPECT_TRUE(v == 0);

    c0.run();
    EXPECT_TRUE(!c0.complete());
    EXPECT_TRUE(v == 0);

    c1.run();
    EXPECT_TRUE(!c1.complete());
    EXPECT_TRUE(v == 0);

    c2.run();
    EXPECT_TRUE(!c2.complete());
    EXPECT_TRUE(v == 0);

    c0.run();
    EXPECT_TRUE(c0.complete());
    EXPECT_TRUE(v == 17);

    c1.run();
    EXPECT_TRUE(c1.complete());
    EXPECT_TRUE(v == 3);

    c2.run();
    EXPECT_TRUE(c2.complete());
    EXPECT_TRUE(v == 2);
}


TEST(mutex, mutex_lock_co_thread) 
{
    mce::mutex mut;

    int v = 0;

    // block coroutine and thread from running
    mut.lock();

    mce::thunk t0 = [&]
    {
        std::unique_lock<mce::mutex> lk(mut);
        v = 1;
    };

    mce::thunk t1 = [&]
    {
        std::unique_lock<mce::mutex> lk(mut);
        v = 2;
    };

    mce::coroutine c0(t0);
    std::thread thr0(t1);

    // allow child thread to execute so that it is enqueued in
    // internal blocked queue first 
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(v == 0);

    c0.run();
    EXPECT_TRUE(!c0.complete());
    EXPECT_TRUE(v == 0);

    EXPECT_TRUE(v == 0);

    // allow child thread to acquire mutex
    mut.unlock();
    thr0.join();

    EXPECT_TRUE(v == 2);

    mut.lock();
    c0.run();
    EXPECT_TRUE(!c0.complete());
    EXPECT_TRUE(v == 2);

    mut.unlock();
    c0.run();
    EXPECT_TRUE(c0.complete());
    EXPECT_TRUE(v == 1);
}

TEST(mutex, thread_mutex_lock)
{
    mce::mutex mut;

    int a = 5;
    int b = 10;
    int c;
    bool done1 {false};
    bool done2 {false};

    std::function<void()> calc1 = [&] () 
    {
        mut.lock();
        EXPECT_TRUE(is_locked(mut));
        c = a + b;
        EXPECT_EQ(c, 15);
        done1 = true;
        mut.unlock();
    };

    std::function<void()> calc2 = [&] ()
    {
        mut.lock();
        EXPECT_TRUE(is_locked(mut));
        c = a * b;
        EXPECT_EQ(c, 50);
        done2 = true;
        mut.unlock();
    };

    std::thread thd2 (calc2);
    std::thread thd1 (calc1);

    thd1.join();
    thd2.join();

    EXPECT_TRUE(done1);
    EXPECT_TRUE(done2);
    EXPECT_FALSE(is_locked(mut));
}

TEST(mutex, thread_mutex_unlock)
{
    int a {5};
    int b;

    mce::mutex mut;

    std::function<void()> func = [&] () 
    { 
        std::unique_lock<mce::mutex> lk (mut);
        b = a * a; 
        EXPECT_TRUE(is_locked(mut));
    };

    std::thread thd (func);

    thd.join();

    EXPECT_FALSE(is_locked(mut));
    EXPECT_EQ(b, 25);
}

TEST(mutex, thread_mutex_try_lock_001)
{
    mce::mutex mut;
    bool test {false};
    mut.lock();

    std::function<void()> func = [&] ()
    {
        if(!mut.try_lock())
        {
            test = true;
        }
    };

    std::thread thd (func);
    thd.join();
    mut.unlock();

    EXPECT_TRUE(test);
}

TEST(mutex, thread_mutex_try_lock_002)
{
    int a {5};
    mce::mutex mut;
    bool test {false};

    std::function<void()> func = [&] () 
    {  
        std::unique_lock<mce::mutex> lk (mut, std::defer_lock);
        if(lk.try_lock())
        {
            a *= a;
           test = true;
        }
    };

    std::thread thd (func);
    thd.join();
    EXPECT_TRUE(test); 
    EXPECT_FALSE(is_locked(mut));
    EXPECT_EQ(a, 25);
}


TEST(mutex, coroutine_mutex_lock)
{
    int a {5};
    int b {2};
    mce::mutex mut;

    mce::thunk thu_a = [&]
    {
        mut.lock();
        a += a;
        mce::yield();
        a += a;
        mut.unlock();
        EXPECT_FALSE(is_locked(mut));
    };

    mce::thunk thu_b = [&]
    {
        mut.lock();
        b += b;
        mce::yield();
        b += b;
        mut.unlock();
        EXPECT_FALSE(is_locked(mut));
    };

    mce::coroutine coro_a (thu_a);
    mce::coroutine coro_b (thu_b);

    coro_a.run();
    coro_b.run();

    EXPECT_EQ(a, 10);
    EXPECT_EQ(b, 2);

    coro_a.run();

    EXPECT_EQ(a, 20);
    EXPECT_EQ(b, 2);
    EXPECT_TRUE(coro_a.complete());

    coro_b.run();

    EXPECT_EQ(b, 4);
    EXPECT_TRUE(is_locked(mut));

    coro_b.run();

    EXPECT_EQ(b, 8);
    EXPECT_FALSE(is_locked(mut));
    EXPECT_TRUE(coro_b.complete());
}

TEST(mutex, coroutine_mutex_unlock)
{
    int a {5};
    mce::mutex mut;

    mce::thunk thu = [&]
    {
        mut.lock();
        a *= a;
        mut.unlock();
    };

    mce::coroutine coro(thu);
    coro.run();

    EXPECT_EQ(a, 25);
    EXPECT_TRUE(coro.complete());
    EXPECT_FALSE(is_locked(mut));
}

TEST(mutex, coroutine_mutex_try_lock)
{
    int a {5};
    bool test_complete {false};
    mce::mutex mut;
    mut.lock();

    mce::thunk thu = [&]
    {
        bool success = mut.try_lock();
        ASSERT_FALSE(success);

        mce::yield();

        success = mut.try_lock();
        a *= a;
        test_complete = true;
        ASSERT_TRUE(success);

        mut.unlock();
    };

    mce::coroutine coro (thu);
    coro.run();

    EXPECT_EQ(a, 5);
    EXPECT_TRUE(is_locked(mut));

    mut.unlock();
    coro.run();

    EXPECT_TRUE(coro.complete());
    EXPECT_EQ(a, 25);
    EXPECT_FALSE(is_locked(mut));
    EXPECT_TRUE(test_complete);
}


TEST(mutex, queue_thread_coro_mutex_interaction)
{
    int a {0};
    bool test_complete {false};
    mce::mutex mut_a;

    mce::thunk thu_a = [&]
    {
        std::unique_lock<mce::mutex> lk (mut_a);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ++a;
    };

    std::function<void()> func_a = [&] ()
    {
        int counter {0};
        size_t total_coro {500};
        std::queue<std::unique_ptr<mce::coroutine>> process_coro {};

        for(size_t i {0}; i < total_coro; i++)
        {
            process_coro.emplace(new mce::coroutine(thu_a));
        }

        while(!process_coro.empty())
        { 
            auto c = std::move(process_coro.front());
            process_coro.pop();

            c->run();
            if(!c->complete())
            {
                process_coro.push(std::move(c));
                ++counter;
            }
        }
        EXPECT_NE(counter, 0);
        test_complete = true;
    };

    std::thread thd_1 (func_a);
    std::thread thd_2 (func_a);
    thd_1.join();
    thd_2.join();

    EXPECT_FALSE(is_locked(mut_a));
    EXPECT_TRUE(test_complete);
    //EXPECT_EQ(a, 10000);
    EXPECT_EQ(a, 1000);
}
