//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "function_utility.hpp"
#include "scheduler.hpp"
#include "buffered_channel.hpp"

#include <array>
#include <memory>
#include <thread>
#include <queue>
#include <string>
#include <deque>

#include <gtest/gtest.h>

#define RETVAL 15

TEST(buffered_channel, buffered_channel_context)
{
    mce::buffered_channel<int> ch;
    EXPECT_EQ(ch.context(),(void*)NULL);
    ch.construct();
    EXPECT_NE(ch.context(),(void*)NULL);
}

TEST(buffered_channel, buffered_channel_type_info)
{
    {
        mce::buffered_channel<int> ch;
        EXPECT_NE(typeid(int), ch.type_info());
        EXPECT_NE(typeid(std::string), ch.type_info());
        EXPECT_EQ(typeid(mce::buffered_channel<int>), ch.type_info());
    }

    {
        mce::buffered_channel<std::string> ch;
        EXPECT_NE(typeid(int), ch.type_info());
        EXPECT_NE(typeid(std::string), ch.type_info());
        EXPECT_EQ(typeid(mce::buffered_channel<std::string>), ch.type_info());
    }
}

TEST(buffered_channel, buffered_channel_co_send_co_recv)
{
    mce::buffered_channel<int> ch;
    ch.construct();

    int ch_out = 0;

    mce::thunk t_send = [ch]{ ch.send(RETVAL); };
    mce::thunk t_recv = [&ch_out,ch]{ ch.recv(ch_out); };

    std::shared_ptr<mce::coroutine> co_send = std::make_shared<mce::coroutine>(t_send);
    std::shared_ptr<mce::coroutine> co_recv = std::make_shared<mce::coroutine>(t_recv);

    std::queue<std::shared_ptr<mce::coroutine>> cq;
    cq.push(co_send);
    cq.push(co_recv);

    EXPECT_TRUE(ch_out == 0);

    // iterate coroutines until buffered_channel send/recv completes
    while(!cq.empty())
    {
        std::shared_ptr<mce::coroutine> co_ptr = cq.front();
        cq.pop();

        co_ptr->run();
        if(!co_ptr->complete()) { cq.push(co_ptr); }
    }

    EXPECT_TRUE(ch_out == RETVAL);
}

// reverse order recv/send
TEST(buffered_channel, buffered_channel_co_recv_co_send)
{
    mce::buffered_channel<int> ch;
    ch.construct();

    int ch_out = 0;

    mce::thunk t_send = [ch]{ ch.send(RETVAL); };
    mce::thunk t_recv = [&ch_out,ch]{ ch.recv(ch_out); };

    std::shared_ptr<mce::coroutine> co_recv = std::make_shared<mce::coroutine>(t_recv);
    std::shared_ptr<mce::coroutine> co_send = std::make_shared<mce::coroutine>(t_send);

    std::queue<std::shared_ptr<mce::coroutine>> cq;
    cq.push(co_recv);
    cq.push(co_send);

    EXPECT_TRUE(ch_out == 0);

    // iterate coroutines until buffered_channel send/recv completes
    while(!cq.empty())
    {
        std::shared_ptr<mce::coroutine> co_ptr = cq.front();
        cq.pop();

        co_ptr->run();
        if(!co_ptr->complete()) { cq.push(co_ptr); }
    }

    EXPECT_TRUE(ch_out == RETVAL);
}

TEST(buffered_channel, buffered_channel_co_send_thread_recv)
{
    int ch_out = 0;

    mce::buffered_channel<int> ch;
    ch.construct();

    mce::thunk coro_thread_func = [ch]
    {
        mce::thunk t_send = [ch]{ ch.send(RETVAL); };
        mce::coroutine co_send(t_send);

        while(!co_send.complete()) { co_send.run(); }
    };

    mce::thunk normal_thread_func = [&ch_out,ch]{ ch.recv(ch_out); };

    EXPECT_TRUE(ch_out == 0);

    std::thread co_t(coro_thread_func);
    std::thread normal_t(normal_thread_func);
    co_t.join();
    normal_t.join();

    EXPECT_TRUE(ch_out == RETVAL);
}

// Reverse which is the mce::coroutine and which is the thread
TEST(buffered_channel, buffered_channel_thread_send_co_recv)
{
    int ch_out = 0;

    mce::buffered_channel<int> ch;
    ch.construct();

    mce::thunk coro_thread_func = [ch]
    {
        mce::thunk t_send = [ch]{ ch.send(RETVAL); };
        mce::coroutine co_send(t_send);

        while(!co_send.complete()) { co_send.run(); }
    };

    mce::thunk normal_thread_func = [&ch_out,ch]{ ch.recv(ch_out); };

    EXPECT_TRUE(ch_out == 0);

    std::thread normal_t(normal_thread_func);
    std::thread co_t(coro_thread_func);
    normal_t.join();
    co_t.join();

    EXPECT_TRUE(ch_out == RETVAL);
}

// reverse order
TEST(buffered_channel, buffered_channel_co_recv_thread_send)
{
    int ch_out = 0;

    mce::buffered_channel<int> ch;
    ch.construct();

    mce::thunk normal_thread_func = [ch]{ ch.send(RETVAL); };

    mce::thunk coro_thread_func = [&ch_out,ch]
    {
        mce::coroutine co_recv([&ch_out,ch]{ ch.recv(ch_out); });

        while(!co_recv.complete()) { co_recv.run(); }
    };

    EXPECT_TRUE(ch_out == 0);

    std::thread co_t(coro_thread_func);
    std::thread normal_t(normal_thread_func);

    co_t.join();
    normal_t.join();

    EXPECT_TRUE(ch_out == RETVAL);
}

// reverse order with mce::coroutine and thread switched
TEST(buffered_channel, buffered_channel_thread_recv_co_send)
{
    int ch_out = 0;

    mce::buffered_channel<int> ch;
    ch.construct();

    mce::thunk normal_thread_func = [ch]{ ch.send(RETVAL); };

    mce::thunk coro_thread_func = [&ch_out,ch]
    {
        mce::coroutine co_recv([&ch_out,ch]{ ch.recv(ch_out); });

        while(!co_recv.complete()) { co_recv.run(); }
    };

    EXPECT_TRUE(ch_out == 0);

    std::thread normal_t(normal_thread_func);
    std::thread co_t(coro_thread_func);

    normal_t.join();
    co_t.join();

    EXPECT_TRUE(ch_out == RETVAL);
}

TEST(buffered_channel, buffered_channel_thread_send_thread_recv)
{
    int ch_out = 0;

    mce::buffered_channel<int> ch;
    ch.construct();

    mce::thunk thread0_func = [ch]{ ch.send(RETVAL); };
    mce::thunk thread1_func = [&ch_out,ch]{ ch.recv(ch_out); };

    EXPECT_TRUE(ch_out == 0);

    std::thread th0(thread0_func);
    std::thread th1(thread1_func);
    th0.join();
    th1.join();

    EXPECT_TRUE(ch_out == RETVAL);
}

// Reverse order
TEST(buffered_channel, buffered_channel_thread_recv_thread_send)
{
    int ch_out = 0;

    mce::buffered_channel<int> ch;
    ch.construct();

    mce::thunk thread0_func = [&ch_out,ch]{ ch.recv(ch_out); };
    mce::thunk thread1_func = [ch]{ ch.send(RETVAL); };

    EXPECT_TRUE(ch_out == 0);

    std::thread th0(thread0_func);
    std::thread th1(thread1_func);
    th0.join();
    th1.join();

    EXPECT_TRUE(ch_out == RETVAL);
}


TEST(buffered_channel, capacity)
{
    mce::buffered_channel<std::uint64_t> ch;
    ch.construct(4); // give a max size of 4

    std::queue<mce::coroutine> q;

    bool cont = true;
    std::uint64_t cnt = 0;

    mce::coroutine sendter([ch, &cont, &cnt]{ while(cont) { ch.send(cnt++); } });

    EXPECT_EQ(ch.size(),0);
    EXPECT_TRUE(ch.empty());

    for(int i=0; i<40; ++i) { sendter.run(); }

    EXPECT_EQ(ch.size(),4);
    EXPECT_TRUE(!ch.empty());
}


TEST(buffered_channel, memory_independence)
{
    mce::buffered_channel<int> ch0, ch1;
    ch0.construct();
    ch1.construct();

    std::queue<mce::coroutine> q;
    mce::thunk t0 = [ch0]{ ch0.send(3); };

    mce::thunk t1 = [ch0,ch1]
    {
        int x;
        ch0.recv(x);
        ch1.send(1);
        EXPECT_EQ(x,3);
    };

    mce::thunk t2 = [ch1]{ int x; ch1.recv(x); };

    q.push(mce::coroutine(t0));
    q.push(mce::coroutine(t1));
    q.push(mce::coroutine(t2));

    while(q.size()>0)
    {
        mce::coroutine& c = q.front();
        c.run();
        if(!c.complete()) { q.push(std::move(c)); }
        q.pop();
    }

    q.push(mce::coroutine(t2));
    q.push(mce::coroutine(t1));
    q.push(mce::coroutine(t0));

    while(q.size()>0)
    {
        mce::coroutine& c = q.front();
        c.run();
        if(!c.complete()) { q.push(std::move(c)); }
        q.pop();
    }
}

TEST(buffered_channel, empty)
{
    mce::buffered_channel<int> ch1, ch2;
    ch1.construct(100);
    ch2.construct(100);
    std::vector<int> vec {};

    mce::thunk th1 = [ch1]
    {
        for(int i{0}; i < 100; ++i)
        {
            ch1.send(i);
        }
    };

    mce::thunk th2 = [ch1, &vec]
    {
        for(int i{0}; i < 100; ++i)
        {
            int r;
            ch1.recv(r);
            vec.push_back(std::move(r));
        }
    };

    mce::coroutine coro1(th1);
    mce::coroutine coro2(th2);

    while(!coro1.complete()){ coro1.run(); }
    EXPECT_FALSE(ch1.empty());
    EXPECT_EQ(ch1.size(), 100);
    while(!coro2.complete()){ coro2.run(); }

    EXPECT_TRUE(ch2.empty());
    EXPECT_TRUE(ch1.empty());
    EXPECT_EQ(ch1.size(), 0);
    EXPECT_EQ(ch2.size(), 0);
}

TEST(buffered_channel, size)
{
    mce::buffered_channel<int> ch1, ch2, ch3;
    ch1.construct();
    ch2.construct(10);
    ch3.construct(100);

    ch1.send(5);
    EXPECT_EQ(ch1.size(), 1);

    mce::thunk th1 = [ch2]
    {
        for(int i{0}; i < 10; ++i)
        {
            ch2.send(i);
        }
    };

    mce::thunk th2 = [ch3]
    {
        for(int i{0}; i < 100; ++i)
        {
            ch3.send(i);
        }
    };

    mce::coroutine coro1(th1);
    mce::coroutine coro2(th2);

    while(!coro1.complete()){ coro1.run(); }
    EXPECT_EQ(ch2.size(), 10);

    while(!coro2.complete()){ coro2.run(); }
    EXPECT_EQ(ch3.size(), 100);
}

TEST(buffered_channel, set_capacity)
{
    mce::buffered_channel<int> ch1, ch2, ch3;
    ch1.construct();
    ch2.construct(100);
    ch3.construct(50);

    std::uint64_t ch1_max{ch1.capacity()};
    std::uint64_t ch2_max{ch2.capacity()};
    std::uint64_t ch3_max{ch3.capacity()};

    EXPECT_EQ(ch1_max, 1);
    EXPECT_EQ(ch2_max, 100);
    EXPECT_EQ(ch3_max, 50);
}

TEST(buffered_channel, send)
{
    mce::buffered_channel<int> ch1, ch2, ch3, ch5;
    ch1.construct(50);
    ch2.construct(2);
    ch3.construct(5);
    ch5.construct();

    std::string test {"Test String"};
    mce::buffered_channel<std::string> ch4;
    ch4.construct();
    ch4.send(test);

    mce::thunk th1 = [ch1]
    {
        for(int i {0}; i < 50; ++i)
        {
            ch1.send(i);
        }
    };

    ch5.send(1500);
    EXPECT_FALSE(ch5.empty());
    EXPECT_EQ(ch5.size(), 1);

    mce::coroutine coro1(th1);
    while(!coro1.complete()){ coro1.run(); }
    EXPECT_FALSE(ch1.empty());
    EXPECT_EQ(ch1.size(), 50);

    int num1{5}, num2{10};
    ch2.send(num1);
    ch2.send(num2);
    EXPECT_FALSE(ch2.empty());
    EXPECT_EQ(ch2.size(), 2);

    std::vector<int> vec {2, 4, 6, 8, 10};
    mce::thunk th2 = [ch3, &vec]
    {
        for(auto &i : vec)
        {
            ch3.send(i);
        }
    };

    mce::coroutine coro2(th2);
    while(!coro2.complete()){ coro2.run(); }
    EXPECT_FALSE(ch3.empty());
    EXPECT_EQ(ch3.size(), 5);
}

TEST(buffered_channel, try_send)
{
    mce::buffered_channel<int> ch1, ch2;
    ch1.construct(1);
    ch2.construct(1);

    int num1 {5};
    int num2 {}, num3 {};
    bool added {true};

    mce::thunk th1 = [=, &added]
    {
        ch1.try_send(num1);
        ch2.try_send(25);
        if(ch1.try_send(10) == mce::result::failure)
        {
            added = false;
        }
    };

    mce::thunk th2 = [&, ch1, ch2]
    {
        ch1.recv(num2);
        ch2.recv(num3);
    };

    mce::thunk th3 = [&, ch1]
    {
        if(ch1.try_send(10) == mce::result::success)
        {
            added = true;
        }
        ch1.recv(num1);
    };

    std::thread thread1(th1);
    thread1.join();

    std::thread thread2(th2);
    thread2.join();

    EXPECT_EQ(num2, 5);
    EXPECT_EQ(num3, 25);
    EXPECT_FALSE(added);
    EXPECT_TRUE(ch1.empty());
    EXPECT_TRUE(ch2.empty());
    EXPECT_EQ(ch1.capacity(), 1);

    std::thread thread3(th3);
    thread3.join();

    EXPECT_EQ(num1, 10);
    EXPECT_TRUE(added);
}

TEST(buffered_channel, recv)
{
    mce::buffered_channel<int> ch;
    ch.construct(10);

    std::array<int, 10> arr;
    for(size_t i=0; i<arr.size(); ++i){ arr[i] = 0; }

    mce::thunk th_in = [&]
    {
        for(int i{0}; i < 10; ++i)
        {
            ch.send(i);
        }
    };

    mce::thunk th_out = [&]
    {
        for(int i{0}; i < 10; ++i)
        {
            ch.recv(arr[i]);
        }
    };

    mce::coroutine coro1(th_in);
    mce::coroutine coro2(th_out);

    for(int i=0; i<11; ++i)
    {
        coro1.run();
        coro2.run();
    }

    EXPECT_TRUE(coro1.complete());
    EXPECT_TRUE(coro2.complete());

    for(int i{0}; i<10; ++i)
    {
        EXPECT_EQ(arr[i], i);
    }
}

TEST(buffered_channel, try_recv)
{
    mce::buffered_channel<int> ch;
    ch.construct(2);

    int num1{20}, num2{50}, num3{5};
    bool test_fail{false}, test_pass{false}, test_pass2{false};

    if(ch.try_recv(num1) != mce::result::success)
    {
        test_fail = true;
    }

    ch.send(num1);
    ch.send(num2);
    if(ch.try_recv(num1) == mce::result::success)
    {
        test_pass = true;
    }

    if(ch.try_recv(num3) == mce::result::success)
    {
        test_pass2 = true;
    }

    EXPECT_TRUE(test_fail);
    EXPECT_TRUE(test_pass);
    EXPECT_TRUE(test_pass2);
    EXPECT_EQ(num1, 20);
    EXPECT_EQ(num3, 50);
}

TEST(buffered_channel, close_state)
{
    mce::buffered_channel<int> ch = mce::buffered_channel<int>::make();
    EXPECT_FALSE(ch.closed());
    ch.close();
    EXPECT_TRUE(ch.closed());
}

TEST(buffered_channel, close_unblocks_thread)
{
    mce::buffered_channel<int> ch = mce::buffered_channel<int>::make();

    std::thread thd([&](mce::buffered_channel<int> ch)
    {
        int i;
        while(ch.recv(i)){ } // loop until ch is closed 

    },ch);

    ch.close();
    thd.join();
}

TEST(buffered_channel, close_unblocks_coroutine_after_wait)
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
    mce::buffered_channel<int> ch = mce::buffered_channel<int>::make();
    mce::buffered_channel<int> done_ch = mce::buffered_channel<int>::make();

    cs->schedule([&](mce::buffered_channel<int> ch)
    {
        int i;
        while(ch.recv(i)){ } // loop until ch is closed 
        done_ch.send(1);
    },ch);

    std::thread thd([=]() mutable { cs->run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ch.close();
    int r;
    done_ch.recv(r);
    cs->halt();
    thd.join();
}

TEST(buffered_channel, close_unblocks_coroutines)
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
    mce::buffered_channel<int> ch1 = mce::buffered_channel<int>::make();
    mce::buffered_channel<int> ch2 = mce::buffered_channel<int>::make(0);
    mce::buffered_channel<int> ch3 = mce::buffered_channel<int>::make(1);
    mce::buffered_channel<int> done_ch = mce::buffered_channel<int>::make();

    auto listener = [&](mce::buffered_channel<int> ch)
    {
        int i;
        while(ch.recv(i)){ } // loop until ch is closed  
        done_ch.send(1);
    };

    cs->schedule(listener,ch1);
    cs->schedule(listener,ch2);
    cs->schedule(listener,ch3);

    std::thread thd([=]() mutable { cs->run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ch1.close();
    ch2.close();
    ch3.close();
    int r;
    done_ch.recv(r);
    done_ch.recv(r);
    done_ch.recv(r);
    cs->halt();
    thd.join();
}

TEST(buffered_channel, sequence)
{
    mce::buffered_channel<int> ch = mce::buffered_channel<int>::make(100);
    auto cs = mce::scheduler::make();
    std::thread thd([&]{ cs->run(); });

    cs->schedule([](mce::buffered_channel<int> ch)
    {
        for(int i=0; i<100; ++i){ ch.send(i); }
    },ch);

    for(int i=0; i<100; ++i)
    {
        int r;
        ch.recv(r);
        EXPECT_EQ(i,r);
    }

    cs->halt();
    thd.join();
}

TEST(buffered_channel, try_recv_till_fail)
{
    auto ch = mce::buffered_channel<int>::make(5);
    auto cs = mce::scheduler::make();

    auto sender = [=]
    {
        for(int i=0; i<10; ++i){ if(!ch.send(0)) { break; } }
    };

    auto try_recv_till_fail = [=]
    {
        int i;
        while(ch.try_recv(i) == mce::result::success){ }
        ch.close();
        mce::this_scheduler().halt();
    };

    cs->schedule(sender);
    cs->schedule(try_recv_till_fail);
    cs->run();
}

TEST(buffered_channel, try_recv_till_success)
{
    auto ch = mce::buffered_channel<int>::make(5);
    auto cs = mce::scheduler::make();

    auto sender = [=]
    {
        while(ch.send(0)){ }
    };

    auto try_recv_till_success = [=]
    {
        int i;
        while(ch.try_recv(i) == mce::result::failure){ }
        ch.close();
        mce::this_scheduler().halt();
    };

    cs->schedule(try_recv_till_success);
    cs->schedule(sender);
    cs->run();
}

TEST(buffered_channel, try_send_till_fail)
{
    auto ch = mce::buffered_channel<int>::make(5);
    auto cs = mce::scheduler::make();

    auto try_send_till_fail = [=]
    {
        int i = 0;
        while(ch.try_send(i) == mce::result::success){ }
        ch.close();
        mce::this_scheduler().halt();
    };

    auto receiver = [=]
    {
        int r;
        for(int i=0; i<10; ++i){ if(!ch.recv(r)){ break; } }
    };

    cs->schedule(receiver);
    cs->schedule(try_send_till_fail);
    cs->run();
}

TEST(buffered_channel, try_send_till_success)
{
    auto ch = mce::buffered_channel<int>::make(5);
    auto cs = mce::scheduler::make();

    auto try_send_till_success = [=]
    {
        while(ch.try_send(0) == mce::result::failure){ }
        ch.close();
        mce::this_scheduler().halt();
    };

    auto receiver = [=]
    {
        int i;
        while(ch.recv(i)){ }
    };

    cs->schedule(try_send_till_success);
    cs->schedule(receiver);
    cs->run();
}

TEST(buffered_channel, iterator_till_closed)
{
    mce::buffered_channel<size_t> ch = mce::buffered_channel<size_t>::make();
    auto cs = mce::scheduler::make();

    auto sender = [ch] 
    {
        size_t cnt = 0;
        while(cnt<10)
        {
            ch.send(cnt);
            ++cnt;
        }
        ch.close();
    };

    size_t recv_cnt = 0;
    auto receiver = [&,ch]
    {
        mce::buffered_channel<size_t>::iterator it = ch.begin();
        mce::buffered_channel<size_t>::iterator end = ch.end();
        while(it!=end)
        {
            EXPECT_EQ(recv_cnt,*it);
            ++recv_cnt;
            ++it;
        }
        mce::this_scheduler().halt();
    };

    cs->schedule(sender);
    cs->schedule(receiver);
    cs->run();

    EXPECT_TRUE(recv_cnt == 10 || recv_cnt == 9);
}

TEST(buffered_channel, iterator_till_closed_range_for)
{
    mce::buffered_channel<size_t> ch = mce::buffered_channel<size_t>::make();
    auto cs = mce::scheduler::make();

    auto sender = [ch] 
    {
        size_t cnt = 0;
        while(cnt<10)
        {
            ch.send(cnt);
            ++cnt;
        }
        ch.close();
    };


    size_t recv_cnt = 0;
    auto receiver = [&,ch]
    {
        for(auto& i : ch)
        {
            EXPECT_EQ(recv_cnt,i);
            ++recv_cnt;
        }
        mce::this_scheduler().halt();
    };

    cs->schedule(sender);
    cs->schedule(receiver);
    cs->run();

    EXPECT_TRUE(recv_cnt == 10 || recv_cnt == 9);
}

#undef RETVAL
