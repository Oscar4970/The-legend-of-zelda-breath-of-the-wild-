#include "homebrew_test_runtime.h"
#include <iostream>
int main(){
  const auto r=homebrew_test::run();
  std::cout<<"AArch64 compiled C -> upstream-generated C++: "
           <<r.got_first<<"/"<<r.expected_first<<", "
           <<r.got_second<<"/"<<r.expected_second
           <<", guest_bss="<<r.final_guest_counter
           <<", svc_test="<<r.service_calls
           <<", horizon_connects="<<r.horizon_connects
           <<", horizon_closes="<<r.horizon_closes
           <<", rejected="<<r.rejected_calls
           <<", open_handles="<<r.handles_alive<<"\n";
  if(!r.ok){
    std::cerr<<"FAIL: "<<r.diagnostic<<"\n";
    return 1;
  }
  std::cout<<"PASS: compiled ARM64 C, stack, globals, and Horizon SVC 0x1f/0x16 ABI verified\n";
  return 0;
}
