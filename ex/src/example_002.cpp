//SPDX-License-Identifier: Apache-2.0
//Author: Blayne Dennis 
// example_002
#include <iostream>
#include "mce.hpp"

int main()
{
  const int max = 10;
  auto ch = mce::chan<int>::make();
  auto done_ch = mce::chan<bool>::make();

  auto sender = [](int max, mce::chan<int> ch)
  { 
      for(int i=0; i<max; ++i)
      { 
          ch.send(i); 
      }
      ch.close();
  };

  auto receiver = [](int max, mce::chan<int> ch, mce::chan<bool> done_ch)
  { 
      for(auto& i : ch)
      {
          std::cout << i << std::endl;
      }
      done_ch.send(true); 
  };

  mce::parallel(sender, max, ch);
  mce::parallel(receiver, max, ch, done_ch);
  bool r;
  done_ch.recv(r);
  return 0;
}
