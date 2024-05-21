//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "fptr.hpp"

int fptr::x_ = 0;
int fptr_void::x_ = 0;
mce::chan<int> fptr::ch_ = mce::chan<int>::make();
mce::chan<int> fptr_void::ch_ = mce::chan<int>::make();

void reset_fptr_vals()
{
    fptr::x_ = 0;
    fptr_void::x_ = 0;
    fptr::ch_ = mce::chan<int>::make();
    fptr_void::ch_ = mce::chan<int>::make();
}
