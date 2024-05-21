//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_008
#include <iostream>
#include <string>
#include "mce.hpp"

using namespace mce;

int main()
{
    mce::chan<std::string> ch = mce::chan<std::string>::make();
    mce::chan<bool> done_ch = mce::chan<bool>::make();
    
    auto sender = [=]
    {
        for(size_t cnt = 0; cnt<10; ++cnt)
        {
            ch.send(std::string("hello") + std::to_string(cnt));
        }
        ch.close();
    };
    
    auto receiver = [=]
    {
        for(auto& s : ch)
        {
            std::cout << s << std::endl;
        }
        done_ch.send(true);
    };
    
    mce::parallel(sender);
    mce::parallel(receiver);
    bool r;
    done_ch.recv(r);
    return 0;
}
