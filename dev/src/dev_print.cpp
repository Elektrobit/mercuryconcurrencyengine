//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
#include "dev_print.hpp"

ccc::unique_spinlock ccc::dev::detail::dev_print_locals::acquire_lock() 
{
    static ccc::detail::spinlock lk;
    return ccc::unique_spinlock(lk);
}
