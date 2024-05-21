//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "function_utility.hpp"
#include "scheduler.hpp"
#include "unbuffered_channel.hpp"

#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

#include <iostream>

#include <gtest/gtest.h>

#define RETVAL 15
#define RETVAL2 6

TEST(unbuffered_channel, unbuffered_channel_context)
{
    mce::unbuffered_channel<int> ch;
    EXPECT_EQ(ch.context(),(void*)NULL);
    ch.construct();
    EXPECT_NE(ch.context(),(void*)NULL);
}

TEST(unbuffered_channel, unbuffered_channel_type_info)
{
    {
        mce::unbuffered_channel<int> ch;
        EXPECT_NE(typeid(int), ch.type_info());
        EXPECT_NE(typeid(std::string), ch.type_info());
        EXPECT_EQ(typeid(mce::unbuffered_channel<int>), ch.type_info());
    }

    {
        mce::unbuffered_channel<std::string> ch;
        EXPECT_NE(typeid(int), ch.type_info());
        EXPECT_NE(typeid(std::string), ch.type_info());
        EXPECT_EQ(typeid(mce::unbuffered_channel<std::string>), ch.type_info());
    }
}

TEST(unbuffered_channel, unbuffered_channel_co_send_co_recv) 
{
    mce::unbuffered_channel<int> ch;
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

    // iterate coroutines until unbuffered_channel send/recv completes
    while(!cq.empty())
    {
        std::shared_ptr<mce::coroutine> co_ptr = cq.front();
        cq.pop();

        co_ptr->run();
        if(!co_ptr->complete()) { cq.push(co_ptr); }
    }

    EXPECT_TRUE(ch_out == RETVAL);
}


// Test that only one sendter and only one recvter can access unbuffered_channel at a time
TEST(unbuffered_channel, unbuffered_channel_multiple_co_send_co_recv)
{
    mce::unbuffered_channel<int> ch;
    ch.construct();

    int ch_out = 0;

    mce::thunk t_send0 = [ch]{ ch.send(RETVAL); };
    mce::thunk t_send1 = [ch]{ ch.send(RETVAL2); };
    mce::thunk t_recv = [&ch_out,ch] { ch.recv(ch_out); };

    std::queue<mce::coroutine> tasks;
    tasks.push(mce::coroutine(t_send0));
    tasks.push(mce::coroutine(t_send1));
    tasks.push(mce::coroutine(t_recv));
    tasks.push(mce::coroutine(t_recv));

    EXPECT_TRUE(ch_out == 0);

    while(!tasks.empty())
    {
        mce::coroutine& c = tasks.front();
        c.run();
        if(!c.complete()) { tasks.push(std::move(c)); }
        tasks.pop();
    }
    
    EXPECT_TRUE(ch_out == RETVAL2 || ch_out == RETVAL);
}

// reverse order recv/send
TEST(unbuffered_channel, unbuffered_channel_co_recv_co_send) 
{
    mce::unbuffered_channel<int> ch;
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

    // iterate coroutines until unbuffered_channel send/recv completes
    while(!cq.empty())
    {
        std::shared_ptr<mce::coroutine> co_ptr = cq.front();
        cq.pop();

        co_ptr->run();
        if(!co_ptr->complete()) { cq.push(co_ptr); }
    }

    EXPECT_TRUE(ch_out == RETVAL);
}

TEST(unbuffered_channel, unbuffered_channel_co_send_thread_recv) 
{
    int ch_out = 0;

    mce::unbuffered_channel<int> ch;
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

// Reverse which is the coroutine and which is the thread
TEST(unbuffered_channel, unbuffered_channel_thread_send_co_recv) 
{
    int ch_out = 0;

    mce::unbuffered_channel<int> ch;
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
TEST(unbuffered_channel, unbuffered_channel_co_recv_thread_send) 
{
    int ch_out = 0;

    mce::unbuffered_channel<int> ch;
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

// reverse order with coroutine and thread switched
TEST(unbuffered_channel, unbuffered_channel_thread_recv_co_send) 
{
    int ch_out = 0;

    mce::unbuffered_channel<int> ch;
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

TEST(unbuffered_channel, unbuffered_channel_thread_send_thread_recv) 
{
    int ch_out = 0;

    mce::unbuffered_channel<int> ch;
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
TEST(unbuffered_channel, unbuffered_channel_thread_recv_thread_send) 
{
    int ch_out = 0;

    mce::unbuffered_channel<int> ch;
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

TEST(unbuffered_channel, competing_unbuffered_channels_by_copy)
{
    mce::unbuffered_channel<int> ch0, ch1;
    mce::unbuffered_channel<std::string> ch2;

    ch0.construct();
    ch1.construct();
    ch2.construct();

    int ret0 = -1;
    int ret1 = -1;
    std::string ret2 = "";

    mce::thunk t0 = [ch0, ch1, &ret0]
    {
        ch0.send(0);
        ch1.recv(ret0); // <-- blocked here
    };

    mce::thunk t1 = [ch0, ch1, ch2, &ret1]
    {
        ch0.recv(ret1); // <-- blocked here
        ch1.send(1);
        ch2.send(std::string("competing_unbuffered_channels"));
    };

    mce::thunk t2 = [ch2, &ret2]
    {
        ch2.recv(ret2); // <-- blocked here
    };

    std::queue<std::shared_ptr<mce::coroutine>> coros;

    // add in reverse of expected execution order
    coros.push(std::make_shared<mce::coroutine>(t2));
    coros.push(std::make_shared<mce::coroutine>(t1));
    coros.push(std::make_shared<mce::coroutine>(t0));

    while(!coros.empty())
    {
        std::shared_ptr<mce::coroutine> c = std::move(coros.front());
        coros.pop();

        c->run();
        if(!c->complete()) { coros.push(std::move(c)); }
    }

    EXPECT_EQ(ret0,1);
    EXPECT_EQ(ret1,0);
    EXPECT_EQ(ret2,"competing_unbuffered_channels");
}

TEST(unbuffered_channel, memory_independence)
{
    mce::unbuffered_channel<int> ch0, ch1;
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

TEST(unbuffered_channel, custom_object)
{
    //custom object
    class Grocery
    {
        private:
            std::string name_;
            double cost_;
            int amount_;
        public:
            Grocery() : name_{"Missing"}, cost_{0.00}, amount_{0} {}
            Grocery(std::string name, double cost, int amount) : name_{name}, cost_{cost}, amount_{amount} {}
            Grocery(const Grocery& source) : name_{source.name_}, cost_{source.cost_}, amount_{source.amount_} {}
            std::string recv_name() { return name_; }
            double recv_cost() { return cost_; }
            int recv_amount() { return amount_; }
            void set_name(std::string name) { name_ = name; }
            void set_cost(double cost) { cost_ = cost; }
            void increment_amount() { ++amount_; } 
    };

    //creating unbuffered_channel
    mce::unbuffered_channel<Grocery> ch;
    ch.construct();

    //variables
    std::vector<Grocery> grocery_list {};
    std::queue<Grocery> groceries {};
    groceries.emplace("Apple", 0.75, 3);
    groceries.emplace("Eggs", 1.20, 12);
    groceries.emplace("Steak", 4.99, 1);
    groceries.back().increment_amount();

    //functions for execution
    mce::thunk th1 = [ch, &groceries]
    {
        while(!groceries.empty())
        {
            ch.send(groceries.front());
            groceries.pop();
        }
    };

    mce::thunk th2 = [ch, &grocery_list]
    {
        Grocery r;
        ch.recv(r);
        grocery_list.push_back(std::move(r));
        ch.recv(r);
        grocery_list.push_back(std::move(r));
        ch.recv(r);
        grocery_list.push_back(std::move(r));
    };

    //coroutines
    std::queue<std::unique_ptr<mce::coroutine>> co_que;
    co_que.emplace(new mce::coroutine(th1));
    co_que.emplace(new mce::coroutine(th2));

    while(!co_que.empty())
    {
        std::unique_ptr<mce::coroutine>& c = co_que.front();
        c->run();
        if(!c->complete())
        {
            co_que.emplace(std::move(c));
        }
        co_que.pop();
    }

    EXPECT_EQ(co_que.size(), 0);
    EXPECT_EQ(grocery_list.size(), 3);
    EXPECT_EQ(grocery_list[2].recv_name(), "Steak");
    EXPECT_EQ(grocery_list[2].recv_amount(), 2);
    EXPECT_EQ(grocery_list[0].recv_cost(), 0.75);
}

TEST(unbuffered_channel, overloaded_operators_and_send)
{
    mce::unbuffered_channel<int> ch1;
    mce::unbuffered_channel<std::string> ch2;
    mce::unbuffered_channel<std::string> ch3;
    ch1.construct();
    ch2.construct();
    ch3.construct();

    int num {5};
    std::string test {"Test String"};
    int recv_num;
    std::string recv_string1;
    std::string recv_string2;

    mce::thunk th1 = [&, ch1, ch2, ch3]
    {
        ch1.send(num);
        ch2.send(test);
        ch3.send("unbuffered_channel String");
    };

    mce::thunk th2 = [&, ch1, ch2, ch3]
    {
        ch1.recv(recv_num);
        ch2.recv(recv_string1);
        ch3.recv(recv_string2);
    };

    std::queue<std::unique_ptr<mce::coroutine>> task_q;
    task_q.emplace(new mce::coroutine(th1));
    task_q.emplace(new mce::coroutine(th2));

    while(!task_q.empty())
    {
        std::unique_ptr<mce::coroutine>& task = task_q.front();
        task->run();
        if(!task->complete())
        {
           task_q.emplace(std::move(task)); 
        }
        task_q.pop();
    }

    EXPECT_EQ(recv_num, 5);
    EXPECT_EQ(recv_string1, "Test String");
    EXPECT_EQ(recv_string2, "unbuffered_channel String");
}

TEST(unbuffered_channel, close_state)
{
    mce::unbuffered_channel<int> ch = mce::unbuffered_channel<int>::make();
    EXPECT_FALSE(ch.closed());
    ch.close();
    EXPECT_TRUE(ch.closed());
}
    
TEST(unbuffered_channel, close_unblocks_thread)
{
    mce::unbuffered_channel<int> ch = mce::unbuffered_channel<int>::make();

    std::thread thd([&](mce::unbuffered_channel<int> ch)
    {
        int r;
        while(ch.recv(r)) { } // loop until ch is closed  
    },ch);

    ch.close();
    thd.join();
}

TEST(unbuffered_channel, close_unblocks_coroutine_after_wait)
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
    mce::unbuffered_channel<int> ch = mce::unbuffered_channel<int>::make();
    mce::unbuffered_channel<int> done_ch = mce::unbuffered_channel<int>::make();

    cs->schedule([&](mce::unbuffered_channel<int> ch)
    {
        int r;
        while(ch.recv(r)) { } // loop until ch is closed  
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

TEST(unbuffered_channel, close_unblocks_coroutines)
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
    mce::unbuffered_channel<int> ch1 = mce::unbuffered_channel<int>::make();
    mce::unbuffered_channel<int> ch2 = mce::unbuffered_channel<int>::make();
    mce::unbuffered_channel<int> ch3 = mce::unbuffered_channel<int>::make();
    mce::unbuffered_channel<int> done_ch = mce::unbuffered_channel<int>::make();

    auto listener = [&](mce::unbuffered_channel<int> ch)
    {
        int r;
        while(ch.recv(r)) { } // loop until ch is closed  
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

TEST(unbuffered_channel, try_recv_till_fail)
{
    auto ch = mce::unbuffered_channel<int>::make();
    auto cs = mce::scheduler::make();

    auto sender = [=]
    {
        while(ch.send(0)){ }
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

TEST(unbuffered_channel, try_recv_till_success)
{
    auto ch = mce::unbuffered_channel<int>::make();
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

TEST(unbuffered_channel, try_send_till_fail)
{
    auto ch = mce::unbuffered_channel<int>::make();
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
        while(ch.recv(r)) { } // loop until ch is closed  
    };

    cs->schedule(receiver);
    cs->schedule(try_send_till_fail);
    cs->run();
}

TEST(unbuffered_channel, try_send_till_success)
{
    auto ch = mce::unbuffered_channel<int>::make();
    auto cs = mce::scheduler::make();

    auto try_send_till_success = [=]
    {
        while(ch.try_send(0) == mce::result::failure){ }
        ch.close();
        mce::this_scheduler().halt();
    };

    auto receiver = [=]
    {
        int r;
        while(ch.recv(r)) { } // loop until ch is closed  
    };

    cs->schedule(try_send_till_success);
    cs->schedule(receiver);
    cs->run();
}

TEST(unbuffered_channel, iterator_till_closed)
{
    mce::unbuffered_channel<size_t> ch = mce::unbuffered_channel<size_t>::make();
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
        mce::unbuffered_channel<size_t>::iterator it = ch.begin();
        mce::unbuffered_channel<size_t>::iterator end = ch.end();
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

TEST(unbuffered_channel, iterator_till_closed_range_for)
{
    mce::unbuffered_channel<size_t> ch = mce::unbuffered_channel<size_t>::make();
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
#undef RETVAL2
