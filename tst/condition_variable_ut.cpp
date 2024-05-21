//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "function_utility.hpp"
#include "scheduler.hpp"
#include "mutex.hpp"
#include "condition_variable.hpp"

#include <mutex>
#include <thread>
#include <queue>

#include <gtest/gtest.h>

TEST(condition_variable, condition_variable_wait_notify_one) 
{
    mce::mutex mut;
    mce::condition_variable cv;
    mce::condition_variable test_cv;
    auto cs = mce::scheduler::make();
    std::thread thd([cs]{ cs->run(); });

    int v = 0;

    bool co_flag=true;
    
    std::unique_lock<mce::mutex> lk(mut);

    cs->schedule([&]
    {
        std::unique_lock<mce::mutex> lk(mut);
        while(co_flag){ cv.wait(lk); }
        co_flag=true;
        test_cv.notify_one();
        v = 1;
    });

    cs->schedule([&]
    {
        std::unique_lock<mce::mutex> lk(mut);
        while(co_flag){ cv.wait(lk); }
        co_flag=true;
        test_cv.notify_one();
        v = 2;
    });

    EXPECT_TRUE(v == 0);
  
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    co_flag=false;
    cv.notify_one();
    test_cv.wait(lk);

    int last_v = v;
    EXPECT_TRUE( v==1 || v==2 );

    co_flag=false;
    cv.notify_one();
    test_cv.wait(lk);

    EXPECT_TRUE( (v==1 || v==2) && v!=last_v);

    cs->halt();
    thd.join();
}

TEST(condition_variable, condition_variable_wait_notify_all) 
{
    mce::mutex mut;
    mce::condition_variable cv;
    mce::condition_variable test_cv;
    mce::condition_variable test_cv2;
    auto cs = mce::scheduler::make();
    std::thread thd([cs]{ cs->run(); });

    int v1 = 0;
    int v2 = 0;
    bool co_flag = true;
    bool flag1 = true;
    bool flag2 = true;

    cs->schedule([&]
    {
        std::unique_lock<mce::mutex> lk(mut);
        while(co_flag){ cv.wait(lk); }
        test_cv.notify_one();
        flag1 = false;
        v1 = 1;
    });

    cs->schedule([&]
    {
        std::unique_lock<mce::mutex> lk(mut);
        while(co_flag){ cv.wait(lk); }
        test_cv.notify_one();
        flag2 = false;
        v2 = 2;
    });

    cs->schedule([&]
    {
        std::unique_lock<mce::mutex> lk(mut);
        while(flag1 || flag2){ test_cv.wait(lk); }
        test_cv2.notify_one();
    });

   
    std::unique_lock<mce::mutex> lk(mut);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    co_flag = false;
    EXPECT_TRUE(v1 == 0);
    EXPECT_TRUE(v2 == 0);
    cv.notify_all();
    test_cv2.wait(lk);

    EXPECT_EQ(v1,1);
    EXPECT_EQ(v2,2);

    cs->halt();
    thd.join();
}


/*
 A thread and a coroutine running at the same time, but the coroutine has to wait
to be notified by the thread before it is able to execute it's task
*/
TEST(condition_variable, condition_variable_wait_for_notify_one_v1)
{
    mce::mutex mut;
    mce::condition_variable cv;

    int num {0};
    const int sleep{50};
    bool flag = true;

    mut.lock();

    auto func1 = [&] (int calc) 
    { 
        std::unique_lock<mce::mutex> lk (mut);
        num += calc; 
        flag = false;
        cv.notify_one();
    };

    mce::thunk th1 = [&]
    {
        std::unique_lock<mce::mutex> lk (mut);
        while(flag)
        { 
            EXPECT_EQ(
                std::cv_status::no_timeout,
                cv.wait_for(lk, std::chrono::milliseconds(sleep))); 
        }
        num += 100;
    };

    mce::coroutine coro(th1);
    std::thread thr(func1, 50);
    coro.run();

    EXPECT_EQ(num, 0);
    EXPECT_FALSE(coro.complete());

    mut.unlock();

    while(!coro.complete()){ coro.run(); }
    EXPECT_TRUE(coro.complete());
    thr.join();
    EXPECT_EQ(num, 150);
}

/*
Two threads are running at the same time, one thread is waiting to be notified before it is able to run
*/
TEST(condition_variable, condition_variable_wait_for_notify_one_v2)
{
    mce::mutex mut;
    mce::condition_variable cv;

    int num {0};
    const int sleep {100};
    bool val1 {false};
    bool val2 {false};
    bool flag {true};

    auto func1 = [&] ()
    {
        std::unique_lock<mce::mutex> lk(mut);
        num = 50;
        val1 = true;
        flag = false;
        cv.notify_one();
    };

    auto func2 = [&] ()
    {
        std::unique_lock<mce::mutex> lk(mut);
        while(flag){ cv.wait_for(lk, std::chrono::milliseconds(sleep)); }
        num = 100;
        val2 = true;
    };

    std::thread thd1(func1);
    std::thread thd2(func2);

    std::this_thread::sleep_for(std::chrono::milliseconds(sleep));

    thd1.join();
    thd2.join();

    EXPECT_TRUE(val1);
    EXPECT_TRUE(val2);
    EXPECT_EQ(num, 100);
}


/*
Two coroutines and a thread are waiting for an amount of time to be notified.
A code block is being used to trigger the notify_one, then the coroutines are 
run again. The sleep is used to allow the task to finish running so that the 
test passes in the correct order.
*/
TEST(condition_variable, condition_variable_wait_for_notify_one_v3)
{
    mce::mutex mut;
    mce::condition_variable cv;

    int num {0};
    bool flag = true;

    mce::thunk th = [&]
    {
        std::unique_lock<mce::mutex> lk(mut);
        while(flag){ cv.wait_for(lk, std::chrono::seconds(5)); }
        flag = true;
        ++num;
    };

    std::thread thd(th);
    mce::coroutine tst_coro1(th);
    mce::coroutine tst_coro2(th);

    EXPECT_FALSE(tst_coro1.complete());
    EXPECT_FALSE(tst_coro2.complete());
    EXPECT_EQ(num, 0);

    {
        std::unique_lock<mce::mutex> lk(mut);
        flag = false;
        cv.notify_one();
    }

    thd.join();  //forcing thread to join to have tests run in correct order
    tst_coro1.run();
    tst_coro2.run();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(tst_coro1.complete());
    EXPECT_FALSE(tst_coro2.complete());
    EXPECT_EQ(num, 1);

    {
        std::unique_lock<mce::mutex> lk(mut);
        flag = false;
        cv.notify_one();
    }

    tst_coro1.run();
    tst_coro2.run();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(tst_coro1.complete() || tst_coro2.complete());
    EXPECT_FALSE(tst_coro1.complete() && tst_coro2.complete());
    EXPECT_EQ(num, 2);

    {
        std::unique_lock<mce::mutex> lk(mut);
        flag = false;
        cv.notify_one();
    }

    if(tst_coro1.complete()){ tst_coro2.run(); }
    else{ tst_coro1.run(); }
    EXPECT_TRUE(tst_coro1.complete() && tst_coro2.complete());
    EXPECT_EQ(num, 3);
}

/* 
Five threads are all executing the same task at the same time waiting for the lock and a amount of time
then notifying all waiting threads
*/
TEST(condition_variable, condition_variable_wait_for_notify_all)
{
    mce::mutex mut;
    mce::condition_variable cv;

    int num {0};
    auto wait_time = std::chrono::milliseconds(50);

    auto add_one_func = [&] ()
    {
        std::unique_lock<mce::mutex> lk(mut);
        cv.wait_for(lk, wait_time);
        ++num;
        cv.notify_all();
    };

    std::thread thd1(add_one_func);
    std::thread thd2(add_one_func);
    std::thread thd3(add_one_func);
    std::thread thd4(add_one_func);
    std::thread thd5(add_one_func);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(num, 5);

    thd1.join();
    thd2.join();
    thd3.join();
    thd4.join();
    thd5.join();
}

TEST(condition_variable, condition_variable_wait_until_no_timeout)
{
    mce::mutex mut;
    mce::condition_variable cv;
    const auto duration = std::chrono::milliseconds(50);
    std::chrono::steady_clock::time_point clock = std::chrono::steady_clock::now() + duration;

    mce::thunk do_things = [&] ()
    {
        std::unique_lock<mce::mutex> lk(mut);
        EXPECT_EQ(std::cv_status::no_timeout, cv.wait_until(lk, clock));
    };

    mce::coroutine c(do_things);
    c.run();
    cv.notify_one();
    while(!c.complete()){ c.run(); }
}

TEST(condition_variable, condition_variable_wait_until_timeout)
{
    mce::mutex mut;
    mce::condition_variable cv;
    const auto duration = std::chrono::milliseconds(50);
    std::chrono::steady_clock::time_point clock = std::chrono::steady_clock::now() + duration;

    mce::thunk do_things = [&] ()
    {
        std::unique_lock<mce::mutex> lk(mut);
        EXPECT_EQ(std::cv_status::timeout, cv.wait_until(lk, clock));
    };

    mce::coroutine c(do_things);
    while(!c.complete()){ c.run(); }
}

/*
A thread is running a queue of 5000 coroutines, the task is waiting until 50 ms from the current clock time
to start the task, then notifying the next task, if the task is not finished it is re-added to the queue
*/
TEST(condition_variable, condition_variable_wait_until_notify_one)
{
    mce::mutex mut;
    mce::condition_variable cv;
    const auto duration = std::chrono::milliseconds(50);
    //Possible bug, Timer expects a "steady_clock" so even though the wait_until is a template
    //that can accept any kind of clock, when passed to timer it must be a steady_clock
    //It may be helpful to either change timer to be a template, or change wait_until to only accept one type
    std::chrono::steady_clock::time_point clock = std::chrono::steady_clock::now() + duration;
    std::queue<mce::coroutine> task_queue {};
    int num {0};

    mce::thunk do_things = [&] ()
    {
        std::unique_lock<mce::mutex> lk(mut);
        cv.wait_until(lk, clock);
        ++num;
        clock = std::chrono::steady_clock::now() + duration;
        cv.notify_one();
    };
    
    for(size_t i{0}; i < 5000; ++i)
    {
        task_queue.push(mce::coroutine(do_things));
    }

    auto running_func = [&] ()
    {
        while(!task_queue.empty())
        {
            auto task = std::move(task_queue.front());
            task.run();
            if(!task.complete())
            {
                task_queue.push(std::move(task));
            }
            task_queue.pop();
        }
    };

    std::thread running_thd(running_func);

    running_thd.join();
    EXPECT_EQ(num, 5000);
}

/*
Ten threads are running at the same time to wait until a specified amount of time then notify all
*/
TEST(condition_variable, condition_variable_wait_until_notify_all)
{
    mce::mutex mut;
    mce::condition_variable cv;
    const auto duration = std::chrono::milliseconds(50);
    std::chrono::steady_clock::time_point clock = std::chrono::steady_clock::now() + duration;
    int num {0};

    auto do_things = [&] (std::chrono::milliseconds time)
    {
        std::unique_lock<mce::mutex> lk(mut);
        cv.wait_until(lk, clock);
        ++num;
        clock = std::chrono::steady_clock::now() + time;
        cv.notify_all();
    };
    
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::thread th1(do_things, std::chrono::milliseconds(100));
    std::thread th2(do_things, std::chrono::milliseconds(200));
    std::thread th3(do_things, std::chrono::milliseconds(300));
    std::thread th4(do_things, std::chrono::milliseconds(400));
    std::thread th5(do_things, std::chrono::milliseconds(500));
    std::thread th6(do_things, std::chrono::milliseconds(600));
    std::thread th7(do_things, std::chrono::milliseconds(700));
    std::thread th8(do_things, std::chrono::milliseconds(800));
    std::thread th9(do_things, std::chrono::milliseconds(900));
    std::thread th10(do_things, std::chrono::milliseconds(1000));

    th1.join();
    th2.join();
    th3.join();
    th4.join();
    th5.join();
    th6.join();
    th7.join();
    th8.join();
    th9.join();
    th10.join();
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto difference = end - start;

    EXPECT_TRUE(difference >= std::chrono::milliseconds(50) && difference <= std::chrono::milliseconds(200));
    EXPECT_EQ(num, 10);
}

/*
Tests that a coroutine will not run until the set clock time in the wait_until
*/
TEST(condition_variable, wait_until)
{
    mce::mutex mut;
    mce::condition_variable cv;
    std::chrono::milliseconds duration(500);
    auto clock = std::chrono::steady_clock::now() + duration;

    mce::thunk th = [&] ()
    {
        std::unique_lock<mce::mutex> lk(mut);
        EXPECT_EQ(std::cv_status::timeout, cv.wait_until(lk, clock));
    };
    auto start = std::chrono::steady_clock::now();
    mce::coroutine coro(th);
    coro.run();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    auto end = std::chrono::steady_clock::now();
    auto difference = end - start;

    EXPECT_FALSE(coro.complete());

    coro.run();
    EXPECT_TRUE(coro.complete());
    EXPECT_TRUE(difference > duration);
    EXPECT_TRUE(difference <= std::chrono::milliseconds(2000));
}

TEST(condition_variable, wait_until_clear_timers)
{
}
