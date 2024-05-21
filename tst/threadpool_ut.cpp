//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include <chrono>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <thread>
#include <iostream>

#include "function_utility.hpp"
#include "timer.hpp"
#include "threadpool.hpp"
#include "unbuffered_channel.hpp"
#include "buffered_channel.hpp"
#include "chan.hpp"
#include "fptr.hpp"

// test only
#include <gtest/gtest.h> 

#include <iostream>

TEST(threadpool, schedule)
{
    auto tp = mce::threadpool::make(1);

    // create 1000 tasks all competing for the same channel, causing enormous
    // contention
    const int num_tasks = 10;

    mce::buffered_channel<int> done_ch;
    done_ch.construct();

    mce::thunk out_th = [&done_ch]
    {
        for(int c=0; c<1000000; ++c) { }
        done_ch.send(0);
    };

    for(int c=0; c<num_tasks; ++c) { tp->worker().schedule(out_th); }

    int r;
    for(int c=0; c<num_tasks; ++c) { done_ch.recv(r); }

    tp->halt();
}

TEST(threadpool, schedule_ignore_return)
{
    auto tp = mce::threadpool::make(1);

    // create 1000 tasks all competing for the same channel, causing enormous
    // contention
    const int num_tasks = 10;

    mce::buffered_channel<int> done_ch;
    done_ch.construct();

    mce::thunk out_th = [&done_ch]
    {
        for(int c=0; c<1000000; ++c) { }
        done_ch.send(0);
        return 2;
    };

    for(int c=0; c<num_tasks; ++c) { tp->worker().schedule(out_th); }

    int r;
    for(int c=0; c<num_tasks; ++c) { done_ch.recv(r); }

    tp->halt();
}

TEST(threadpool, schedule_rvalue)
{
    auto tp = mce::threadpool::make(1);

    // create 1000 tasks all competing for the same channel, causing enormous
    // contention
    const int num_tasks = 10;

    mce::buffered_channel<int> done_ch;
    done_ch.construct();

    for(int c=0; c<num_tasks; ++c) 
    { 
        tp->worker().schedule([&done_ch]
        {
            for(int c=0; c<1000000; ++c) { }
            done_ch.send(0);
        }); 
    }

    int r;
    for(int c=0; c<num_tasks; ++c) { done_ch.recv(r); }

    tp->halt();
}

TEST(threadpool, schedule_rvalue_ignore_return)
{
    auto tp = mce::threadpool::make(1);

    // create 1000 tasks all competing for the same channel, causing enormous
    // contention
    const int num_tasks = 10;

    mce::buffered_channel<int> done_ch;
    done_ch.construct();

    for(int c=0; c<num_tasks; ++c) 
    { 
        tp->worker().schedule([&done_ch]
        {
            for(int c=0; c<1000000; ++c) { }
            done_ch.send(0);
            return 22;
        }); 
    }

    int r;
    for(int c=0; c<num_tasks; ++c) { done_ch.recv(r); }

    tp->halt();
}

TEST(threadpool, schedule_args)
{
    auto tp = mce::threadpool::make(1);

    // create 1000 tasks all competing for the same channel, causing enormous
    // contention
    const int num_tasks = 10;

    mce::buffered_channel<int> done_ch;
    done_ch.construct();

    auto out_th = [](mce::buffered_channel<int> done_ch)
    {
        for(int c=0; c<1000000; ++c) { }
        done_ch.send(0);
    };

    for(int c=0; c<num_tasks; ++c) { tp->worker().schedule(out_th,done_ch); }

    int r;
    for(int c=0; c<num_tasks; ++c) { done_ch.recv(r); }

    tp->halt();
}

TEST(threadpool, schedule_args_ignore_return)
{
    auto tp = mce::threadpool::make(1);

    // create 1000 tasks all competing for the same channel, causing enormous
    // contention
    const int num_tasks = 10;

    mce::buffered_channel<int> done_ch;
    done_ch.construct();

    auto out_th = [](mce::buffered_channel<int> done_ch)
    {
        for(int c=0; c<1000000; ++c) { }
        done_ch.send(0);
        return 64;
    };

    for(int c=0; c<num_tasks; ++c) { tp->worker().schedule(out_th,done_ch); }

    int r;
    for(int c=0; c<num_tasks; ++c) { done_ch.recv(r); }

    tp->halt();
}

TEST(threadpool, schedule_args_rvalue)
{
    auto tp = mce::threadpool::make(1);

    // create 1000 tasks all competing for the same channel, causing enormous
    // contention
    const int num_tasks = 10;

    mce::buffered_channel<int> done_ch;
    done_ch.construct();

    for(int c=0; c<num_tasks; ++c) 
    { 
        tp->worker().schedule([](mce::buffered_channel<int> done_ch)
        {
            for(int c=0; c<1000000; ++c) { }
            done_ch.send(0);
        }, done_ch); 
    }

    int r;
    for(int c=0; c<num_tasks; ++c) { done_ch.recv(r); }

    tp->halt();
}

TEST(threadpool, schedule_args_rvalue_ignore_return)
{
    auto tp = mce::threadpool::make(1);

    // create 1000 tasks all competing for the same channel, causing enormous
    // contention
    const int num_tasks = 10;

    mce::buffered_channel<int> done_ch;
    done_ch.construct();

    for(int c=0; c<num_tasks; ++c) 
    { 
        tp->worker().schedule([](mce::buffered_channel<int> done_ch)
        {
            for(int c=0; c<1000000; ++c) { }
            done_ch.send(0);
            return 128;
        }, done_ch); 
    }

    int r;
    for(int c=0; c<num_tasks; ++c) { done_ch.recv(r); }

    tp->halt();
}

TEST(threadpool, schedule_fptr)
{
    reset_fptr_vals();
    int x=0;

    auto tp = mce::threadpool::make(2);

    fptr::x_ = 3;
    tp->worker().schedule(&(fptr::f));
    fptr::ch_.recv(x);
    EXPECT_EQ(x,6);

    fptr::x_ = 4;
    tp->worker().schedule(&(fptr::f2),4);
    fptr::ch_.recv(x);
    EXPECT_EQ(x,16);

    fptr_void::x_ = 1;
    tp->worker().schedule(&(fptr_void::f));
    fptr_void::ch_.recv(x);
    EXPECT_EQ(x,2);

    fptr_void::x_ = 2;
    tp->worker().schedule(&(fptr_void::f2),2);
    fptr_void::ch_.recv(x);
    EXPECT_EQ(x,4);

    tp->halt();
}

TEST(threadpool, schedule_communication)
{
    // create 1000 tasks all competing for the same channel, causing enormous
    // contention
    const int num_tasks = 1000;
    int ret = 0;

    {
        auto tp = mce::threadpool::make(1);

        mce::buffered_channel<int> done_ch;
        mce::unbuffered_channel<int> out_ch;
        done_ch.construct();
        out_ch.construct();

        mce::thunk out_th = [=]
        {
            int x;
            int c = 0;
            for(; c<num_tasks; ++c) { done_ch.recv(x); }
            out_ch.send(c);
        };

        tp->worker().schedule(out_th);

        for(int c=0; c<num_tasks; ++c)
        {
            mce::unbuffered_channel<int> ch;
            ch.construct();

            mce::thunk th0 = [ch]{ ch.send(0); };

            mce::thunk th1 = [ch,done_ch]
            { 
                int x; 
                ch.recv(x); 
                done_ch.send(0);
            };

            tp->worker().schedule(th0);
            tp->worker().schedule(th1);
        }

        out_ch.recv(ret);

        tp->halt();
    }

    EXPECT_EQ(ret, num_tasks);
}

TEST(threadpool, schedule_competing)
{
    // create 1000 tasks all competing for the same channel, causing enormous
    // contention
    const int num_tasks = 1000;
    int ret = 0;

    {
        auto tp = mce::threadpool::make(1);

        mce::unbuffered_channel<int> co_ch;
        mce::unbuffered_channel<int> out_ch;
        co_ch.construct();
        out_ch.construct();

        mce::thunk co_th = [co_ch]{ co_ch.send(0); };
        mce::thunk out_th = [=]
        {
            int c = 0;
            int ret = 0;
            while(c<num_tasks) 
            { 
                co_ch.recv(ret); 
                ++c;
            }

            out_ch.send(c);
        };

        // insert out_th first so execution can begin while we are still adding 
        // tasks
        tp->worker().schedule(out_th);

        // enqueue all our competing tasks
        for(int c = 0; c<num_tasks; ++c){ tp->worker().schedule(co_th); }

        out_ch.recv(ret);

        tp->halt();
    }

    EXPECT_EQ(ret, num_tasks);
}

TEST(threadpool, this_threadpool)
{
    std::shared_ptr<mce::threadpool> ctp=nullptr;

    mce::chan<std::shared_ptr<mce::threadpool>> ch = mce::chan<std::shared_ptr<mce::threadpool>>::make();
    auto get_ctp = [ch]{ ch.send(mce::this_threadpool()); };

    auto tp1 = mce::threadpool::make(1);
    auto tp2 = mce::threadpool::make(2);

    EXPECT_EQ(ctp.get(),nullptr);

    tp1->worker().schedule(get_ctp);
    ch.recv(ctp);
    EXPECT_NE(ctp.get(),nullptr);

    ctp->worker().schedule(get_ctp);
    std::shared_ptr<mce::threadpool> r;
    ch.recv(r);
    EXPECT_EQ(ctp.get(), r.get());

    tp2->worker().schedule(get_ctp);
    ch.recv(r);
    EXPECT_NE(ctp.get(),r.get());

    tp1->halt();
    tp2->halt();
}

TEST(threadpool, worker_and_workers)
{
    size_t size = 8;
    auto tp = mce::threadpool::make(size);

    auto wcs = &(tp->worker());
    auto wcs_vec = tp->workers();

    EXPECT_EQ(wcs_vec.size(), size);
    EXPECT_EQ(tp->size(), size);

    bool found_worker = false;
    for(auto& cs : wcs_vec)
    {
        if(wcs == cs.get())
        {
            found_worker=true;
            break;
        }
    }

    for(size_t i=0; i<wcs_vec.size(); ++i)
    {
        EXPECT_EQ(&(tp->worker(i)), wcs_vec[i].get());
    }

    tp->halt();

    EXPECT_TRUE(found_worker);
}

TEST(threadpool, suspend_resume) 
{
    auto tp = mce::threadpool::make(4);
    auto workers = tp->workers();

    EXPECT_EQ(4, tp->size());
    EXPECT_EQ(4, workers.size());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(mce::lifecycle::state::running, tp->get_state());

    for(auto w : workers) 
    {
        EXPECT_EQ(mce::lifecycle::state::running, w->get_state());
    }

    EXPECT_EQ(true, tp->suspend());

    EXPECT_EQ(mce::lifecycle::state::suspended, tp->get_state());

    for(auto w : workers) 
    {
        EXPECT_EQ(mce::lifecycle::state::suspended, w->get_state());
    }

    auto do_nothing = []{}; 

    tp->resume();

    for(auto w : workers) 
    {
        w->schedule(do_nothing);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(mce::lifecycle::state::running, tp->get_state());

    for(auto w : workers) 
    {
        EXPECT_EQ(mce::lifecycle::state::running, w->get_state());
    }

    tp->halt();
}

TEST(threadpool, suspend_resume_from_child) 
{
    auto tp = mce::threadpool::make(4);
    auto workers = tp->workers();

    EXPECT_EQ(4, tp->size());
    EXPECT_EQ(4, workers.size());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(mce::lifecycle::state::running, tp->get_state());

    for(auto w : workers) 
    {
        EXPECT_EQ(mce::lifecycle::state::running, w->get_state());
    }

    tp->worker().schedule([]{ mce::this_threadpool().suspend(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(mce::lifecycle::state::suspended, tp->get_state());

    for(auto w : workers) 
    {
        EXPECT_EQ(mce::lifecycle::state::suspended, w->get_state());
    }

    auto do_nothing = []{}; 

    tp->resume();

    for(auto w : workers) 
    {
        w->schedule(do_nothing);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(mce::lifecycle::state::running, tp->get_state());

    for(auto w : workers) 
    {
        EXPECT_EQ(mce::lifecycle::state::running, w->get_state());
    }

    tp->worker().schedule([]{ mce::this_scheduler().suspend(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(mce::lifecycle::state::suspended, tp->get_state());

    for(auto w : workers) 
    {
        EXPECT_EQ(mce::lifecycle::state::suspended, w->get_state());
    }

    tp->resume();

    for(auto w : workers) 
    {
        w->schedule(do_nothing);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(mce::lifecycle::state::running, tp->get_state());

    for(auto w : workers) 
    {
        EXPECT_EQ(mce::lifecycle::state::running, w->get_state());
    }

    tp->halt();
}

#define BILLION 1000000000
#define MILLION 1000000 
#define THOUSAND 1000

using namespace mce;

TEST(threadpool, default_threadpool_enabled)
{
    EXPECT_TRUE(mce::default_threadpool_enabled());
}

TEST(threadpool, default_threadpool_size)
{
    size_t count = default_threadpool().size();
    std::cout << "default_threadpool::size: " << count << std::endl;
    EXPECT_TRUE(count > 0);
}

TEST(threadpool, parallel)
{
    unbuffered_channel<int> ch;
    ch.construct();

    thunk t = [ch]{ ch.send(3); };
    parallel(t);

    int ret = 0;
    ch.recv(ret);

    EXPECT_EQ(ret,3);
}

TEST(threadpool, parallel_with_args)
{
    unbuffered_channel<int> ch;
    ch.construct();

    std::function<void(int)> f = [ch](int x){ ch.send(x); };

    parallel(f,11);

    int x;
    ch.recv(x);
    EXPECT_EQ(x,11);

    std::function<void(int,int)> f2 = [ch](int x, int y){ ch.send((x+y)); };

    parallel(f2,11,4);

    ch.recv(x);
    EXPECT_EQ(x,15);
}

TEST(threadpool, parallel_with_rvalue_lambda)
{
    unbuffered_channel<int> ch;
    ch.construct();

    parallel([ch](int x){ ch.send(x); },11);

    int x;
    ch.recv(x);
    EXPECT_EQ(x,11);

    parallel([ch](int x, int y){ ch.send((x+y)); },11,4);

    ch.recv(x);
    EXPECT_EQ(x,15);
}

TEST(threadpool, parallel_fptr)
{
    reset_fptr_vals();
    int x=0;

    fptr::x_ = 3;
    parallel(&(fptr::f));
    fptr::ch_.recv(x);
    EXPECT_EQ(x,6);

    fptr::x_ = 4;
    parallel(&(fptr::f2),4);
    fptr::ch_.recv(x);
    EXPECT_EQ(x,16);

    fptr_void::x_ = 1;
    parallel(&(fptr_void::f));
    fptr_void::ch_.recv(x);
    EXPECT_EQ(x,2);

    fptr_void::x_ = 2;
    parallel(&(fptr_void::f2),2);
    fptr_void::ch_.recv(x);
    EXPECT_EQ(x,4);
}

TEST(threadpool, parallel_which_scheduler)
{
    auto cs = mce::scheduler::make();
    auto tp = mce::threadpool::make();
    auto cs_ch = mce::chan<std::shared_ptr<mce::scheduler>>::make();
    auto tp_ch = mce::chan<std::shared_ptr<mce::threadpool>>::make();
    auto return_sched = [=]
    { 
        cs_ch.send((std::shared_ptr<mce::scheduler>)mce::this_scheduler()); 
        tp_ch.send((std::shared_ptr<mce::threadpool>)mce::this_threadpool());
    };
        
    mce::parallel(return_sched);

    std::shared_ptr<mce::scheduler> recv_cs;
    std::shared_ptr<mce::threadpool> recv_tp;
    cs_ch.recv(recv_cs);
    tp_ch.recv(recv_tp);
    EXPECT_NE(std::shared_ptr<mce::scheduler>(),recv_cs);
    EXPECT_NE(cs, recv_cs);
    EXPECT_NE(std::shared_ptr<mce::threadpool>(),recv_tp);
    EXPECT_EQ((std::shared_ptr<mce::threadpool>)mce::default_threadpool(),recv_tp);
    EXPECT_NE(tp, recv_tp);

    cs->schedule([=]
    {
        auto sync_ch = mce::chan<int>::make();
        mce::parallel([=]
        {
            return_sched();
            sync_ch.send(0);
        });
        int r;
        sync_ch.recv(r);
    });
    std::thread thd([=]{ cs->run(); });

    cs_ch.recv(recv_cs);
    tp_ch.recv(recv_tp);
    EXPECT_NE(std::shared_ptr<mce::scheduler>(),recv_cs);
    EXPECT_NE(cs, recv_cs);
    EXPECT_NE(std::shared_ptr<mce::threadpool>(),recv_tp);
    EXPECT_EQ((std::shared_ptr<mce::threadpool>)mce::default_threadpool(),recv_tp);
    EXPECT_NE(tp, recv_tp);

    cs->halt();
    thd.join();

    tp->worker().schedule(return_sched);

    cs_ch.recv(recv_cs);
    tp_ch.recv(recv_tp);
    EXPECT_NE(std::shared_ptr<mce::scheduler>(),recv_cs);
    EXPECT_NE(cs, recv_cs);
    EXPECT_NE(std::shared_ptr<mce::threadpool>(),recv_tp);
    EXPECT_NE((std::shared_ptr<mce::threadpool>)mce::default_threadpool(),recv_tp);
    EXPECT_EQ(tp, recv_tp);

    tp->halt();
}

TEST(threadpool, concurrent)
{
    unbuffered_channel<int> ch;
    ch.construct();

    thunk t = [ch]{ ch.send(3); };
    concurrent(t);

    int ret = 0;
    ch.recv(ret);

    EXPECT_EQ(ret,3);
}

TEST(threadpool, concurrent_with_args)
{
    unbuffered_channel<int> ch;
    ch.construct();

    std::function<void(int)> f = [ch](int x){ ch.send(x); };

    concurrent(f,11);

    int x;
    ch.recv(x);
    EXPECT_EQ(x,11);

    std::function<void(int,int)> f2 = [ch](int x, int y){ ch.send(x+y); };

    concurrent(f2,11,4);

    ch.recv(x);
    EXPECT_EQ(x,15);
}

TEST(threadpool, concurrent_with_rvalue_lambda)
{
    unbuffered_channel<int> ch;
    ch.construct();

    concurrent([ch](int x){ ch.send(x); },11);

    int x;
    ch.recv(x);
    EXPECT_EQ(x,11);

    concurrent([ch](int x, int y){ ch.send(x+y); },11,4);

    ch.recv(x);
    EXPECT_EQ(x,15);
}

TEST(threadpool, concurrent_fptr)
{
    reset_fptr_vals();
    int x=0;

    fptr::x_ = 3;
    concurrent(&(fptr::f));
    fptr::ch_.recv(x);
    EXPECT_EQ(x,6);

    fptr::x_ = 4;
    concurrent(&(fptr::f2),4);
    fptr::ch_.recv(x);
    EXPECT_EQ(x,16);

    fptr_void::x_ = 1;
    concurrent(&(fptr_void::f));
    fptr_void::ch_.recv(x);
    EXPECT_EQ(x,2);

    fptr_void::x_ = 2;
    concurrent(&(fptr_void::f2),2);
    fptr_void::ch_.recv(x);
    EXPECT_EQ(x,4);
}

TEST(threadpool, balance)
{
    unbuffered_channel<int> ch;
    ch.construct();

    thunk t = [ch]{ ch.send(3); };
    balance(t);

    int ret = 0;
    ch.recv(ret);

    EXPECT_EQ(ret,3);
}

TEST(threadpool, balance_with_args)
{
    unbuffered_channel<int> ch;
    ch.construct();

    std::function<void(int)> f = [ch](int x){ ch.send(x); };

    balance(f,11);

    int x;
    ch.recv(x);
    EXPECT_EQ(x,11);

    std::function<void(int,int)> f2 = [ch](int x, int y){ ch.send(x+y); };

    balance(f2,11,4);

    ch.recv(x);
    EXPECT_EQ(x,15);
}

TEST(threadpool, balance_with_rvalue_lambda)
{
    unbuffered_channel<int> ch;
    ch.construct();

    balance([ch](int x){ ch.send(x); },11);

    int x;
    ch.recv(x);
    EXPECT_EQ(x,11);

    balance([ch](int x, int y){ ch.send(x+y); },11,4);

    ch.recv(x);
    EXPECT_EQ(x,15);
}

TEST(threadpool, balance_fptr)
{
    reset_fptr_vals();
    int x=0;

    fptr::x_ = 3;
    balance(&(fptr::f));
    fptr::ch_.recv(x);
    EXPECT_EQ(x,6);

    fptr::x_ = 4;
    balance(&(fptr::f2),4);
    fptr::ch_.recv(x);
    EXPECT_EQ(x,16);

    fptr_void::x_ = 1;
    balance(&(fptr_void::f));
    fptr_void::ch_.recv(x);
    EXPECT_EQ(x,2);

    fptr_void::x_ = 2;
    balance(&(fptr_void::f2),2);
    fptr_void::ch_.recv(x);
    EXPECT_EQ(x,4);
}

TEST(threadpool, balance_rebalance)
{
    EXPECT_EQ(mce::balance_ratio(), MCEBALANCERATIO);

    auto convert_to_size_to_bit_string = [](size_t r, std::string& s)
    {
        do {
            s = std::to_string(r & 1) + s;
        } while ( r>>=1 );
    };

    size_t thread_count = 4;
    size_t task_count = 100;
    size_t per_max_core_tasks = 50;

    auto tp = mce::threadpool::make(thread_count);
    auto workers = tp->workers();
    auto blocking_ch = mce::chan<int>::make();
    auto done_ch = mce::chan<int>::make();

    EXPECT_FALSE(blocking_ch.closed());

    // ensure this task is always enqueued
    auto continuous_execution = [=]
    {
        while(!blocking_ch.closed()){ mce::yield(); }
        done_ch.send(0);
    };

    // ensure this task is generally blocking
    auto blocking_task = [=]
    {
        for(auto& i : blocking_ch) { i = i+1; }
        done_ch.send(0);
    };

    // launch tasks from a single core
    auto launch_tasks = [&]
    { 
        for(size_t i=0; i<task_count; ++i)
        {
            mce::balance(blocking_task);
        }
    };

    // launch a continuously executing task on each thread so there's always 
    // one enqueued
    for(auto& w : workers)
    {
        w->schedule(continuous_execution);
    }

    // launch a coroutine to launch other coroutines on exactly one thread
    tp->worker().schedule(launch_tasks);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_FALSE(blocking_ch.closed());

    size_t worker_validation = 0;

    for(auto& w : workers)
    {
        auto weight = w->measure();
        std::string weight_bits;
        size_t weight_raw = weight;
        convert_to_size_to_bit_string(weight_raw, weight_bits);

        std::cout << "weight: "
                  << weight_raw
                  << "; weight bits: " 
                  << weight_bits
                  << "; weight.enqueued(): "
                  << weight.enqueued()
                  << "; weight.scheduled(): "
                  << weight.scheduled()
                  << std::endl;

        EXPECT_LT(weight.enqueued(), per_max_core_tasks);
        EXPECT_LT(weight.scheduled(), per_max_core_tasks);
        EXPECT_GT(weight.enqueued(), 0);
        EXPECT_GT(weight.scheduled(), 0);
        EXPECT_EQ(weight.blocked(), weight.scheduled() - weight.enqueued());
        ++worker_validation;
    }

    EXPECT_EQ(worker_validation, 4);

    blocking_ch.close();

    for(size_t i=0; i<task_count+thread_count; ++i)
    {
        int r;
        done_ch.recv(r);
    }

    done_ch.close();

    tp->halt();
}

TEST(threadpool, memory_independence)
{
    unbuffered_channel<int> ch0, ch1;
    unbuffered_channel<int> done_ch;
    ch0.construct();
    ch1.construct();
    done_ch.construct();

    mce::thunk t0 = [ch0]{ ch0.send(3); };

    mce::thunk t1 = [ch0,ch1,done_ch] 
    { 
        int x; 
        ch0.recv(x); 
        ch1.send(1); 
        done_ch.send(x);
    };

    mce::thunk t2 = [ch1]{ int x; ch1.recv(x); };

    parallel(t0);
    parallel(t1);
    parallel(t2);

    int x;
    done_ch.recv(x);
    EXPECT_EQ(x,3);
}

void yield_repeat(buffered_channel<std::uint64_t> done_ch, 
                  std::uint64_t repeat,
                  std::uint64_t target)
{
    std::uint64_t c=0;
    for(size_t i=0; i<repeat; ++i)
    {
        for(c=0; c<target; ++c) { }
        mce::yield();
    }
    done_ch.send(c);
}

TEST(threadpool, counting_parallel)
{
    std::uint64_t task_num = 100*THOUSAND;
    std::uint64_t target = BILLION;
    std::uint64_t repeat = 1;

    std::cout << task_num
              << " coroutines each counting to " 
              << BILLION
              << std::endl;

    buffered_channel<std::uint64_t> done_ch;
    done_ch.construct(task_num);
    
    thunk t = [&]{ yield_repeat(done_ch, repeat, target); };

    for(std::uint64_t c=0; c<task_num; ++c){ parallel(t); }

    std::uint64_t x;
    for(std::uint64_t c=0; c<task_num; ++c){ done_ch.recv(x); }
}

TEST(threadpool, counting_concurrent)
{
    std::uint64_t task_num = 100*THOUSAND;
    std::uint64_t target = BILLION;
    std::uint64_t repeat = 1;

    std::cout << task_num
              << " coroutines each counting to " 
              << BILLION
              << std::endl;

    buffered_channel<std::uint64_t> done_ch;
    done_ch.construct(task_num);
    
    thunk t = [&]{ yield_repeat(done_ch, repeat, target); };

    for(std::uint64_t c=0; c<task_num; ++c){ concurrent(t); }

    std::uint64_t x;
    for(std::uint64_t c=0; c<task_num; ++c){ done_ch.recv(x); }
}

TEST(threadpool, counting_balance)
{
    std::uint64_t task_num = 100*THOUSAND;
    std::uint64_t target = BILLION;
    std::uint64_t repeat = 1;

    std::cout << task_num
              << " coroutines each counting to " 
              << BILLION
              << std::endl;

    buffered_channel<std::uint64_t> done_ch;
    done_ch.construct(task_num);
    
    thunk t = [&]{ yield_repeat(done_ch, repeat, target); };

    for(std::uint64_t c=0; c<task_num; ++c){ balance(t); }

    std::uint64_t x;
    for(std::uint64_t c=0; c<task_num; ++c){ done_ch.recv(x); }
}

TEST(threadpool, counting_concurrent_in_threadpool)
{
    std::uint64_t task_num = 100*THOUSAND;
    std::uint64_t target = BILLION;
    std::uint64_t repeat = 1;

    std::cout << task_num
              << " coroutines each counting to " 
              << BILLION
              << std::endl;

    buffered_channel<std::uint64_t> done_ch;
    done_ch.construct(task_num);
    
    thunk t = [&]{ yield_repeat(done_ch, repeat, target); };

    default_threadpool().worker().schedule([=]
    { 
        for(std::uint64_t c=0; c<task_num; ++c){ concurrent(t); }
    });

    std::uint64_t x;
    for(std::uint64_t c=0; c<task_num; ++c){ done_ch.recv(x); }
}

TEST(threadpool, counting_parallel_in_threadpool)
{
    std::uint64_t task_num = 100*THOUSAND;
    std::uint64_t target = BILLION;
    std::uint64_t repeat = 1;

    std::cout << task_num
              << " coroutines each counting to " 
              << BILLION
              << std::endl;

    buffered_channel<std::uint64_t> done_ch;
    done_ch.construct(task_num);
    
    thunk t = [&]{ yield_repeat(done_ch, repeat, target); };

    default_threadpool().worker().schedule([=]
    { 
        for(std::uint64_t c=0; c<task_num; ++c){ parallel(t); }
    });

    std::uint64_t x;
    for(std::uint64_t c=0; c<task_num; ++c){ done_ch.recv(x); }
}

TEST(threadpool, counting_balance_in_threadpool)
{
    std::uint64_t task_num = 100*THOUSAND;
    std::uint64_t target = BILLION;
    std::uint64_t repeat = 1;

    std::cout << task_num
              << " coroutines each counting to " 
              << BILLION
              << std::endl;

    buffered_channel<std::uint64_t> done_ch;
    done_ch.construct(task_num);
    
    thunk t = [&]{ yield_repeat(done_ch, repeat, target); };

    default_threadpool().worker().schedule([=]
    { 
        for(std::uint64_t c=0; c<task_num; ++c){ balance(t); }
    });

    std::uint64_t x;
    for(std::uint64_t c=0; c<task_num; ++c){ done_ch.recv(x); }
}

TEST(threadpool, counting_yield_10)
{
    std::uint64_t task_num = 100*THOUSAND;
    std::uint64_t repeat = 10;
    std::uint64_t target = BILLION/repeat;

    std::cout << task_num
              << " coroutines each counting to " 
              << target 
              << " " 
              << repeat 
              << " times with yields between each repeat"
              << std::endl;

    buffered_channel<std::uint64_t> done_ch;
    done_ch.construct(task_num);

    thunk t = [&]{ yield_repeat(done_ch, repeat, target); };

    for(std::uint64_t c=0; c<task_num; ++c) { parallel(t); }

    std::uint64_t x;
    for(std::uint64_t c=0; c<task_num; ++c) { done_ch.recv(x); }
}

TEST(threadpool, counting_yield_100)
{
    std::uint64_t task_num = 100*THOUSAND;
    std::uint64_t repeat = 100;
    std::uint64_t target = BILLION/repeat;

    std::cout << task_num
              << " coroutines each counting to " 
              << target 
              << " " 
              << repeat 
              << " times with yields between each repeat"
              << std::endl;

    buffered_channel<std::uint64_t> done_ch;
    done_ch.construct(task_num);

    thunk t = [&]{ yield_repeat(done_ch, repeat, target); };

    for(std::uint64_t c=0; c<task_num; ++c) { parallel(t); }

    std::uint64_t x;
    for(std::uint64_t c=0; c<task_num; ++c) { done_ch.recv(x); }
}

TEST(threadpool, baseball)
{
    std::uint64_t games = 50*THOUSAND;
    int pass_count = 3;

    std::cout << games
              << " games each with 2 coroutines passing the ball "
              << pass_count
              << " times (coroutine total: "
              << games*2
              << ")"
              << std::endl;

    mce::buffered_channel<int> done_ch;
    done_ch.construct(games);

    for(std::uint64_t c=0; c<games; ++c)
    {
        mce::unbuffered_channel<int> father_to_son;
        mce::unbuffered_channel<int> son_to_father;
        father_to_son.construct();
        son_to_father.construct();
        
        mce::thunk t1 = [father_to_son,son_to_father,done_ch]
        {
            for(int catches=0; catches<3; ++catches)
            {
                int ball;
                father_to_son.recv(ball);
                son_to_father.send(catches);
            }
            done_ch.send(0);
        };

        mce::thunk t0 = [father_to_son,son_to_father,t1]
        {
            mce::concurrent(t1);
            father_to_son.send(0);

            for(int catches=0; catches<3; ++catches)
            {
                int ball;
                son_to_father.recv(ball);
                father_to_son.send(catches+1);
            }
        };

        mce::parallel(t0);
    }

    std::function<int()> tdone = [&]
    {
        int x;
        for(std::uint64_t c=0; c<games; ++c) { done_ch.recv(x); }
        return 0;
    };


    auto bch = mce::chan<int>::make();
    parallel([=]{ bch.send(tdone()); });

    int x = -1;
    bch.recv(x);

    EXPECT_EQ(x,0);
}

TEST(threadpool, baseball_balance)
{
    std::uint64_t games = 50*THOUSAND;
    int pass_count = 3;

    std::cout << games
              << " games each with 2 coroutines passing the ball "
              << pass_count
              << " times (coroutine total: "
              << games*2
              << ")"
              << std::endl;

    mce::buffered_channel<int> done_ch;
    done_ch.construct(games);

    for(std::uint64_t c=0; c<games; ++c)
    {
        mce::unbuffered_channel<int> father_to_son;
        mce::unbuffered_channel<int> son_to_father;
        father_to_son.construct();
        son_to_father.construct();
        
        mce::thunk t1 = [father_to_son,son_to_father,done_ch]
        {
            for(int catches=0; catches<3; ++catches)
            {
                int ball;
                father_to_son.recv(ball);
                son_to_father.send(catches);
            }
            done_ch.send(0);
        };

        mce::thunk t0 = [father_to_son,son_to_father,t1]
        {
            mce::balance(t1);
            father_to_son.send(0);

            for(int catches=0; catches<3; ++catches)
            {
                int ball;
                son_to_father.recv(ball);
                father_to_son.send(catches+1);
            }
        };

        mce::parallel(t0);
    }

    std::function<int()> tdone = [&]
    {
        int x;
        for(std::uint64_t c=0; c<games; ++c) { done_ch.recv(x); }
        return 0;
    };


    auto bch = mce::chan<int>::make();
    parallel([=]{ bch.send(tdone()); });

    int x = -1;
    bch.recv(x);

    EXPECT_EQ(x,0);
}
