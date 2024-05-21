//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "timer.hpp"
#include "function_utility.hpp"
#include "scheduler.hpp"
#include "unbuffered_channel.hpp"
#include "buffered_channel.hpp"
#include "chan.hpp"

#include <thread>
#include <exception>
#include <atomic>
#include <vector>
#include <list>
#include <forward_list>

#include <gtest/gtest.h>

TEST(scheduler, run)
{
    int i1 = 0;
    int i2 = 0;
    int i3 = 0;

    mce::buffered_channel<int> done_ch;
    mce::unbuffered_channel<int> halt_ch;
    done_ch.construct();
    halt_ch.construct();

    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();

    mce::thunk t1 = [&i1,done_ch]{ i1 = 1; done_ch.send(0); };
    mce::thunk t2 = [&i2,done_ch]{ i2 = 2; done_ch.send(0); };
    mce::thunk t3 = [&i3,done_ch]{ i3 = 3; done_ch.send(0); };
    mce::thunk t4 = [done_ch,halt_ch]
    {
        int x;
        for(int c=0; c<3; ++c)
        {
            done_ch.recv(x);
        }
        halt_ch.send(0);
    };

    cs->schedule(t1);
    cs->schedule(t2);
    cs->schedule(t3);
    cs->schedule(t4);

    std::thread thd([&]{ cs->run(); });

    int x;
    halt_ch.recv(x);

    cs->halt();
    thd.join();

    EXPECT_EQ(i1,1);
    EXPECT_EQ(i2,2);
    EXPECT_EQ(i3,3);
}

TEST(scheduler, run_suspend_resume)
{
    int i1 = 0;
    int i2 = 0;
    int i3 = 0;

    mce::buffered_channel<int> done_ch;
    mce::unbuffered_channel<int> halt_ch;
    done_ch.construct();
    halt_ch.construct();

    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
        
    int x;

    mce::thunk t1 = [&i1,done_ch]{ i1 = 1; done_ch.send(0); };
    mce::thunk t2 = [&i2,done_ch]{ i2 = 2; done_ch.send(1); };
    mce::thunk t3 = [&i3,done_ch]{ i3 = 3; done_ch.send(2); };
    mce::thunk t4 = [done_ch,halt_ch,cs,&x]() mutable
    {
        for(int c=0; c<3; ++c)
        {
            done_ch.recv(x);
            EXPECT_EQ(mce::lifecycle::state::running, cs->get_state());
            cs->suspend();
            EXPECT_EQ(mce::lifecycle::state::suspended, cs->get_state());
            mce::this_coroutine()->yield();
        }
        halt_ch.send(4);
    };

    cs->schedule(t1);
    cs->schedule(t2);
    cs->schedule(t3);
    cs->schedule(t4);

    EXPECT_EQ(mce::lifecycle::state::ready, cs->get_state());

    std::thread thd([&]{ while(cs->run()){ } });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(0, x);
    EXPECT_EQ(mce::lifecycle::state::suspended, cs->get_state());
    cs->resume();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(1, x);
    cs->resume();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(2, x);
    cs->resume();

    halt_ch.recv(x);
    EXPECT_EQ(4, x);

    cs->halt();
    thd.join();
    
    EXPECT_EQ(mce::lifecycle::state::halted, cs->get_state());

    EXPECT_EQ(i1,1);
    EXPECT_EQ(i2,2);
    EXPECT_EQ(i3,3);
}

TEST(scheduler, run_suspend_resume_continous)
{
    int sync;
    size_t i=0;
    const size_t target=1000000;
    mce::unbuffered_channel<int> halt_ch;
    halt_ch.construct();
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();

    auto yielding_task = [&]
    {
        // loop whenever the chance arises for execution
        for(; i<target; ++i) 
        {
            mce::this_coroutine()->yield();
        }

        halt_ch.send(0);
    };

    cs->schedule(yielding_task);
    cs->suspend(); // pre-suspend worker thread

    std::thread thd([=]{ while(cs->run()) { } });

    while(halt_ch.try_recv(sync) == mce::result::failure) 
    {
        cs->resume();
        cs->suspend();
    }

    cs->halt();
    thd.join();

    EXPECT_EQ(target, i);
}

TEST(scheduler, run_channels)
{
    mce::unbuffered_channel<int> ch1, ch2;
    mce::unbuffered_channel<int> halt_ch;
    ch1.construct();
    ch2.construct();
    halt_ch.construct();

    int i1 = 0;
    int i2 = 0;

    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();

    mce::thunk t1 = [&i1,ch1,ch2,halt_ch]
    {
        ch1.send(1);
        ch2.recv(i1);
        halt_ch.send(0);
    };

    mce::thunk t2 = [&i2,ch1,ch2]
    {
        ch1.recv(i2);
        ch2.send(2);
    };

    cs->schedule(t2);
    cs->schedule(t1);

    std::thread thd([&]{ cs->run(); });

    int x;
    halt_ch.recv(x);

    cs->halt();
    thd.join();

    EXPECT_EQ(i1,2);
    EXPECT_EQ(i2,1);
}

TEST(scheduler, run_competing_coroutines)
{
    mce::unbuffered_channel<int> ch;
    mce::unbuffered_channel<int> halt_ch;
    ch.construct();
    halt_ch.construct();

    int ret = 2;

    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();

    mce::thunk send_t0 = [ch]{ ch.send(0); };
    mce::thunk send_t1 = [ch]{ ch.send(1); };
    mce::thunk recv_t = [&ret,ch,halt_ch]
    { 
        ch.recv(ret); 
        ch.recv(ret); 
        halt_ch.send(0);
    };

    cs->schedule(recv_t);
    cs->schedule(send_t1);
    cs->schedule(send_t0);

    std::thread thd([&]{ cs->run(); });

    int x;
    halt_ch.recv(x);

    cs->halt();
    thd.join();

    EXPECT_TRUE(ret == 0 || ret == 1);
}

TEST(scheduler, schedule_with_args)
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
    std::thread thd([&]{ cs->run(); });

    mce::chan<int> ch = mce::chan<int>::make();
    mce::chan<int> done_ch = mce::chan<int>::make();
    int i = 0;

    cs->schedule([&](mce::chan<int> ch)
    {
        mce::this_scheduler().schedule([&](mce::chan<int> ch)
        {
            ch.recv(i);
            done_ch.send(0);
        }, ch);

        ch.send(3);
    }, ch);

    int r;
    done_ch.recv(r);
    EXPECT_EQ(i,3);

    cs->halt();
    thd.join();
}

TEST(scheduler, same_thread_halt)
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();

    mce::chan<int> ch = mce::chan<int>::make();
    int i = 0;

    cs->schedule([&](mce::chan<int> ch)
    {
        mce::this_scheduler().schedule([&](mce::chan<int> ch)
        {
            ch.recv(i);
            mce::this_scheduler().halt();
        }, ch);

        ch.send(3);
    }, ch);
    cs->run();

    EXPECT_EQ(i,3);
}

// Creates a mce::scheduler and checks the running state before/during/after
TEST(scheduler, schedule_running)
{
    mce::unbuffered_channel<int> halt_chan;
    halt_chan.construct();
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
    int x{0};


    mce::thunk th = [halt_chan]()
    {
        halt_chan.send(50);
    };

    cs->schedule(th);
    std::thread thd([=]{ cs->run(); });

    halt_chan.recv(x);
    cs->halt();
    thd.join();

    EXPECT_EQ(x, 50);
}

// Creates a mce::scheduler and schedules using a lambda/pointer/std::move()
TEST(scheduler, schedule_variants_lambda_pointer_move)
{
    mce::unbuffered_channel<int> halt_chan;
    mce::unbuffered_channel<int> sync_chan;
    halt_chan.construct();
    sync_chan.construct();
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
    int x{0};

    mce::thunk th1 = [sync_chan]() { sync_chan.send(50); };
    mce::thunk th2 = [&x, halt_chan, sync_chan]() 
    { 
        sync_chan.recv(x);
        halt_chan.send(x);
    };

    cs->schedule(std::move(th1));
    cs->schedule([sync_chan]() { sync_chan.send(100); });
    mce::thunk* th_ptr {new mce::thunk(th2)};
    cs->schedule(*th_ptr);

    std::thread thd([=]{ cs->run(); } );

    halt_chan.recv(x);
    cs->halt();
    thd.join();

    EXPECT_TRUE(x == 50 || x == 100);
    delete th_ptr;
}

// Creates a mce::scheduler and schedules using func(args)/func(return)
int func_ret()
{
    return 50;
}

void func_args(int& a)
{
    a *= a;
}

TEST(scheduler, schedule_variants_func_args_return)
{
    mce::chan<int> halt_chan {mce::chan<int>::make()};
    mce::chan<int> sync_chan {mce::chan<int>::make()};
    mce::chan<int> ret_chan {mce::chan<int>::make()};
    std::shared_ptr<mce::scheduler> cs {mce::scheduler::make()};
    int x{10}, y{10}, z{10};

    mce::thunk th1 = [&, sync_chan]() { sync_chan.send(x); };
    mce::thunk th2 = [&, sync_chan, halt_chan]() { sync_chan.recv(z); halt_chan.send(0); };

    cs->schedule([=]() { ret_chan.send(func_ret()); });
    cs->schedule([=](int &y) { func_args(y); }, std::ref(y));
    cs->schedule(th1);
    cs->schedule(th2);

    std::thread thd([=] { cs->run();});

    ret_chan.recv(x);
    halt_chan.recv(z);
    cs->halt();
    thd.join();

    EXPECT_EQ(y, 100);
    EXPECT_EQ(x, 50);
    EXPECT_EQ(z, 0);
}

TEST(scheduler, measure)
{
    std::thread thd;
    auto scheduler = mce::scheduler::make();
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

    scheduler->schedule(continuous_execution);

    {
        auto weight = scheduler->measure();
        EXPECT_EQ(weight.enqueued(), 1);
        EXPECT_EQ(weight.scheduled(), 1);
        EXPECT_EQ(weight.blocked(), 0);
    }

    thd = std::thread([=]{ scheduler->run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        auto weight = scheduler->measure();
        EXPECT_EQ(weight.enqueued(), 1);
        EXPECT_EQ(weight.scheduled(), 1);
        EXPECT_EQ(weight.blocked(), 0);
    }

    scheduler->schedule(continuous_execution);
    scheduler->schedule(blocking_task);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        auto weight = scheduler->measure();
        EXPECT_EQ(weight.enqueued(), 2);
        EXPECT_EQ(weight.scheduled(), 3);
        EXPECT_EQ(weight.blocked(), 1);
    }

    scheduler->schedule(blocking_task);
    scheduler->schedule(blocking_task);
    scheduler->schedule(blocking_task);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        auto weight = scheduler->measure();
        EXPECT_EQ(weight.enqueued(), 2);
        EXPECT_EQ(weight.scheduled(), 6);
        EXPECT_EQ(weight.blocked(), 4);

        blocking_ch.close();

        for(size_t i=0; i<weight.scheduled(); ++i)
        {
            int r;
            done_ch.recv(r);
        }

        done_ch.close();
    }

    scheduler->halt();
    thd.join();
}

namespace test {
namespace detail {

template <typename CONTAINER, typename PUSH_OP>
void group_coroutines(CONTAINER& c, PUSH_OP& push)
{
}

template <typename CONTAINER, typename PUSH_OP, typename A, typename... As>
void group_coroutines(CONTAINER& c, PUSH_OP& push, A&& a, As&&... as)
{
    push(c, std::move(a));
    group_coroutines(c, push, std::forward<As>(as)...);
}

template <typename CONTAINER, typename PUSH_OP>
void schedule_group_container_test(std::shared_ptr<mce::scheduler> sch, PUSH_OP& push)
{
    int r;
    int test_count = 0;
    auto uch = mce::unbuffered_channel<int>::make();
    auto uch2 = mce::unbuffered_channel<int>::make();
    auto done_ch = mce::buffered_channel<int>::make();

    auto t1 = [=](int i) mutable 
    {
        uch.send(i);
    };

    auto t2 = [](mce::unbuffered_channel<int> uch, mce::unbuffered_channel<int> uch2) 
    {
        int i;
        uch.recv(i);
        uch2.send(i);
    };

    auto t3 =  [](mce::unbuffered_channel<int> uch, mce::buffered_channel<int> done_ch)
    {
        int i;
        uch.recv(i);
        done_ch.send(i);
    };

    {
        const int i = 1;
        CONTAINER c;
        group_coroutines(
            c, 
            push, 
            mce::coroutine::make(t1, i),
            mce::coroutine::make(t2, uch, uch2),
            mce::coroutine::make(t3, uch2, done_ch));
        sch->schedule(c);
        done_ch.recv(r);
        EXPECT_EQ(i,r);
        ++test_count;
    }

    {
        const int i = 2;
        CONTAINER c;
        group_coroutines(
            c, 
            push, 
            mce::coroutine::make(t3, uch2, done_ch));
        sch->schedule(
            mce::coroutine::make(t1, i),
            mce::coroutine::make(t2, uch, uch2),
            c);
        done_ch.recv(r);
        EXPECT_EQ(i,r);
        ++test_count;
    }

    {
        const int i = 3;
        sch->schedule(
            mce::coroutine::make(t1, i),
            mce::coroutine::make(t2, uch, uch2),
            mce::coroutine::make(t3, uch2, done_ch));
        done_ch.recv(r);
        EXPECT_EQ(i,r);
        ++test_count;
    }

    {
        const int i = 4;
        sch->schedule(
            mce::coroutine::make(t1, i),
            mce::coroutine::make(t2, uch, uch2),
            t3, 
            uch2, 
            done_ch);
        done_ch.recv(r);
        EXPECT_EQ(i,r);
        ++test_count;
    }

    {
        const int i = 5;
        CONTAINER c;
        group_coroutines(
            c, 
            push, 
            mce::coroutine::make(t2, uch, uch2));
        sch->schedule(
            mce::coroutine::make(t1, i),
            c,
            t3, 
            uch2, 
            done_ch);
        done_ch.recv(r);
        EXPECT_EQ(i,r);
        ++test_count;
    }
 
    {
        const int i = 6;
        CONTAINER c;
        group_coroutines(
            c, 
            push, 
            mce::coroutine::make(t1, i));
        sch->schedule(
            c,
            mce::coroutine::make(t2, uch, uch2),
            t3, 
            uch2, 
            done_ch);
        done_ch.recv(r);
        EXPECT_EQ(i,r);
        ++test_count;
    }
 
    {
        const int i = 7;
        CONTAINER c;
        group_coroutines(
            c, 
            push, 
            mce::coroutine::make(t1, i),
            mce::coroutine::make(t2, uch, uch2));
        sch->schedule(c, t3, uch2, done_ch);
        done_ch.recv(r);
        EXPECT_EQ(i,r);
        ++test_count;
    }
   
    EXPECT_EQ(7,test_count);
}

}
}

TEST(scheduler, schedule_group)
{
    auto sch = mce::scheduler::make();
    std::thread thd([=]() mutable { while(sch->run()){} });

    auto vec_push = [](std::vector<std::unique_ptr<mce::coroutine>>& v, std::unique_ptr<mce::coroutine>&& c)
    {
        v.push_back(std::move(c));
    };

    auto list_push = [](std::list<std::unique_ptr<mce::coroutine>>& l, std::unique_ptr<mce::coroutine>&& c)
    {
        l.push_back(std::move(c));
    };

    auto forward_list_push = [](std::forward_list<std::unique_ptr<mce::coroutine>>& fl, std::unique_ptr<mce::coroutine>&& c)
    {
        fl.push_front(std::move(c));
    };

    test::detail::schedule_group_container_test<std::vector<std::unique_ptr<mce::coroutine>>>(sch, vec_push);
    test::detail::schedule_group_container_test<std::list<std::unique_ptr<mce::coroutine>>>(sch, list_push);
    test::detail::schedule_group_container_test<std::forward_list<std::unique_ptr<mce::coroutine>>>(sch, forward_list_push);

    sch->halt();
    thd.join();
}
