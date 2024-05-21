//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "coroutine.hpp"

mce::coroutine*& mce::detail::tl_this_coroutine()
{
    thread_local mce::coroutine* tltc = nullptr;
    return tltc;
}
