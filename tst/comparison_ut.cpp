//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include <queue>

#include "mce.hpp"

#include <gtest/gtest.h> 
#include <vector>
#include <iostream>
#include <condition_variable>
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

void print_hardware_concurrency()
{
#if 0
    std::cout << "hardware_concurrency[" 
              << std::thread::hardware_concurrency() 
              << "]"
              << std::endl;
#endif
}

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
void launch_core_multiplier_op(F&& f, size_t multiplier, As&&... as)
{
    size_t core_count = std::thread::hardware_concurrency();
    size_t concurrent_count = multiplier * core_count;

    std::cout << key_value_str("core count", core_count) 
              << ", " 
              << key_value_str("concurrent operation count", concurrent_count) 
              << std::endl;

    f(concurrent_count, std::forward<As>(as)...);
}

class comparison_thread_computation: public ::testing::Test 
{
    protected:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown() { }
};

class comparison_thread_threadpool_computation: public ::testing::Test 
{
    protected:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown() { }
};

class comparison_thread_simple_communication: public ::testing::Test 
{
    protected:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown() { }
};

class comparison_thread_large_lvalue_communication: public ::testing::Test 
{
    protected:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown() { }
};

class comparison_thread_large_rvalue_communication: public ::testing::Test 
{
    protected:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown() { }
};

class comparison_thread_congested_communication: public ::testing::Test 
{
    protected:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown() { }
};

class comparison_parallel_computation: public ::testing::Test 
{
    public:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown(){}
};

class comparison_concurrent_computation: public ::testing::Test 
{
    public:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown(){}
};

class comparison_parallel_threadpool_computation: public ::testing::Test 
{
    public:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown(){}
};

class comparison_concurrent_threadpool_computation: public ::testing::Test 
{
    public:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown(){}
};

class comparison_concurrent_simple_communication: public ::testing::Test 
{
    public:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown(){}
};

class comparison_concurrent_large_lvalue_communication: public ::testing::Test 
{
    public:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown(){}
};

class comparison_concurrent_large_rvalue_communication: public ::testing::Test 
{
    public:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown(){}
};

class comparison_concurrent_congested_communication: public ::testing::Test 
{
    public:
        void SetUp() { print_hardware_concurrency(); }
        void TearDown(){}
};

void count_test(std::uint64_t target,
                spinlock& l,
                scheduler::parkable& p,
                std::uint64_t& completed_task_count,
                const std::uint64_t completed_task_target)
{
    std::uint64_t c=0;
    while(c<target) { ++c; }

    std::unique_lock<mce::spinlock> lk(l);
    ++completed_task_count;

    if(completed_task_count >= completed_task_target)
    {
        p.unpark(lk);
    }
}

void thread_computation_op(std::uint64_t task_num, std::uint64_t target)
{
    std::uint64_t completed_task_count = 0;
    spinlock l;
    scheduler::parkable p;
    
    thunk t = [&]
    { 
        count_test(target, 
                   l,
                   p,
                   completed_task_count,
                   task_num); 
    };

    std::vector<std::thread> workers(task_num);
    std::thread launcher;

    std::unique_lock<mce::spinlock> lk(l);

    launcher = std::thread([&]
    {
        for(auto& w : workers)
        {
            w = std::thread(t);
            w.detach();
        }
    });

    launcher.detach();
    p.park(lk);
}

TEST_F(comparison_thread_computation, 1x_core_count_threads_count_to_1000000000)
{
    launch_core_multiplier_op(thread_computation_op, 1, BILLION);
}

TEST_F(comparison_thread_computation, 10x_core_count_threads_count_to_1000000000)
{
    launch_core_multiplier_op(thread_computation_op, 10, BILLION);
}

TEST_F(comparison_thread_computation, 100x_core_count_threads_count_to_1000000000)
{
    launch_core_multiplier_op(thread_computation_op, 100, BILLION);
}

TEST_F(comparison_thread_computation, 1000x_core_count_threads_count_to_1000000000)
{
    launch_core_multiplier_op(thread_computation_op, 1000, BILLION);
}

void parallel_computation_op(std::uint64_t task_num, std::uint64_t target)
{
    std::uint64_t completed_task_count = 0;
    spinlock l;
    scheduler::parkable p;

    thunk t = [&]
    { 
        count_test(target, 
                   l,
                   p,
                   completed_task_count,
                   task_num); 
    };

    std::unique_lock<mce::spinlock> lk(l);

    mce::parallel([&]
    {
        for(std::uint64_t c=0; c<task_num; ++c)
        {
            mce::parallel(t);
        }
    });

    p.park(lk);
}

TEST_F(comparison_parallel_computation, 1x_core_count_coroutines_count_to_1000000000)
{
    launch_core_multiplier_op(parallel_computation_op, 1, BILLION);
}

TEST_F(comparison_parallel_computation, 10x_core_count_coroutines_count_to_1000000000)
{
    launch_core_multiplier_op(parallel_computation_op, 10, BILLION);
}

TEST_F(comparison_parallel_computation, 100x_core_count_coroutines_count_to_1000000000)
{
    launch_core_multiplier_op(parallel_computation_op, 100, BILLION);
}

TEST_F(comparison_parallel_computation, 1000x_core_count_coroutines_count_to_1000000000)
{
    launch_core_multiplier_op(parallel_computation_op, 1000, BILLION);
}

void concurrent_computation_op(std::uint64_t task_num, std::uint64_t target)
{
    std::uint64_t completed_task_count = 0;
    spinlock l;
    scheduler::parkable p;

    thunk t = [&]
    { 
        count_test(target, 
                   l,
                   p,
                   completed_task_count,
                   task_num); 
    };

    std::unique_lock<mce::spinlock> lk(l);

    mce::concurrent([&]
    {
        for(std::uint64_t c=0; c<task_num; ++c)
        {
            mce::concurrent(t);
        }
    });

    p.park(lk);
}

TEST_F(comparison_concurrent_computation, 1x_core_count_coroutines_count_to_1000000000)
{
    launch_core_multiplier_op(concurrent_computation_op, 1, BILLION);
}

TEST_F(comparison_concurrent_computation, 10x_core_count_coroutines_count_to_1000000000)
{
    launch_core_multiplier_op(concurrent_computation_op, 10, BILLION);
}

TEST_F(comparison_concurrent_computation, 100x_core_count_coroutines_count_to_1000000000)
{
    launch_core_multiplier_op(concurrent_computation_op, 100, BILLION);
}

TEST_F(comparison_concurrent_computation, 1000x_core_count_coroutines_count_to_1000000000)
{
    launch_core_multiplier_op(concurrent_computation_op, 1000, BILLION);
}


void std_threadpool_count_op(std::uint64_t task_num, std::uint64_t target)
{
    std::uint64_t thread_cnt = std::thread::hardware_concurrency();
    std::uint64_t task_num_per_thread = task_num / thread_cnt;
    std::uint64_t leftover = task_num - thread_cnt *task_num_per_thread;

    std::uint64_t completed_task_count = 0;
    spinlock l;
    scheduler::parkable p;

    auto count_f = [&](std::uint64_t task_count)
    { 
        for(std::uint64_t c=0; c<task_count; ++c)
        {
            count_test(target, 
                       l,
                       p,
                       completed_task_count,
                       task_num); 
        }
    };

    std::vector<std::thread> workers(thread_cnt);
    std::thread launcher;
    
    {
        std::unique_lock<mce::spinlock> lk(l);

        launcher = std::thread([&]
        {
            for(auto& w : workers)
            {
                std::uint64_t task_count = task_num_per_thread;
                if(leftover)
                {
                    ++task_count;
                    --leftover;
                }

                w = std::thread(count_f, task_count);
                w.detach();
            }
        });

        p.park(lk);
    }

    launcher.detach();
}

TEST_F(comparison_thread_threadpool_computation, std_threadpool_count_to_1000000000_1000_times)
{
    std_threadpool_count_op(1000,BILLION);
}

TEST_F(comparison_thread_threadpool_computation, std_threadpool_count_to_1000000000_10000_times)
{
    std_threadpool_count_op(10000,BILLION);
}

TEST_F(comparison_thread_threadpool_computation, std_threadpool_count_to_1000000000_100000_times)
{
    std_threadpool_count_op(100000,BILLION);
}

TEST_F(comparison_thread_threadpool_computation, std_threadpool_count_to_1000000000_1000000_times)
{
    std_threadpool_count_op(1000000,BILLION);
}

void parallel_threadpool_count_op(std::uint64_t task_num, std::uint64_t target)
{
    std::uint64_t thread_cnt = default_threadpool().size();
    std::uint64_t task_num_per_thread = task_num / thread_cnt;
    std::uint64_t leftover = task_num - thread_cnt * task_num_per_thread;

    std::uint64_t completed_task_count = 0;
    spinlock l;
    scheduler::parkable p;

    std::function<void(std::uint64_t)> f = [&] 
    (std::uint64_t task_num_per_thread)
    { 
        for(std::uint64_t c=0; c<task_num_per_thread; ++c)
        {
            count_test(target, 
                       l,
                       p,
                       completed_task_count,
                       task_num); 
        }
    };

    std::unique_lock<mce::spinlock> lk(l);

    mce::parallel([&]
    {
        for(std::uint64_t c=0; 
            c<thread_cnt; 
            ++c) 
        { 
            if(leftover) 
            { 
                parallel(f,task_num_per_thread+1); 
                --leftover;
            }
            else { parallel(f,task_num_per_thread); }
        }
    });

    p.park(lk);
}

TEST_F(comparison_parallel_threadpool_computation, coroutine_threadpool_count_to_1000000000_1000_times)
{
    parallel_threadpool_count_op(1000,BILLION);
}

TEST_F(comparison_parallel_threadpool_computation, coroutine_threadpool_count_to_1000000000_10000_times)
{
    parallel_threadpool_count_op(10000,BILLION);
}

TEST_F(comparison_parallel_threadpool_computation, coroutine_threadpool_count_to_1000000000_100000_times)
{
    parallel_threadpool_count_op(100000,BILLION);
}

TEST_F(comparison_parallel_threadpool_computation, coroutine_threadpool_count_to_1000000000_1000000_times)
{
    parallel_threadpool_count_op(1000000,BILLION);
}

void concurrent_threadpool_count_op(std::uint64_t task_num, std::uint64_t target)
{
    std::uint64_t thread_cnt = default_threadpool().size();
    std::uint64_t task_num_per_thread = task_num / thread_cnt;
    std::uint64_t leftover = task_num - thread_cnt * task_num_per_thread;

    std::uint64_t completed_task_count = 0;
    spinlock l;
    scheduler::parkable p;

    std::function<void(std::uint64_t)> f = [&] 
    (std::uint64_t task_num_per_thread)
    { 
        for(std::uint64_t c=0; c<task_num_per_thread; ++c)
        {
            count_test(target, 
                       l,
                       p,
                       completed_task_count,
                       task_num); 
        }
    };

    {
        std::unique_lock<mce::spinlock> lk(l);

        mce::concurrent([&]
        {
            for(std::uint64_t c=0; 
                c<thread_cnt; 
                ++c) 
            { 
                if(leftover) 
                { 
                    mce::concurrent(f,task_num_per_thread+1); 
                    --leftover;
                }
                else { mce::concurrent(f,task_num_per_thread); }
            }

        });

        p.park(lk);
    }
}

TEST_F(comparison_concurrent_threadpool_computation, coroutine_threadpool_count_to_1000000000_1000_times)
{
    concurrent_threadpool_count_op(1000,BILLION);
}

TEST_F(comparison_concurrent_threadpool_computation, coroutine_threadpool_count_to_1000000000_10000_times)
{
    concurrent_threadpool_count_op(10000,BILLION);
}

TEST_F(comparison_concurrent_threadpool_computation, coroutine_threadpool_count_to_1000000000_100000_times)
{
    concurrent_threadpool_count_op(100000,BILLION);
}

TEST_F(comparison_concurrent_threadpool_computation, coroutine_threadpool_count_to_1000000000_1000000_times)
{
    concurrent_threadpool_count_op(1000000,BILLION);
}

void thread_simple_communication_op(std::uint64_t thread_total, 
                                    std::uint64_t recv_total)
{
    std::queue<std::thread> q;

    std::uint64_t repeat = thread_total/2;

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        unbuffered_channel<int> ch0, ch1;
        ch0.construct();
        ch1.construct();

        thunk t0 = [ch0,ch1,recv_total]
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.send(recv);
                ch1.recv(message);
            }
        };
        
        thunk t1 = [ch0,ch1,recv_total]
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.recv(message);
                ch1.send(recv);
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

TEST_F(comparison_thread_simple_communication, 1x_core_count_threads_communicating_in_pairs_10000_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_simple_communication_op, 1, 10000);
}

TEST_F(comparison_thread_simple_communication, 2x_core_count_threads_communicating_in_pairs_10000_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_simple_communication_op, 2, 10000);
}

TEST_F(comparison_thread_simple_communication, 4x_core_count_threads_communicating_in_pairs_10000_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_simple_communication_op, 4, 10000);
}

TEST_F(comparison_thread_simple_communication, 8x_core_count_threads_communicating_in_pairs_10000_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_simple_communication_op, 8, 10000);
}

TEST_F(comparison_thread_simple_communication, 16x_core_count_threads_communicating_in_pairs_10000_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_simple_communication_op, 16, 10000);
}


void concurrent_simple_communication_op(std::uint64_t thread_total, 
                                std::uint64_t recv_total)
{
    std::queue<thunk> q;

    mce::buffered_channel<int> done_ch;
    done_ch.construct(std::thread::hardware_concurrency());

    std::uint64_t repeat = thread_total/2;

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        mce::unbuffered_channel<int> ch0, ch1;
        ch0.construct();
        ch1.construct();

        mce::thunk t1 = [ch0,ch1,recv_total,done_ch]
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.recv(message);
                ch1.send(recv);
            }
            done_ch.send(0);
        };

        mce::thunk t0 = [ch0,ch1,recv_total,t1]
        {
            mce::concurrent(t1);

            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.send(recv);
                ch1.recv(message);
            }

        };

        mce::parallel(t0);
    }

    int x; // temp val
    for(std::uint64_t c=0; c<repeat; ++c) { done_ch.recv(x); }
}

TEST_F(comparison_concurrent_simple_communication, 1x_core_count_coroutines_communicating_in_pairs_10000_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_simple_communication_op, 1, 10000);
}

TEST_F(comparison_concurrent_simple_communication, 2x_core_count_coroutines_communicating_in_pairs_10000_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_simple_communication_op, 2, 10000);
}

TEST_F(comparison_concurrent_simple_communication, 4x_core_count_coroutines_communicating_in_pairs_10000_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_simple_communication_op, 4, 10000);
}

TEST_F(comparison_concurrent_simple_communication, 8x_core_count_coroutines_communicating_in_pairs_10000_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_simple_communication_op, 8, 10000);
}

TEST_F(comparison_concurrent_simple_communication, 16x_core_count_coroutines_communicating_in_pairs_10000_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_simple_communication_op, 16, 10000);
}

void thread_large_lvalue_communication_op(std::uint64_t thread_total, 
                                    std::uint64_t recv_total)
{
    std::cout << "thread_large_lvalue_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;
    std::queue<std::thread> q;

    std::uint64_t repeat = thread_total/2;
    
    for(std::uint64_t c=0; c<repeat; ++c)
    {
        unbuffered_channel<std::string> ch0, ch1;
        ch0.construct();
        ch1.construct();

        thunk t0 = [ch0,ch1,recv_total]
        {
            std::string send_message(100000,'3');
            std::string recv_message;
            std::string message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.send(send_message);
                ch1.recv(recv_message);
            }
        };
        
        thunk t1 = [ch0,ch1,recv_total]
        {
            std::string send_message(100000,'3');
            std::string recv_message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.recv(recv_message);
                ch1.send(send_message);
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

TEST_F(comparison_thread_large_lvalue_communication, 1x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_large_lvalue_communication_op, 1, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_thread_large_lvalue_communication, 2x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_large_lvalue_communication_op, 2, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_thread_large_lvalue_communication, 4x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_large_lvalue_communication_op, 4, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_thread_large_lvalue_communication, 8x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_large_lvalue_communication_op, 8, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}


void concurrent_large_lvalue_communication_op(std::uint64_t thread_total, 
                                std::uint64_t recv_total)
{
    mce::buffered_channel<int> done_ch;

    std::uint64_t repeat = thread_total/2;
    done_ch.construct(repeat);
    
    std::cout << "concurrent_large_lvalue_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        mce::unbuffered_channel<std::string> ch0, ch1;
        ch0.construct();
        ch1.construct();
        
        mce::thunk t1 = [ch0,ch1,recv_total,done_ch]
        {
            std::string send_message(100000,'3');
            std::string recv_message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.recv(recv_message);
                ch1.send(send_message);
            }
            done_ch.send(0);
        };

        mce::thunk t0 = [ch0,ch1,recv_total,t1]
        {
            mce::concurrent(t1);

            std::string send_message(100000,'3');
            std::string recv_message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.send(send_message);
                ch1.recv(recv_message);
            }
        };

        mce::parallel(t0);
    }

    int x; // temp val
    for(std::uint64_t c=0; c<repeat; ++c) { done_ch.recv(x); }
}

TEST_F(comparison_concurrent_large_lvalue_communication, 1x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_large_lvalue_communication_op, 1, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_concurrent_large_lvalue_communication, 2x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_large_lvalue_communication_op, 2, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_concurrent_large_lvalue_communication, 4x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_large_lvalue_communication_op, 4, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_concurrent_large_lvalue_communication, 8x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_large_lvalue_communication_op, 8, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

void thread_large_rvalue_communication_op(std::uint64_t thread_total, 
                                    std::uint64_t recv_total)
{
    std::queue<std::thread> q;

    std::uint64_t repeat = thread_total/2;
    std::cout << "thread_large_rvalue_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        unbuffered_channel<std::string> ch0, ch1;
        ch0.construct();
        ch1.construct();

        thunk t0 = [ch0,ch1,recv_total]
        {
            std::string message(100000,'3');
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.send(std::move(message));
                ch1.recv(message);
            }
        };
        
        thunk t1 = [ch0,ch1,recv_total]
        {
            std::string message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.recv(message);
                ch1.send(std::move(message));
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

TEST_F(comparison_thread_large_rvalue_communication, 1x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_large_rvalue_communication_op, 1, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_thread_large_rvalue_communication, 2x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_large_rvalue_communication_op, 2, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_thread_large_rvalue_communication, 4x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_large_rvalue_communication_op, 4, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_thread_large_rvalue_communication, 8x_core_count_threads_communicating_in_pairs_N_msgs_sent_per_thread_over_channel)
{
    launch_core_multiplier_op(thread_large_rvalue_communication_op, 8, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}


void concurrent_large_rvalue_communication_op(std::uint64_t thread_total, 
                                std::uint64_t recv_total)
{
    mce::buffered_channel<int> done_ch;
    done_ch.construct(std::thread::hardware_concurrency());

    std::uint64_t repeat = thread_total/2;
    std::cout << "concurrent_large_rvalue_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        mce::unbuffered_channel<std::string> ch0, ch1;
        ch0.construct();
        ch1.construct();
        
        thunk t1 = [ch0,ch1,recv_total,done_ch]
        {
            std::string message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.recv(message);
                ch1.send(std::move(message));
            }
            done_ch.send(0);
        };

        thunk t0 = [ch0,ch1,recv_total,t1]
        {
            mce::concurrent(t1);

            std::string message(100000,'3');
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.send(std::move(message));
                ch1.recv(message);
            }
        };

        mce::parallel(t0);
    }

    int x; // temp val
    for(std::uint64_t c=0; c<repeat; ++c) { done_ch.recv(x); }
}

TEST_F(comparison_concurrent_large_rvalue_communication, 1x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_large_rvalue_communication_op, 1, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_concurrent_large_rvalue_communication, 2x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_large_rvalue_communication_op, 2, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_concurrent_large_rvalue_communication, 4x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_large_rvalue_communication_op, 4, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

TEST_F(comparison_concurrent_large_rvalue_communication, 8x_core_count_coroutines_communicating_in_pairs_N_msgs_sent_per_coroutine_over_channel)
{
    launch_core_multiplier_op(concurrent_large_rvalue_communication_op, 8, MCE_UT_LARGE_VALUE_MESSAGE_COUNT);
}

void thread_congested_communication_op(std::uint64_t thread_total,
                                       std::uint64_t recv_total)
{
    std::cout << "thread_congested_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;
    std::queue<std::thread> q;

    std::uint64_t repeat = thread_total/2;

    buffered_channel<int> ch0, ch1;
    ch0.construct();
    ch1.construct();

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        thunk t0 = [ch0,ch1,recv_total]
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.send(recv);
                ch1.recv(message);
            }
        };
        
        thunk t1 = [ch0,ch1,recv_total]
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.recv(message);
                ch1.send(recv);
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

TEST_F(comparison_thread_congested_communication, 1x_core_count_threads_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_thread)
{
    launch_core_multiplier_op(thread_congested_communication_op, 1, MCE_UT_CONGESTED_MESSAGE_COUNT);
}

TEST_F(comparison_thread_congested_communication, 2x_core_count_threads_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_thread)
{
    launch_core_multiplier_op(thread_congested_communication_op, 2, MCE_UT_CONGESTED_MESSAGE_COUNT);
}

TEST_F(comparison_thread_congested_communication, 3x_core_count_threads_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_thread)
{
    launch_core_multiplier_op(thread_congested_communication_op, 3, MCE_UT_CONGESTED_MESSAGE_COUNT);
}

void concurrent_congested_communication_op(std::uint64_t thread_total, std::uint64_t recv_total)
{
    std::cout << "concurrent_congested_communication_op(thread_total: " << thread_total << ", recv_total: " << recv_total << ")" << std::endl;
    mce::buffered_channel<int> done_ch;
    done_ch.construct();

    std::uint64_t repeat = thread_total/2;

    mce::unbuffered_channel<int> ch0, ch1;
    ch0.construct();
    ch1.construct();

    for(std::uint64_t c=0; c<repeat; ++c)
    {
        thunk t1 = [ch0,ch1,recv_total,done_ch]
        {
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.recv(message);
                ch1.send(recv);
            }
            done_ch.send(0);
        };

        thunk t0 = [ch0,ch1,recv_total,t1]
        {
            mce::concurrent(t1);
            int message;
            for(size_t recv=0; recv<recv_total; ++recv)
            {
                ch0.send(recv);
                ch1.recv(message);
            }
        };

        mce::parallel(t0);
    }

    int x; // temp val
    std::uint64_t c=0;
    for(; c<repeat; ++c) { done_ch.recv(x); }
}


TEST_F(comparison_concurrent_congested_communication, 1x_core_count_coroutines_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_coroutine)
{
    launch_core_multiplier_op(concurrent_congested_communication_op, 1, MCE_UT_CONGESTED_MESSAGE_COUNT);
}

TEST_F(comparison_concurrent_congested_communication, 2x_core_count_coroutines_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_coroutine)
{
    launch_core_multiplier_op(concurrent_congested_communication_op, 2, MCE_UT_CONGESTED_MESSAGE_COUNT);
}

TEST_F(comparison_concurrent_congested_communication, 3x_core_count_coroutines_communicating_in_pairs_compete_for_channel_N_msgs_sent_per_coroutine)
{
    launch_core_multiplier_op(concurrent_congested_communication_op, 3, MCE_UT_CONGESTED_MESSAGE_COUNT);
}
