//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#pragma once
#include <iostream>

using namespace std;

class Verbose {
public:
    Verbose(int v = 0) : val(v) {
        cout << "constructor " << val << endl;
    }

    Verbose(const Verbose& other) : val(other.val) {
        cout << "copy constructor " << val << endl;
    }

    Verbose(Verbose&& other) : val(std::move(other.val)) {
        cout << "move constructor " << val << endl;
    }

    Verbose& operator=(const Verbose& other) {
        val = other.val;
        cout << "copy assign operator " << val << endl;
        return *this;
    }

    Verbose& operator=(Verbose&& other) {
        val = std::move(other.val);
        cout << "move assign operator " << val << endl;
        return *this;
    }

    bool operator==(const Verbose& other) const {
        cout << "operator== " << val << endl;
        return other.val == val;
    }

    bool operator!=(const Verbose& other) const {
        cout << "operator!= " << val << endl;
        return other.val != val;
    }

    int val;
};
