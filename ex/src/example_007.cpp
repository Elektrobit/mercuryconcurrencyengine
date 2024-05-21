//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_007
#include <iostream>
#include <string>
#include "mce.hpp"

int main()
{
    // is a buffered channel
    mce::chan<std::string> buf_ch = mce::chan<std::string>::make(mce::buffered_channel<std::string>::make(3));
    
    // no channel provided, defaults to unbuffered_channel
    mce::chan<std::string> unbuf_ch = mce::chan<std::string>::make(); 
    
    mce::chan<int> done_ch = mce::chan<int>::make();
    
    auto my_function = [](mce::chan<std::string> ch1, 
                          mce::chan<std::string> ch2, 
			  mce::chan<int> done_ch)
    {
        std::string r1;
        std::string r2;
        ch1.recv(r1);
        ch2.recv(r2);
        std::cout << "ch1: " << r1 << "; ch2: " << r2 << std::endl;
        done_ch.send(0);
    };
    
    mce::parallel(my_function, buf_ch, unbuf_ch, done_ch);
    
    buf_ch.send("hello");
    unbuf_ch.send("world");
    int r;
    done_ch.recv(r);
    
    return 0;
}
