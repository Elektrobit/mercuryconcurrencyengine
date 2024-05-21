//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "timer.hpp"
#include "function_utility.hpp"
#include "scheduler.hpp"
#include "await.hpp"
#include "chan.hpp"
#include "buffered_channel.hpp"

#include <string>

#include <gtest/gtest.h>

TEST(await, return_value_int)
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
    mce::chan<int> ch = mce::chan<int>::make();

    cs->schedule([=]
    { 
        auto f = [](int i){ return i*2; };
        ch.send(mce::await(f, 8)); 
    });
    std::thread thd([=]{ cs->run(); });

    int r;
    ch.recv(r);
    EXPECT_EQ(r,16);
    cs->halt();
    thd.join();
}

TEST(await, return_value_void)
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
    mce::chan<int> ch = mce::chan<int>::make();

    cs->schedule([=]
    { 
        auto f = [](int i){ };
        ch.send(mce::await(f, 8)); 
    });
    std::thread thd([=]{ cs->run(); });

    int r;
    ch.recv(r);
    EXPECT_EQ(r,0);
    cs->halt();
    thd.join();
}

TEST(await, return_value_string)
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
    mce::chan<std::string> ch = mce::chan<std::string>::make();

    cs->schedule([=]
    { 
        auto f = [](std::string s){ return s+" world"; };
        ch.send(mce::await(f, std::string("hello"))); 
    });
    std::thread thd([=]{ cs->run(); });

    std::string r;
    ch.recv(r);
    EXPECT_EQ(r,std::string("hello world"));
    cs->halt();
    thd.join();
}

TEST(await, await_reference)
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();
    int i = 0;

    cs->schedule([](int& i)
    { 
        auto f = [](int& i, int val){ i = val; };
        mce::await(f, std::ref(i), 2); 
        mce::this_scheduler().halt();
    }, std::ref(i));
    cs->run();

    EXPECT_EQ(i,2);
}

TEST(await, ensure_same_thread_reused)
{
    auto get_id = [=]{ return std::this_thread::get_id(); };
    auto id = mce::await(get_id);
    EXPECT_NE(id, std::thread::id());
    EXPECT_EQ(id, std::this_thread::get_id());
    EXPECT_EQ(id, mce::await(get_id));
    EXPECT_EQ(id, mce::await(get_id));

    auto cs = mce::scheduler::make();
    std::thread thd([=]{ cs->run(); });

    auto id_ch = mce::chan<std::thread::id>::make();
    cs->schedule([=]{ id_ch.send(mce::await(get_id)); });
    id_ch.recv(id);
    EXPECT_NE(id, std::thread::id());
    EXPECT_NE(id, std::this_thread::get_id());
    
    cs->schedule([=]{ id_ch.send(mce::await(get_id)); });
    // expect id to be reused in coroutine 
    std::thread::id r;
    id_ch.recv(r);
    EXPECT_EQ(id, r);

    cs->halt();
    thd.join();
}

TEST(await, is_io)
{
    auto cs = mce::scheduler::make();
    std::thread thd([=]{ cs->run(); });

    size_t is_not_await_count = 0;
    size_t is_await_count = 0;

    auto update_count = [&]
    {
        if(mce::is_await()) 
        {
            ++is_await_count;
        }
        else
        {
            ++is_not_await_count;
        }
    };

    cs->schedule(update_count);
    cs->schedule(update_count);
    cs->schedule(update_count);
    cs->schedule([&]{ mce::await(update_count); });
    cs->schedule([&]{ mce::await(update_count); });
   
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    cs->halt();
    thd.join();

    EXPECT_EQ(3, is_not_await_count);
    EXPECT_EQ(2, is_await_count);
}

TEST(await, correct_this_scheduler)
{
    auto cs = mce::scheduler::make();
    std::thread thd([=]{ cs->run(); });
    auto done_ch = mce::unbuffered_channel<bool>::make();

    size_t expected_success_count = 11;
    size_t success_count = 0;

    size_t expected_deepest_recursion = 1;
    size_t deepest_recursion = 0;

    std::function<bool(size_t)> scheduler_check;
    auto scheduler_check_lambda = [&](size_t recursive_depth) -> bool
    {
        deepest_recursion = recursive_depth;
        bool success = true;

        auto update_success = [&](bool new_val)
        {
            success = success && new_val;
            if(success) { ++success_count; }
        };

        update_success((std::shared_ptr<mce::scheduler>)(*(mce::detail::tl_this_scheduler_redirect())) == cs);
        update_success(mce::in_scheduler());
        update_success((std::shared_ptr<mce::scheduler>)mce::this_scheduler() == cs);

        if(mce::is_await())
        {
            update_success(recursive_depth == 1);
            update_success(
                mce::detail::tl_this_scheduler() !=
                mce::detail::tl_this_scheduler_redirect());
        }
        else 
        {
            update_success(recursive_depth == 0);
            update_success(
                mce::detail::tl_this_scheduler() ==
                mce::detail::tl_this_scheduler_redirect());

            update_success(mce::await(scheduler_check, recursive_depth+1));
            done_ch.send(0);
        }

        return success;
    };

    scheduler_check = scheduler_check_lambda;

    cs->schedule(scheduler_check, 0);

    bool ret;
    done_ch.recv(ret);

    cs->halt();
    thd.join();

    EXPECT_EQ(expected_success_count, success_count);
    EXPECT_EQ(expected_deepest_recursion, deepest_recursion);
}

TEST(await, correct_this_threadpool)
{
    auto base_tp = std::shared_ptr<mce::threadpool>();
    auto tp = mce::threadpool::make(1);
    auto cs = (std::shared_ptr<mce::scheduler>)tp->worker();

    auto done_ch = mce::unbuffered_channel<bool>::make();

    size_t expected_success_count = 15;
    size_t success_count = 0;

    size_t expected_deepest_recursion = 1;
    size_t deepest_recursion = 0;

    std::function<bool(size_t)> scheduler_check;
    auto scheduler_check_lambda = [&](size_t recursive_depth) -> bool
    {
        deepest_recursion = recursive_depth;
        bool success = true;

        auto update_success = [&](bool new_val)
        {
            success = success && new_val;
            if(success) { ++success_count; }
        };

        std::shared_ptr<mce::scheduler> this_sch = mce::this_scheduler();
        std::shared_ptr<mce::threadpool> this_tp = mce::this_threadpool();

        update_success(
            (std::shared_ptr<mce::scheduler>)
            *(mce::detail::tl_this_scheduler_redirect()) == cs);
        update_success(mce::in_scheduler());
        update_success(this_tp == tp);
        update_success(this_tp != base_tp);
        update_success(this_sch == cs);

        if(mce::is_await())
        {
            update_success(recursive_depth == 1);
            update_success(
                mce::detail::tl_this_scheduler() !=
                mce::detail::tl_this_scheduler_redirect());
        }
        else 
        {
            update_success(recursive_depth == 0);
            update_success(
                mce::detail::tl_this_scheduler() ==
                mce::detail::tl_this_scheduler_redirect());

            update_success(mce::await(scheduler_check, recursive_depth+1));
            done_ch.send(0);
        }

        return success;
    };

    scheduler_check = scheduler_check_lambda;

    tp->worker().schedule(scheduler_check, 0);

    bool ret;
    done_ch.recv(ret);

    // force scheduler to halt and worker to join
    tp.reset();

    EXPECT_EQ(expected_success_count, success_count);
    EXPECT_EQ(expected_deepest_recursion, deepest_recursion);
}

TEST(await, correct_recursive_io)
{
    auto cs = mce::scheduler::make();
    std::thread thd([=]{ cs->run(); });
    auto done_ch = mce::unbuffered_channel<bool>::make();

    std::thread::id tid;
    size_t expected_success_count = 100;
    size_t success_count = 0;

    auto check_tid = [&]
    { 
        // thread id shouldn't change on recurssive calls
        bool success = tid == std::this_thread::get_id();
        if(success) { ++success_count; }
    };

    std::function<void(size_t)> recursive_op;

    auto recursive_op_lambda = [&](size_t remaining)
    {
        if(remaining)
        {
            check_tid();
            mce::await(recursive_op, --remaining);
        }
        else 
        {
            done_ch.send(0);
        }
    };

    recursive_op = recursive_op_lambda;

    auto recursive_init = [&]
    {
        tid = std::this_thread::get_id();
        mce::await(recursive_op, expected_success_count);
    };

    auto launch_ops = [&]{ mce::await(recursive_init); };

    cs->schedule(launch_ops);

    bool ret;
    done_ch.recv(ret);

    cs->halt();
    thd.join();

    EXPECT_EQ(expected_success_count, success_count);
}

TEST(await, ensure_unlimited_workers)
{
    auto cs = mce::scheduler::make();
    std::thread thd([=]{ cs->run(); });

    auto get_id = [=]{ return std::this_thread::get_id(); };
    auto blocker = [=](mce::buffered_channel<int> run_ch, 
                       mce::buffered_channel<int> wait_ch,
                       mce::buffered_channel<int> done_ch)
    {
        run_ch.send(0);
        int r;
        wait_ch.recv(r);
        done_ch.send(1);
    };
    
    std::thread::id id;
    EXPECT_EQ(id, std::thread::id());

    id = mce::await(get_id);
    EXPECT_NE(id, std::thread::id());
    EXPECT_EQ(id, mce::await(get_id));
    EXPECT_EQ(id, std::this_thread::get_id());

    auto id_ch = mce::chan<std::thread::id>::make();
    cs->schedule([=]{ id_ch.send(mce::await(get_id)); });
    id_ch.recv(id);
    EXPECT_NE(id, std::thread::id());
    EXPECT_NE(id, mce::await(get_id));
    EXPECT_NE(id, std::this_thread::get_id());

    std::vector<mce::buffered_channel<int>> confirm_running_chs;
    std::vector<mce::buffered_channel<int>> wait_chs;
    std::vector<mce::buffered_channel<int>> confirm_done_chs;

    for(size_t i=0; i<100; ++i)
    {
        auto run_ch = mce::buffered_channel<int>::make(1);
        auto wait_ch = mce::buffered_channel<int>::make(1);
        auto done_ch = mce::buffered_channel<int>::make(1);
        confirm_running_chs.push_back(run_ch);
        wait_chs.push_back(wait_ch);
        confirm_done_chs.push_back(done_ch);

        cs->schedule([=]
        { 
            mce::await(blocker, run_ch, wait_ch, done_ch); 
        });
    }

    EXPECT_EQ(confirm_running_chs.size(),100);
    EXPECT_EQ(confirm_done_chs.size(),100);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for(auto& ch : confirm_done_chs)
    {
        EXPECT_EQ(ch.size(), 0);
    }

    for(auto& ch : confirm_running_chs) 
    {
        EXPECT_EQ(ch.size(), 1);
        int r;
        ch.recv(r);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // expect 101st concurrent await() call to not have the same thread id 
    cs->schedule([=]{ id_ch.send(mce::await(get_id)); });
    std::thread::id id2;
    id_ch.recv(id2);
    EXPECT_NE(id, id2);

    for(auto& ch : confirm_done_chs)
    {
        EXPECT_EQ(ch.size(), 0);
    }

    for(auto& ch : wait_chs)
    {
        ch.send(0);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for(auto& ch : confirm_done_chs)
    {
        EXPECT_EQ(ch.size(), 1);
        int r;
        ch.recv(r);
    }

    cs->halt();
    thd.join();
}
