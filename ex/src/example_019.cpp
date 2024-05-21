//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_019
#include <iostream>
#include "mce.hpp"

int main()
{
    std::shared_ptr<mce::scheduler> cs = mce::scheduler::make();                                   
    mce::chan<int> done_ch = mce::chan<int>::make(); 
    
    auto recv_function = [](mce::chan<int> done_ch)
    {                                                                              
        int r;
        done_ch.recv(r);
        std::cout << "recv done" << std::endl;
        mce::this_scheduler().halt();                                              
    };                

    auto send_function = [&](mce::chan<int> done_ch)
    { 
        mce::this_scheduler().schedule(recv_function, done_ch);
        std::cout << "send done" << std::endl;
        done_ch.send(0);
    };

    cs->schedule(send_function, done_ch); // schedule send_function for exection
    cs->run(); // execute coroutines on the current thread until halt() is called
    return 0;
}
