//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_020  
#include <iostream>
#include "mce.hpp"

int main()
{
    mce::chan<int> ch = mce::chan<int>::make();
    mce::chan<int> done_ch = mce::chan<int>::make();
    
    auto f = [](mce::chan<int> done_ch, mce::chan<int> ch)
    {
        // schedule coroutine by indirectly using the threadpool
        mce::this_threadpool().worker().schedule([ch]{ ch.send(0); });
        
        int x;
        ch.recv(x);
        
        mce::this_threadpool().worker().schedule([=]{ done_ch.send(0); });
    };
    
    // start a threadpool with 2 worker threads
    std::shared_ptr<mce::threadpool> tp = mce::threadpool::make(2);
    
    // schedule coroutine by directly using the threadpool
    tp->worker().schedule(f, done_ch, ch);
   
    int r;
    done_ch.recv(r);
    tp->halt();
    std::cout << "received done confirmation" << std::endl;
    return 0;
}
