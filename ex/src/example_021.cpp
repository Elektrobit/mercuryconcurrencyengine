//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_021
#include <iostream>
#include "mce.hpp"
int main()
{
    mce::unbuffered_channel<int> ch1;
    mce::unbuffered_channel<int> ch2;
    ch1.construct();
    ch2.construct();
    
    std::thread thd([](mce::unbuffered_channel<int> ch1, 
                   mce::unbuffered_channel<int> ch2)
    {
        int x;
        ch1.recv(x);
        std::cout << x << std::endl;
        ch2.send(0);
    }, ch1, ch2);
    
    ch1.send(14);
    int r;
    ch2.recv(r);
    thd.join();
    return 0;
}
