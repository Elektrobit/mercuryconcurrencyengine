//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_012
#include <thread>
#include <iostream>
#include "mce.hpp"

void my_function(mce::buffered_channel<int> done_ch)
{
    mce::thunk t = []
    {
        std::cout << "executing on thread " 
                  << std::this_thread::get_id() 
                  << std::endl;
    };

    t();

    // launch a new thread, block the current coroutine or thread, execute t 
    // on the new thread, then unblock the calling thread with the result
    mce::await(t);

    t();

    done_ch.send(0);
}

int main(int argc, char** argv)
{
    mce::buffered_channel<int> done_ch;
    done_ch.construct();

    // execute functions as coroutines with the concurrent() call
    mce::parallel(my_function, done_ch);

    // wait for a coroutine to send a value over done_ch before returning
    int x; 
    done_ch.recv(x);
    return 0;
}
