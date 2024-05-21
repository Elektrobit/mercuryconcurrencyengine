//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_005
#include <iostream>
#include <string>
#include "mce.hpp"

void my_function(int arg0, std::string arg1, mce::chan<int> done_ch)
{
    std::cout << "arg0: "
              << arg0
              << "; arg1: "
              << arg1 
              << std::endl;

    done_ch.send(0);
}

int main(int argc, char** argv)
{
    mce::chan<int> done_ch = mce::chan<int>::make();

    // execute functions as a parallel, concurrent coroutine using the parallel() call
    mce::parallel(my_function, 3, "hello world", done_ch);

    // wait for a coroutine to send a value over done_ch before returning
    int r;
    done_ch.recv(r);
    return 0;
}
