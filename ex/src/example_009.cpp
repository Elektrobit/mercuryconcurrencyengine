//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_009
#include <string>
#include <iostream>
#include "mce.hpp"

void concurrent_foo(mce::chan<std::string> ch) {
    ch.send("hello");
}

int main() {
    mce::chan<std::string> ch = mce::chan<std::string>::make();
    mce::concurrent(concurrent_foo,ch);

    // interact with concurrent_foo by sending messages over channel object "ch"
    std::string s;
    ch.recv(s);

    std::cout << "received: " << s << std::endl;
    return 0;
}
