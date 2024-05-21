//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_004
#include <iostream>
#include "mce.hpp"

int main()
{
    int i=0;
    auto done_ch = mce::chan<bool>::make();
    auto modify_reference = [=,&i](int val){ i = val; done_ch.send(true); };
    mce::parallel(modify_reference, 2);
    bool r;
    done_ch.recv(r);
    std::cout << "i: " << i << std::endl;
    return 0;
}
