//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_018
#include <iostream>
#include <mutex> // for std::unique_lock 
#include "mce.hpp"

int main()
{
    mce::mutex mtx;
    mce::condition_variable cv;
    bool flag = false;
    int i = 0;
    
    std::unique_lock<mce::mutex> lk(mtx);
    
    std::thread t([&]
    {
        {
            std::unique_lock<mce::mutex> lk(mtx);
            flag = true;
            i = 1;
        }
        cv.notify_one();
    });
    
    std::cout << "i: " << i << std::endl;
    
    while(!flag){ cv.wait(lk); };
    
    std::cout << "i: " << i << std::endl;
    
    t.join();
    return 0;
}
