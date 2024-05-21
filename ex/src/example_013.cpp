//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_013
#include <functional>
#include <iostream>

int main(int argc, char** arv)
{
    int c=0;
    std::function<void()> my_thunk = [&]{ ++c; };

    std::cout << "c: " << c << std::endl; // prints 0
    my_thunk();
    std::cout << "c: " << c << std::endl; // prints 1
    return 0;
}
