//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_003
#include <iostream>
#include "mce.hpp"

void modify_reference(int& i, int val, mce::chan<bool> done_ch)
{ 
    i = val; 
    done_ch.send(true);
}

int main()
{
    int i=0;
    auto done_ch = mce::chan<bool>::make();
    mce::parallel(modify_reference, std::ref(i), 2, done_ch);
    // ensure modify_reference is complete before reading i
    bool r;
    done_ch.recv(r);
    std::cout << "i: " << i << std::endl;
    return 0;
}
