//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "timer.hpp"

mce::timer_service::timer_id mce::timer_service::create_timer(timer_data&& td)
{
    mce::timer_service::timer_id id;

    {
        std::unique_lock<mce::spinlock> lk(mut_);

        id = get_id();
        td.id = id;
        timers_.push_back(std::move(td));
        timers_.sort();
        new_timers_ = true;

        if(waiting_for_timeouts_)
        {
            // wakeup timer_service
            waiting_for_timeouts_ = false;
            lk.unlock();
            cv_.notify_one();
        }
    }

    return id;
} 

mce::timer_service& mce::default_timer_service() 
{
    struct default_timer_service_worker 
    {
        mce::timer_service default_timer_service; 
        std::thread default_timer_thread;

        default_timer_service_worker() :
            default_timer_thread(
                std::thread([=]{ default_timer_service.start(); }))
        {
            default_timer_service.ready();
        }

        ~default_timer_service_worker()
        {
            default_timer_service.shutdown();
            default_timer_thread.join();
        }
    };

    static default_timer_service_worker g_default_timer_service_worker;

    return g_default_timer_service_worker.default_timer_service;
}
