//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_014 
#include <iostream>
#include "mce.hpp"

void print_threadpool_address()
{
    // just for synchronizing prints
    static mce::mutex mtx;
    std::unique_lock<mce::mutex> lk(mtx);
    std::cout << "threadpool address[" 
              << &(mce::this_threadpool())
              << "]"
              << std::endl;
};

void some_function(mce::chan<int> done_ch)
{
    print_threadpool_address();
    done_ch.send(0);
}

int main()
{
    mce::chan<int> done_ch = mce::chan<int>::make();
    auto tp = mce::threadpool::make();

    std::cout << "tp address[" << tp.get() << "]" << std::endl;
    std::cout << "default threadpool address[" 
              << &(mce::default_threadpool())
              << "]" 
              << std::endl;
    
    // executes on the current threadpool
    tp->worker().schedule(some_function, done_ch);
    tp->worker().schedule([=]
    {   
        // executes on the current threadpool, not the default
        mce::parallel(some_function, done_ch);
    });
    
    // execute on the default threadpool, not our custom threadpool
    mce::parallel(some_function, done_ch);

    int r;
    done_ch.recv(r);
    done_ch.recv(r);
    done_ch.recv(r);
    tp->halt();
    return 0;
}
