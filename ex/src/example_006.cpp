//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_006
#include <iostream>
#include "mce.hpp"

void func(mce::buffered_channel<std::string> out_ch)
{
    out_ch.send("hello");
}

void func2(mce::buffered_channel<std::string> in_ch, mce::unbuffered_channel<int> done_ch)
{
    std::string s;
    in_ch.send(s);
    std::cout << "func said " << s << std::endl;
    done_ch.send(0);
}

int main(int argc, char** argv)
{
    // has an internal container that can hold 3 simultaneous values
    auto ch = mce::buffered_channel<std::string>::make(3);
    
    // has no internal container for values
    auto done_ch = mce::unbuffered_channel<int>::make(); 

    mce::parallel(func, ch);
    mce::parallel(func2, ch, done_ch);

    int r;
    done_ch.recv(r);
    return 0;
}
