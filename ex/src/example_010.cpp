//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_010
#include <iostream>
#include "mce.hpp"

const int the_number = 3;

int main()
{
    // this channel behaves like an std::future
    mce::chan<int> ret_ch = mce::chan<int>::make(); 
    
    mce::parallel([ret_ch]{ ret_ch.send(the_number); });
   
    int r;
    ret_ch.recv(r);
    std::cout << "The number: " << r << std::endl;
    return 0;
}
