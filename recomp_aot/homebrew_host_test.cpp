#include "homebrew_test_runtime.h"
#include <iostream>
int main(){
  const auto r=homebrew_test::run();
  std::cout<<"AArch64 compiled C -> upstream-generated C++: "
           <<r.got_first<<"/"<<r.expected_first<<", "
           <<r.got_second<<"/"<<r.expected_second
           <<", guest_bss="<<r.final_guest_counter
           <<", svc_test="<<r.service_calls<<"\n";
  if(!r.ok){
    std::cerr<<"FAIL: "<<r.diagnostic<<"\n";
    return 1;
  }
  std::cout<<"PASS: compiled C functions, stack, data, BSS and test SVC execute on host\n";
  return 0;
}
