//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_028
#include <iostream>
#include "mce.hpp"

// only an object pointer will be constructed, but no data will ever be copied
struct void_t { };

void receiver(mce::chan<void_t> ch) {
    void_t r;
    ch.recv(r);
    std::cout << "receiver received" << std::endl;
    ch.send({});
}

int main() {
    auto ch = mce::chan<void_t>::make();
    mce::concurrent(receiver, ch);

    ch.send({});
    void_t r;
    ch.recv(r);
    std::cout << "main received" << std::endl;

    return 0;
}
