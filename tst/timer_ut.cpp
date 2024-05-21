//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "function_utility.hpp"
#include "timer.hpp"
#include "mutex.hpp"
#include "condition_variable.hpp"
#include "chan.hpp"
#include "threadpool.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <mutex>

#include <iostream>

#define MINIMUM_MICRO 100

TEST(timer_service, timer_service_timer)
{
    mce::timer_service ts;

    // access timer service by reference
    std::thread thd([&]{ ts.start(); });

    ts.ready();

    int v = 0;
    mce::thunk f = [&v]{ v = 1; };

    EXPECT_TRUE(v == 0);

    // 100000 microseconds == 100 milliseconds == 0.1 second
    ts.timer(mce::time_unit::microsecond,100000, f);

    EXPECT_TRUE(v == 0);

    // sleep 200 milliseconds so timeout occurs 
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(v == 1);

    ts.shutdown();
    thd.join();
}

TEST(timer_service, timer_service_remove)
{
    mce::timer_service ts;
    std::thread thd([&]{ ts.start(); });
    ts.ready();

    int v = 0;
    mce::thunk f = [&v]{ v = 1; };

    EXPECT_TRUE(v == 0);

    // 10000000 micro == 10000 milli = 10 seconds
    mce::timer_id id = ts.timer(mce::time_unit::microsecond,10000000, f);

    EXPECT_TRUE(v == 0);

    ts.remove(id);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(v == 0);

    ts.shutdown();
    thd.join();
}

TEST(timer_service, timer_service_shutdown)
{
    mce::timer_service ts;
    std::thread thd([&]{ ts.start(); });
    ts.ready();
    ts.shutdown();
    thd.join();
}

TEST(timer_service, lots_of_timers)
{
    std::mutex mut;

    std::uint64_t shared_cnt = 0;
    std::uint64_t number_timers = 10 * 1000; // 10 thousand

    mce::timer_service ts;
    mce::thunk timer_func = [&]{ ts.start(); };
    std::thread thd(timer_func);
    ts.ready();

    std::uint64_t timeout_micro = MINIMUM_MICRO;
    mce::thunk timeout_handler = [&]
    {
        std::lock_guard<std::mutex> lk(mut);
        ++shared_cnt;
    };

    for(size_t i=0; i<number_timers; ++i)
    {
        ts.timer(mce::time_unit::microsecond,timeout_micro,timeout_handler);
    }

    bool cont = true;
    while(cont)
    {
        std::this_thread::yield();
        {
            std::lock_guard<std::mutex> lk(mut);
            if(shared_cnt == number_timers) { cont = false; }
        }
    }

    ts.shutdown();
    thd.join();

    EXPECT_EQ(shared_cnt,number_timers);
}

TEST(timer_service, recursive_timer_timers)
{
    std::mutex mut;

    std::uint64_t shared_cnt = 0;
    std::uint64_t number_timers = 100; 

    mce::timer_service ts;
    mce::thunk timer_func = [&]{ ts.start(); };
    std::thread thd(timer_func);
    ts.ready();

    std::uint64_t timeout_micro = MINIMUM_MICRO;
    mce::thunk timeout_handler = [&]
    {
        std::lock_guard<std::mutex> lk(mut);
        ++shared_cnt;
        if(shared_cnt<number_timers) 
        { 
            ts.timer(mce::time_unit::microsecond,timeout_micro, timeout_handler);
        }
    };

    ts.timer(mce::time_unit::microsecond,timeout_micro,timeout_handler);

    bool cont = true;
    while(cont)
    {
        {
            std::lock_guard<std::mutex> lk(mut);
            if(shared_cnt == number_timers) { cont = false; }
        }
        std::this_thread::yield(); // yield to timer thread
    }

    ts.shutdown();
    thd.join();

    EXPECT_EQ(shared_cnt,number_timers);
}

TEST(timer_service, timer_move_thunk_variant)
{
    mce::timer_service ts;
    std::thread thd([&]{ ts.start(); });
    ts.ready();

    int x {10};

    ts.timer(mce::time_unit::millisecond, 100, [&] () { x *= x; });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ts.shutdown();
    thd.join();

    EXPECT_EQ(x, 100);
}

TEST(timer_service, timer_thunk_reference_variant)
{
    mce::timer_service ts;
    std::thread thd([&]{ ts.start(); });
    ts.ready();

    int x {10};
    mce::thunk execute = [&] () { x *= x; };

    ts.timer(mce::time_unit::millisecond, 100, execute);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ts.shutdown();
    thd.join();

    EXPECT_EQ(x, 100);
}

TEST(timer_service, timer_duration_move_thunk_variant)
{
    mce::timer_service ts;
    std::thread thd([&]{ ts.start(); });
    ts.ready();

    int x {10};
    std::chrono::nanoseconds time {10000};

    ts.timer(time, [&]() { x *= x; });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ts.shutdown();
    thd.join();

    EXPECT_EQ(x, 100);
}

TEST(timer_service, timer_duration_thunk_reference_variant)
{

    mce::timer_service ts;
    std::thread thd([&]{ ts.start(); });
    ts.ready();

    int x {10};
    std::chrono::nanoseconds time {10000};
    mce::thunk execute = [&] () { x *= x; };

    ts.timer(time, execute);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ts.shutdown();
    thd.join();

    EXPECT_EQ(x, 100);
}

TEST(timer_service, timer_time_point_move_thunk_variant)
{
    mce::timer_service ts;
    std::thread thd([&]{ ts.start(); });
    ts.ready();

    int x {10};
    mce::duration time = std::chrono::seconds (1);
    mce::time_point start (time);

    ts.timer(start, [&]() { x *= x; });
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    ts.shutdown();
    thd.join();

    EXPECT_EQ(x, 100);
}

TEST(timer_service, timer_time_point_thunk_reference_variant)
{
    mce::timer_service ts;
    std::thread thd([&]{ ts.start(); });
    ts.ready();

    int x {10};
    mce::duration time = std::chrono::seconds (1);
    mce::time_point start (time);
    mce::thunk execute = [&]() { x *= x; };

    ts.timer(start, execute);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    ts.shutdown();
    thd.join();

    EXPECT_EQ(x, 100);
}

TEST(timer_service, count)
{
    mce::timer_service ts;
    std::thread thd([&]{ ts.start(); });
    ts.ready();

    for(int i=0; i<10; ++i)
    {
        ts.timer(mce::time_unit::millisecond, 100, [](){ });
    }

    EXPECT_EQ(ts.count(), 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ts.shutdown();
    thd.join();
    EXPECT_EQ(ts.count(), 0);
}

TEST(timer_service, clear)
{
    mce::timer_service ts;
    std::thread thd([&]{ ts.start(); });
    ts.ready();

    int x {10};

    ts.timer(mce::time_unit::millisecond, 100, [&] () { x *= x; });
    ts.clear();

    ts.shutdown();
    thd.join();

    EXPECT_EQ(x, 10);  
}

TEST(timer_service, default_timer_service)
{
    int x {10};
    mce::thunk execute = [&]() { x *= x; };
    mce::default_timer_service().timer(mce::time_unit::millisecond, 100, execute);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(x, 100);   
}

TEST(timer_service, default_timer_service_no_start)
{
    int x {10};
    mce::thunk execute = [&]() { x *= x; };
    mce::default_timer_service().timer(mce::time_unit::millisecond, 100, execute);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(x, 100);   
}

TEST(timer_service, default_timer_service_multiple_timers)
{
    int x{0};
    size_t num_timers{100};
    size_t count{0};
    size_t failure_count = 0;
    mce::timer_id tid;
    size_t target_runtime = 100;
    mce::time_point timer_start = mce::current_time();
    size_t error_runtime = 3 * target_runtime;

    std::function<void()> timeout = [&]
    { 
        x += 10; 
        auto actual_runtime = mce::get_time_point_difference(mce::time_unit::millisecond, timer_start, mce::current_time());

        EXPECT_TRUE(actual_runtime < error_runtime);
        if(actual_runtime >= error_runtime)
        {
            ++failure_count;
            std::cout << "timer runtime: " << actual_runtime << std::endl;
        }

        if(count < num_timers)
        {
            ++count;
            timer_start = mce::current_time();
            mce::default_timer_service().timer(
                    mce::time_unit::millisecond, 
                    target_runtime,
                    timeout);
        }
    };
    
    ++count;
    mce::default_timer_service().timer(mce::time_unit::millisecond, 
                                        target_runtime,
                                        timeout);
    
    auto projected_max_runtime = num_timers * 200;
    std::this_thread::sleep_for(std::chrono::milliseconds(projected_max_runtime));
    EXPECT_EQ(count, num_timers);
    EXPECT_EQ(x, 1000);   
    EXPECT_EQ(failure_count, 0);
}

TEST(timer_service, get_duration)
{
    std::chrono::seconds time (1);
    mce::duration dura = mce::get_duration(mce::time_unit::second, 1);
    EXPECT_EQ(dura, time);
}

TEST(timer_service, get_time_point_difference)
{
    mce::duration dura1 = std::chrono::seconds(1);
    mce::duration dura2 = std::chrono::seconds(2);
    mce::time_point tp1 (dura1);
    mce::time_point tp2 (dura2);
    uint64_t dif = mce::get_time_point_difference(mce::time_unit::second, tp1, tp2);
    uint64_t num = 1;

    EXPECT_EQ(dif, num);
}

TEST(timer, timers_const_thunk_time_unit)
{
    mce::mutex mtx;
    bool done = false;
    const mce::thunk f = [&]{ std::unique_lock<mce::mutex> lk(mtx); done = true; };
    mce::timer(mce::time_unit::millisecond, 50, f);

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    std::unique_lock<mce::mutex> lk(mtx);

    EXPECT_TRUE(done);
}

TEST(timer, timers_const_thunk_duration)
{
    mce::mutex mtx;
    bool done = false;
    const mce::thunk f = [&]{ std::unique_lock<mce::mutex> lk(mtx); done = true; };
    mce::duration d = mce::get_duration(mce::time_unit::millisecond, 50);
    mce::timer(d, f);

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    std::unique_lock<mce::mutex> lk(mtx);

    EXPECT_TRUE(done);
}

TEST(timer, timers_const_thunk_time_point)
{
    mce::mutex mtx;
    bool done = false;
    const mce::thunk f = [&]{ std::unique_lock<mce::mutex> lk(mtx); done = true; };
    mce::duration d = mce::get_duration(mce::time_unit::millisecond, 50);
    mce::time_point tp = mce::current_time() + d;
    mce::timer(tp, f);

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    std::unique_lock<mce::mutex> lk(mtx);

    EXPECT_TRUE(done);
}

TEST(timer, timers_non_thunk_time_unit)
{
    mce::mutex mtx;
    bool done = false;
    std::function<bool()> f = [&]
    { 
        std::unique_lock<mce::mutex> lk(mtx); 
        done = true; 
        return true;
    };
    mce::timer(mce::time_unit::millisecond, 50, f);

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    std::unique_lock<mce::mutex> lk(mtx);

    EXPECT_TRUE(done);
}

TEST(timer, timers_non_thunk_duration)
{
    mce::mutex mtx;
    bool done = false;
    std::function<bool()> f = [&]
    { 
        std::unique_lock<mce::mutex> lk(mtx); 
        done = true; 
        return true;
    };
    mce::duration d = mce::get_duration(mce::time_unit::millisecond, 50);
    mce::timer(d, f);

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    std::unique_lock<mce::mutex> lk(mtx);

    EXPECT_TRUE(done);
}

TEST(timer, timers_non_thunk_time_point)
{
    mce::mutex mtx;
    bool done = false;
    std::function<bool()> f = [&]
    { 
        std::unique_lock<mce::mutex> lk(mtx); 
        done = true; 
        return true;
    };
    mce::duration d = mce::get_duration(mce::time_unit::millisecond, 50);
    mce::time_point tp = mce::current_time() + d;
    mce::timer(tp, f);

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    std::unique_lock<mce::mutex> lk(mtx);

    EXPECT_TRUE(done);
}

TEST(timer, timers_func_with_args_time_unit)
{
    mce::mutex mtx;
    bool done = false;
    std::function<bool(int)> f = [&](int i)
    { 
        std::unique_lock<mce::mutex> lk(mtx); 
        done = true; 
        return true;
    };
    mce::timer(mce::time_unit::millisecond, 50, f, 3);

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    std::unique_lock<mce::mutex> lk(mtx);

    EXPECT_TRUE(done);
}

TEST(timer, timers_func_with_args_duration)
{
    mce::mutex mtx;
    bool done = false;
    std::function<bool(int)> f = [&](int i)
    { 
        std::unique_lock<mce::mutex> lk(mtx); 
        done = true; 
        return true;
    };
    mce::duration d = mce::get_duration(mce::time_unit::millisecond, 50);
    mce::timer(d, f, 3);

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    std::unique_lock<mce::mutex> lk(mtx);

    EXPECT_TRUE(done);
}

TEST(timer, timers_func_with_args_time_point)
{
    mce::mutex mtx;
    bool done = false;
    std::function<bool(int)> f = [&](int i)
    { 
        std::unique_lock<mce::mutex> lk(mtx); 
        done = true; 
        return true;
    };
    mce::duration d = mce::get_duration(mce::time_unit::millisecond, 50);
    mce::time_point tp = mce::current_time() + d;
    mce::timer(tp, f, 3);

    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    std::unique_lock<mce::mutex> lk(mtx);

    EXPECT_TRUE(done);
}

#define MINIMUM_MICRO 100

TEST(timer, lots_of_timers)
{
    mce::mutex mut;
    mce::condition_variable cv;

    uint64_t shared_cnt = 0;
    uint64_t number_timers = 10 * 1000; // 10 thousand
    uint64_t timeout_micro = MINIMUM_MICRO;

    mce::thunk timeout_handler = [&]
    {
        {
            std::unique_lock<mce::mutex> lk(mut);
            ++shared_cnt;
        }
        cv.notify_one();
    };

    for(size_t i=0; i<number_timers; ++i)
    {
        mce::timer(mce::time_unit::microsecond,timeout_micro,timeout_handler);
    }

    std::unique_lock<mce::mutex> lk(mut);
    while(shared_cnt != number_timers){ cv.wait(lk); }

    EXPECT_EQ(shared_cnt,number_timers);
}

TEST(timer, recursive_timer_timers)
{
    mce::mutex mut;
    mce::condition_variable cv;

    uint64_t shared_cnt = 0;
    uint64_t number_timers = 50; 

    uint64_t timeout_micro = MINIMUM_MICRO;
    mce::thunk timeout_handler = [&]
    {
        {
            std::lock_guard<mce::mutex> lk(mut);
            ++shared_cnt;
            if(shared_cnt<number_timers) 
            { 
                mce::timer(mce::time_unit::microsecond, timeout_micro, timeout_handler);
            }
        }
        cv.notify_one();
    };

    std::unique_lock<mce::mutex> lk(mut);
    mce::timer(mce::time_unit::microsecond,timeout_micro,timeout_handler);
    while(shared_cnt != number_timers){ cv.wait(lk); }

    EXPECT_EQ(shared_cnt,number_timers);
}

TEST(timer, timer_move_thunk_variant)
{
    int x {10};
    mce::timer(mce::time_unit::millisecond, 100, [&](){ x *= x; });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(x, 100);
}

TEST(timer, timer_thunk_reference_variant)
{
    int x {10};
    mce::thunk execute = [&] () { x *= x; };

    mce::timer(mce::time_unit::millisecond, 100, execute);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(x, 100);
}

TEST(timer, timer_duration_move_thunk_variant)
{
    int x {10};
    std::chrono::nanoseconds time {10000};

    mce::timer(time, [&](){ x *= x; });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(x, 100);
}

TEST(timer, timer_duration_thunk_reference_variant)
{
    int x {10};
    std::chrono::nanoseconds time {10000};
    mce::thunk execute = [&](){ x *= x; };

    mce::timer(time, execute);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(x, 100);
}

TEST(timer, timer_time_point_move_thunk_variant)
{
    int x {10};
    mce::duration time = std::chrono::seconds (1);
    mce::time_point start (time);

    mce::timer(start, [&](){ x *= x; });
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_EQ(x, 100);
}

TEST(timer, timer_time_point_thunk_reference_variant)
{
    int x {10};
    mce::duration time = std::chrono::seconds (1);
    mce::time_point start (time);
    mce::thunk execute = [&]() { x *= x; };

    mce::timer(start, execute);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_EQ(x, 100);
}

TEST(timer, count_timers)
{
    for(int i=0; i<10; ++i)
    {
        mce::timer(mce::time_unit::millisecond, 100, [](){ });
    }
    EXPECT_EQ(mce::count_timers(), 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(mce::count_timers(), 0);
}

TEST(timer, remove_timer)
{
    int x {10};
    mce::timer(mce::time_unit::millisecond, 100, [&](){ x *= x; });
    mce::timer_id tid = timer(mce::time_unit::millisecond, 100, [&](){ x *= x; });
    EXPECT_EQ(mce::count_timers(), 2);
    EXPECT_TRUE(mce::remove_timer(tid));
    EXPECT_EQ(mce::count_timers(), 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(x, 100);
    EXPECT_EQ(mce::count_timers(), 0);
}

TEST(timer, clear_timers)
{
    int x {10};
    for(int i=0; i<1000; ++i)
    {
        mce::timer(mce::time_unit::millisecond, 200, [&](){ x *= x; });
    }

    mce::default_timer_service().clear();
    EXPECT_EQ(mce::count_timers(), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(x, 10);  
}

TEST(sleep, sleep_time_unit)
{
    mce::time_point start_tp = mce::current_time();
    EXPECT_TRUE(mce::sleep(mce::time_unit::millisecond, 1000));
    mce::time_point end_tp = mce::current_time();

    size_t dif = mce::get_time_point_difference(mce::time_unit::millisecond, start_tp, end_tp);
    EXPECT_TRUE(dif >= 1000);
    EXPECT_TRUE(dif < 1200);
}

TEST(sleep, sleep_time_unit_clear)
{
    mce::time_point start_tp = mce::current_time();

    mce::parallel([&]
    {
        EXPECT_TRUE(mce::sleep(mce::time_unit::millisecond, 500));
        mce::default_timer_service().clear();
    });

    EXPECT_FALSE(mce::sleep(mce::time_unit::millisecond, 1000));
    mce::time_point end_tp = mce::current_time();

    size_t dif = mce::get_time_point_difference(mce::time_unit::millisecond, start_tp, end_tp);
    EXPECT_TRUE(dif < 1000);
}

TEST(sleep, sleep_duration)
{
    mce::time_point start_tp = mce::current_time();
    mce::duration d = mce::get_duration(mce::time_unit::millisecond, 1000);
    EXPECT_TRUE(mce::sleep(d));
    mce::time_point end_tp = mce::current_time();

    size_t dif = mce::get_time_point_difference(mce::time_unit::millisecond, start_tp, end_tp);
    EXPECT_TRUE(dif >= 1000);
    EXPECT_TRUE(dif < 1200);
}

TEST(sleep, sleep_duration_clear)
{
    mce::time_point start_tp = mce::current_time();
    mce::duration d = mce::get_duration(mce::time_unit::millisecond, 1000);

    mce::parallel([&]
    {
        auto d = mce::get_duration(mce::time_unit::millisecond, 500);
        EXPECT_TRUE(mce::sleep(d));
        mce::default_timer_service().clear();
    });

    EXPECT_FALSE(mce::sleep(d));
    mce::time_point end_tp = mce::current_time();

    size_t dif = mce::get_time_point_difference(mce::time_unit::millisecond, start_tp, end_tp);
    EXPECT_TRUE(dif < 1000);
}

TEST(sleep, sleep_time_unit_coroutine)
{
    mce::time_point start_tp;

    mce::chan<bool> ch = mce::chan<bool>::make();

    {
        start_tp = mce::current_time();

        mce::parallel([&,ch]
        {
            bool success = mce::sleep(mce::time_unit::millisecond, 1000);
            ch.send(success);
        });

        bool r;
        ch.recv(r);
        size_t dif = mce::get_time_point_difference(
            mce::time_unit::millisecond, 
            start_tp, 
            mce::current_time());

        EXPECT_TRUE(r);
        EXPECT_TRUE(dif >= 1000);
        EXPECT_TRUE(dif < 1200);
    }

    {
        start_tp = mce::current_time();

        mce::parallel([&,ch]
        {
            bool success = mce::sleep(mce::time_unit::millisecond, 1000);
            ch.send(success);
        });

        mce::sleep(mce::time_unit::millisecond, 500);
        mce::default_timer_service().clear();

        bool r;
        ch.recv(r);
        size_t dif = mce::get_time_point_difference(
            mce::time_unit::millisecond, 
            start_tp, 
            mce::current_time());

        EXPECT_FALSE(r);
        EXPECT_TRUE(dif < 1000);
    }
}

TEST(sleep, sleep_duration_coroutine)
{
    mce::time_point start_tp;
    mce::duration d;

    mce::chan<bool> ch = mce::chan<bool>::make();

    {
        start_tp = mce::current_time();

        mce::parallel([&,ch]
        {
            d = mce::get_duration(mce::time_unit::millisecond, 1000);
            bool success = mce::sleep(d);
            ch.send(success);
        });

        bool r;
        ch.recv(r);
        size_t dif = mce::get_time_point_difference(
            mce::time_unit::millisecond, 
            start_tp, 
            mce::current_time());

        EXPECT_TRUE(r);
        EXPECT_TRUE(dif >= 1000);
        EXPECT_TRUE(dif < 1200);
    }

    {
        start_tp = mce::current_time();

        mce::parallel([&,ch]
        {
            d = mce::get_duration(mce::time_unit::millisecond, 1000);
            bool success = mce::sleep(d);
            ch.send(success);
        });

        mce::sleep(mce::time_unit::millisecond, 500);
        mce::default_timer_service().clear();

        bool r;
        ch.recv(r);
        size_t dif = mce::get_time_point_difference(
            mce::time_unit::millisecond, 
            start_tp, 
            mce::current_time());

        EXPECT_FALSE(r);
        EXPECT_TRUE(dif < 1000);
    }
}
