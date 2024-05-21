//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include <queue>
#include <deque>

#include "mce.hpp"

#include <gtest/gtest.h> 
#include <vector>
#include <iostream>
#include <mutex>
#include <string>

using namespace mce;

#define BILLION 1000000000
#define MILLION 1000000 
#define THOUSAND 1000   

#ifndef MCE_UT_LARGE_VALUE_MESSAGE_COUNT
#define MCE_UT_LARGE_VALUE_MESSAGE_COUNT 10000 
#endif 

#ifndef MCE_UT_CONGESTED_MESSAGE_COUNT
#define MCE_UT_CONGESTED_MESSAGE_COUNT 250 
#endif

void mutex_print_hardware_concurrency()
{
#if 0
    std::cout << "hardware_concurrency[" 
              << std::thread::hardware_concurrency() 
              << "]"
              << std::endl;
#endif
}

struct cv_data {
    inline void wait(std::unique_lock<mce::mutex>& lk) {
        flag = false;

        while(!flag) {
            cv.wait(lk);
        }
    }

    inline void notify() {
        if(!flag) {
            flag = true;
            cv.notify_one();
        } 
    }

private:
    bool flag = false;
    mce::condition_variable cv;
};

// a basic asynchronous-safe queue implementation using mce::mutex
template <typename T>
struct up_unbuf_queue {
    inline void send(const T& t) {
        std::unique_lock<mce::mutex> lk(mtx_);

        if(recv_q_.size()) {
            std::pair<void*,bool> p((void*)&t, false);
            recv_q_.front()((void*)&p);
            recv_q_.pop_front();
        } else {
            cv_data cd;

            send_q_.push_back([&](void* m) { 
                *((T*)m) = t; 
                cd.notify();
            });

            cd.wait(lk);
        }
    }

    inline void send(T&& t) {
        std::unique_lock<mce::mutex> lk(mtx_);

        if(recv_q_.size()) {
            send_pair_t p(&t, true);
            recv_q_.front()((void*)&p);
            recv_q_.pop_front();
        } else {
            cv_data cd;

            send_q_.push_back([&](void* m) { 
                *((T*)m) = std::move(t); 
                cd.notify();
            });

            cd.wait(lk);
        }
    }

    inline void recv(T& t) {
        std::unique_lock<mce::mutex> lk(mtx_);

        if(send_q_.size()) {
            send_q_.front()((void*)&t);
            send_q_.pop_front();
        } else {
            cv_data cd;

            recv_q_.push_back([&](void* m)
            {
                send_pair_t* p = (send_pair_t*)m;
                if(p->second) {
                    t = std::move(*((T*)(p->first)));
                } else {
                    t = *((const T*)(p->first));
                }

                cd.notify();
            });

            cd.wait(lk);
        }
    }

private:
    typedef std::pair<T*,bool> send_pair_t;
    typedef std::deque<std::function<void(void*)>> waiter_queue_t;
    mce::mutex mtx_;
    waiter_queue_t send_q_;
    waiter_queue_t recv_q_;
};


template <typename T>
struct up_buf_queue {

    template <typename T2>
    inline void send(T2&& t) {
        std::unique_lock<mce::mutex> lk(mtx_);
        vals_.push_back(std::forward<T2>(t));
        cd_.notify();
    }

    inline void recv(T& t) {
        std::unique_lock<mce::mutex> lk(mtx_);

        do {
            if(vals_.size()) {
                t = std::move(vals_.front());
                vals_.pop_front();
                break;
            } 

            cd_.wait(lk);
        } while(true); 
    }

private:
    typedef std::pair<T*,bool> send_pair_t;
    mce::mutex mtx_;
    std::deque<T> vals_;
    cv_data cd_;
};

template <typename T>
std::string key_value_str(std::string key, std::string value)
{
    return std::move(key) + std::string("[") + std::move(value) + std::string("]");
}

template <typename T>
std::string key_value_str(std::string key, T value)
{
    return std::move(key) + std::string("[") + std::to_string(std::move(value)) + std::string("]");
}

template <typename F, typename... As>
void mutex_launch_core_multiplier_op(F&& f, size_t multiplier, As&&... as)
{
    size_t core_count = std::thread::hardware_concurrency();
    size_t concurrent_count = multiplier * core_count;

    std::cout << key_value_str("core count", core_count) 
              << ", " 
              << key_value_str("concurrent operation count", concurrent_count) 
              << std::endl;

    f(concurrent_count, std::forward<As>(as)...);
}

class mutex_comparison_thread_computation: public ::testing::Test 
{
    protected:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown() { }
};

class mutex_comparison_thread_threadpool_computation: public ::testing::Test 
{
    protected:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown() { }
};

class mutex_comparison_thread_simple_communication: public ::testing::Test 
{
    protected:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown() { }
};

class mutex_comparison_thread_large_lvalue_communication: public ::testing::Test 
{
    protected:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown() { }
};

class mutex_comparison_thread_large_rvalue_communication: public ::testing::Test 
{
    protected:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown() { }
};

class mutex_comparison_thread_congested_communication: public ::testing::Test 
{
    protected:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown() { }
};

class mutex_comparison_parallel_computation: public ::testing::Test 
{
    public:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown(){}
};

class mutex_comparison_concurrent_computation: public ::testing::Test 
{
    public:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown(){}
};

class mutex_comparison_parallel_threadpool_computation: public ::testing::Test 
{
    public:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown(){}
};

class mutex_comparison_concurrent_threadpool_computation: public ::testing::Test 
{
    public:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown(){}
};

class mutex_comparison_concurrent_simple_communication: public ::testing::Test 
{
    public:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown(){}
};

class mutex_comparison_concurrent_large_lvalue_communication: public ::testing::Test 
{
    public:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown(){}
};

class mutex_comparison_concurrent_large_rvalue_communication: public ::testing::Test 
{
    public:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown(){}
};

class mutex_comparison_concurrent_congested_communication: public ::testing::Test 
{
    public:
        void SetUp() { mutex_print_hardware_concurrency(); }
        void TearDown(){}
};

void mutex_thread_simple_communication_op(std::uint64_t thread_total, 
                                    std::uint64_t recv_total)
{
    std::queue<std::thread> q;

    std::uint64_t repeat = thread_total/2;

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        auto ch0 = std::make_shared<up_unbuf_queue<int>>();
        auto ch1 = std::make_shared<up_unbuf_queue<int>>();

        thunk t0 = [ch0,ch1,recv_total]() mutable
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->send(recv);
                ch1->recv(message);
            }
        };
        
        thunk t1 = [ch0,ch1,recv_total]() mutable
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->recv(message);
                ch1->send(recv);
            }
        };

        std::thread thd0(t0);
        std::thread thd1(t1);

        q.push(std::move(thd0));
        q.push(std::move(thd1));
    }

    while(!q.empty())
    {
        q.front().join();
        q.pop();
    }
}

TEST_F(mutex_comparison_thread_simple_communication, 1x_core_count_threads_communicating_in_pairs_10000_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_simple_communication_op, 1, 10000);
}

TEST_F(mutex_comparison_thread_simple_communication, 2x_core_count_threads_communicating_in_pairs_10000_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_simple_communication_op, 2, 10000);
}

TEST_F(mutex_comparison_thread_simple_communication, 4x_core_count_threads_communicating_in_pairs_10000_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_simple_communication_op, 4, 10000);
}

TEST_F(mutex_comparison_thread_simple_communication, 8x_core_count_threads_communicating_in_pairs_10000_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_simple_communication_op, 8, 10000);
}

TEST_F(mutex_comparison_thread_simple_communication, 16x_core_count_threads_communicating_in_pairs_10000_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_simple_communication_op, 16, 10000);
}


void mutex_concurrent_simple_communication_op(std::uint64_t thread_total, 
                                std::uint64_t recv_total)
{
    std::queue<thunk> q;

    up_buf_queue<int> done_ch;

    std::uint64_t repeat = thread_total/2;

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        auto ch0 = std::make_shared<up_unbuf_queue<int>>();
        auto ch1 = std::make_shared<up_unbuf_queue<int>>();

        mce::thunk t1 = [ch0,ch1,recv_total,&done_ch]() mutable
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->recv(message);
                ch1->send(recv);
            }
            done_ch.send(0);
        };

        mce::thunk t0 = [ch0,ch1,recv_total,t1]() mutable
        {
            mce::concurrent(t1);

            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->send(recv);
                ch1->recv(message);
            }
        };

        mce::parallel(t0);
    }

    int x; // temp val
    for(std::uint64_t c=0; c<repeat; ++c) { done_ch.recv(x); }
}

TEST_F(mutex_comparison_concurrent_simple_communication, 1x_core_count_coroutines_communicating_in_pairs_10000_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_simple_communication_op, 1, 10000);
}

TEST_F(mutex_comparison_concurrent_simple_communication, 2x_core_count_coroutines_communicating_in_pairs_10000_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_simple_communication_op, 2, 10000);
}

TEST_F(mutex_comparison_concurrent_simple_communication, 4x_core_count_coroutines_communicating_in_pairs_10000_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_simple_communication_op, 4, 10000);
}

TEST_F(mutex_comparison_concurrent_simple_communication, 8x_core_count_coroutines_communicating_in_pairs_10000_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_simple_communication_op, 8, 10000);
}

TEST_F(mutex_comparison_concurrent_simple_communication, 16x_core_count_coroutines_communicating_in_pairs_10000_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_simple_communication_op, 16, 10000);
}

void mutex_thread_large_lvalue_communication_op(std::uint64_t thread_total, 
                                    std::uint64_t recv_total)
{
    std::cout << "mutex_thread_large_lvalue_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;
    std::queue<std::thread> q;

    std::uint64_t repeat = thread_total/2;

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        auto ch0 = std::make_shared<up_unbuf_queue<std::string>>();
        auto ch1 = std::make_shared<up_unbuf_queue<std::string>>();

        thunk t0 = [ch0,ch1,recv_total]() mutable
        {
            std::string send_message(100000,'3');
            std::string recv_message;
            std::string message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->send(send_message);
                ch1->recv(recv_message);
            }
        };
        
        thunk t1 = [ch0,ch1,recv_total]() mutable
        {
            std::string send_message(100000,'3');
            std::string recv_message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->recv(recv_message);
                ch1->send(send_message);
            }
        };

        std::thread thd0(t0);
        std::thread thd1(t1);

        q.push(std::move(thd0));
        q.push(std::move(thd1));
    }

    while(!q.empty())
    {
        q.front().join();
        q.pop();
    }
}

TEST_F(mutex_comparison_thread_large_lvalue_communication, 1x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_large_lvalue_communication_op, 1, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_thread_large_lvalue_communication, 2x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_large_lvalue_communication_op, 2, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_thread_large_lvalue_communication, 4x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_large_lvalue_communication_op, 4, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_thread_large_lvalue_communication, 8x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_large_lvalue_communication_op, 8, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}


void mutex_concurrent_large_lvalue_communication_op(std::uint64_t thread_total, 
                                std::uint64_t recv_total)
{
    std::cout << "mutex_concurrent_large_lvalue_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;
    up_buf_queue<int> done_ch;

    std::uint64_t repeat = thread_total/2;

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        auto ch0 = std::make_shared<up_unbuf_queue<std::string>>();
        auto ch1 = std::make_shared<up_unbuf_queue<std::string>>();
        
        mce::thunk t1 = [ch0,ch1,recv_total,&done_ch]() mutable
        {
            std::string send_message(100000,'3');
            std::string recv_message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->recv(recv_message);
                ch1->send(send_message);
            }
            done_ch.send(0);
        };

        mce::thunk t0 = [ch0,ch1,recv_total,t1]() mutable
        {
            mce::concurrent(t1);

            std::string send_message(100000,'3');
            std::string recv_message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->send(send_message);
                ch1->recv(recv_message);
            }
        };

        mce::parallel(t0);
    }

    int x; // temp val
    for(std::uint64_t c=0; c<repeat; ++c) { done_ch.recv(x); }
}

TEST_F(mutex_comparison_concurrent_large_lvalue_communication, 1x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_large_lvalue_communication_op, 1, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_concurrent_large_lvalue_communication, 2x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_large_lvalue_communication_op, 2, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_concurrent_large_lvalue_communication, 4x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_large_lvalue_communication_op, 4, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_concurrent_large_lvalue_communication, 8x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_large_lvalue_communication_op, 8, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

void mutex_thread_large_rvalue_communication_op(std::uint64_t thread_total, 
                                    std::uint64_t recv_total)
{
    std::cout << "mutex_thread_large_rvalue_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;
    std::queue<std::thread> q;

    std::uint64_t repeat = thread_total/2;

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        auto ch0 = std::make_shared<up_unbuf_queue<std::string>>();
        auto ch1 = std::make_shared<up_unbuf_queue<std::string>>();

        thunk t0 = [ch0,ch1,recv_total]() mutable
        {
            std::string message(100000,'3');
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->send(std::move(message));
                ch1->recv(message);
            }
        };
        
        thunk t1 = [ch0,ch1,recv_total]() mutable
        {
            std::string message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->recv(message);
                ch1->send(std::move(message));
            }
        };

        std::thread thd0(t0);
        std::thread thd1(t1);

        q.push(std::move(thd0));
        q.push(std::move(thd1));
    }

    while(!q.empty())
    {
        q.front().join();
        q.pop();
    }
}

TEST_F(mutex_comparison_thread_large_rvalue_communication, 1x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_large_rvalue_communication_op, 1, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_thread_large_rvalue_communication, 2x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_large_rvalue_communication_op, 2, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_thread_large_rvalue_communication, 4x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_large_rvalue_communication_op, 4, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_thread_large_rvalue_communication, 8x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_thread_large_rvalue_communication_op, 8, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}


void mutex_concurrent_large_rvalue_communication_op(std::uint64_t thread_total, 
                                std::uint64_t recv_total)
{
    std::cout << "mutex_concurrent_large_rvalue_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;
    up_buf_queue<int> done_ch;

    std::uint64_t repeat = thread_total/2;

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        auto ch0 = std::make_shared<up_unbuf_queue<std::string>>();
        auto ch1 = std::make_shared<up_unbuf_queue<std::string>>();
        
        thunk t1 = [ch0,ch1,recv_total,&done_ch]() mutable
        {
            std::string message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->recv(message);
                ch1->send(std::move(message));
            }
            done_ch.send(0);
        };

        thunk t0 = [ch0,ch1,recv_total,t1]() mutable
        {
            mce::concurrent(t1);

            std::string message(100000,'3');
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->send(std::move(message));
                ch1->recv(message);
            }
        };

        mce::parallel(t0);
    }

    int x; // temp val
    for(std::uint64_t c=0; c<repeat; ++c) { done_ch.recv(x); }
}

TEST_F(mutex_comparison_concurrent_large_rvalue_communication, 1x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_large_rvalue_communication_op, 1, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_concurrent_large_rvalue_communication, 2x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_large_rvalue_communication_op, 2, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_concurrent_large_rvalue_communication, 4x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_large_rvalue_communication_op, 4, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_concurrent_large_rvalue_communication, 8x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_large_rvalue_communication_op, 8, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

void mutex_thread_congested_communication_op(std::uint64_t thread_total,
                                       std::uint64_t recv_total)
{
    std::cout << "mutex_thread_congested_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;
    std::queue<std::thread> q;

    std::uint64_t repeat = thread_total/2;

    auto ch0 = std::make_shared<up_unbuf_queue<int>>();
    auto ch1 = std::make_shared<up_unbuf_queue<int>>();

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        thunk t0 = [ch0,ch1,recv_total]() mutable
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->send(recv);
                ch1->recv(message);
            }
        };
        
        thunk t1 = [ch0,ch1,recv_total]() mutable
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->recv(message);
                ch1->send(recv);
            }
        };

        std::thread thd0(t0);
        std::thread thd1(t1);

        q.push(std::move(thd0));
        q.push(std::move(thd1));
    }

    while(!q.empty())
    {
        q.front().join();
        q.pop();
    }
}

TEST_F(mutex_comparison_thread_congested_communication, 1x_core_count_threads_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_thread)
{
    mutex_launch_core_multiplier_op(mutex_thread_congested_communication_op, 1, MCE_UT_CONGESTED_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_thread_congested_communication, 2x_core_count_threads_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_thread)
{
    mutex_launch_core_multiplier_op(mutex_thread_congested_communication_op, 2, MCE_UT_CONGESTED_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_thread_congested_communication, 3x_core_count_threads_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_thread)
{
    mutex_launch_core_multiplier_op(mutex_thread_congested_communication_op, 3, MCE_UT_CONGESTED_MESSAGE_COUNT);
}

void mutex_concurrent_congested_communication_op(std::uint64_t thread_total, std::uint64_t recv_total)
{
    std::cout << "mutex_concurrent_congested_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;
    up_buf_queue<int> done_ch;

    std::uint64_t repeat = thread_total/2;

    auto ch0 = std::make_shared<up_unbuf_queue<int>>();
    auto ch1 = std::make_shared<up_unbuf_queue<int>>();

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        thunk t1 = [ch0,ch1,recv_total,&done_ch]() mutable
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->recv(message);
                ch1->send(recv);
            }
            done_ch.send(0);
        };

        thunk t0 = [ch0,ch1,recv_total,t1]() mutable
        {
            mce::concurrent(t1);
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0->send(recv);
                ch1->recv(message);
            }
        };

        mce::parallel(t0);
    }

    int x; // temp val
    std::uint64_t c=0;
    for(; c<repeat; ++c) { done_ch.recv(x); }
}


TEST_F(mutex_comparison_concurrent_congested_communication, 1x_core_count_coroutines_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_coroutine)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_congested_communication_op, 1, MCE_UT_CONGESTED_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_concurrent_congested_communication, 2x_core_count_coroutines_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_coroutine)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_congested_communication_op, 2, MCE_UT_CONGESTED_MESSAGE_COUNT);
}

TEST_F(mutex_comparison_concurrent_congested_communication, 3x_core_count_coroutines_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_coroutine)
{
    mutex_launch_core_multiplier_op(mutex_concurrent_congested_communication_op, 3, MCE_UT_CONGESTED_MESSAGE_COUNT);
}
