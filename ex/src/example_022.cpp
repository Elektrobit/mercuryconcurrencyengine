//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_022
#include <iostream>
#include "mce.hpp"
int main()
{
    mce::buffered_channel<int> ch1;
    mce::buffered_channel<int> ch2;
    ch1.construct(1); // can hold 1 value before forcing the sender to block
    ch1.construct(); // defaults to buffer size 1
    ch1.construct(0); // defaults to buffer size 1
    ch2.construct(30); // can hold 30 simultaneous values before forcing sender to block
    
    std::thread thd([](mce::buffered_channel<int> ch1, 
                       mce::buffered_channel<int> ch2)
    {
        int x;
        ch1.recv(x);
        std::cout << x << std::endl;
        ch2.send(0);
    }, ch1, ch2);
    
    ch1.send(42);
    int r;
    ch2.recv(r);
    thd.join();
    return 0;
}
