//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// human_only_example_001
#include <iostream>
#include <string>
#include "mce.hpp"

int main(int argc, char** argv)
{
    mce::chan<int> test_ch = mce::chan<int>::make();

    std::string inp;

    // CPU usage should be 0 because we are using OS level blocking when no 
    // tasks are available
    std::cout << "wait for user input before accessing default threadpool: ";
    std::cin >> inp;

    // spawn task that will block forever
    mce::parallel([&]{ int x; test_ch.recv(x);});

    std::cout << "wait for user input before launching await operation: ";
    std::cin >> inp;

    mce::await([]{ std::cout << "awaited" << std::endl; });

    std::cout << "wait for user input before launching timer operation: ";
    std::cin >> inp;

    mce::timer(mce::time_unit::millisecond, 0, []{ std::cout << "timeout" << std::endl; });
    mce::sleep(mce::time_unit::millisecond, 0);

    std::cout << "default_threadpool worker count: " << mce::default_threadpool().size() << std::endl;
    std::cout << "await threadpool worker count: " << mce::await_count() << std::endl;

    std::cout << "wait for user input before exitting: ";
    std::cin >> inp;

    return 0;
}
