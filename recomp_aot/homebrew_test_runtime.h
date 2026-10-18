#pragma once
#include <cstdint>
#include <string>
namespace homebrew_test {
struct Result {
  bool ok=false;
  uint32_t got_first=0, expected_first=0, got_second=0, expected_second=0;
  uint32_t final_guest_counter=0, service_calls=0;
  uint32_t horizon_connects=0, horizon_closes=0, rejected_calls=0, handles_alive=0;
  std::string diagnostic;
};
Result run();
}
