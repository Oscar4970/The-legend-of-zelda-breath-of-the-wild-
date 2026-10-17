#include "funcs.h"
#include <cassert>
#include <iostream>
int main(){
  nx::CPU c{};
  c.x[0]=1;
  test_00000040(c); // invoke code genuinely generated from AArch64 NSO
  assert(c.x[0]==42);
  std::cout<<"PASS: upstream recompiled real AArch64 NSO code executes on host: w0=42\n";
  c.x[0]=99;
  test_00000040(c);
  assert(c.x[0]==42);
  std::cout<<"PASS: recompiled code independently executes again\n";
}
