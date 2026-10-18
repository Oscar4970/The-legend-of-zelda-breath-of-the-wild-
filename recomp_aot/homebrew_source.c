/* Own freestanding test program. No Nintendo code or game data. */
#include <stdint.h>
volatile uint32_t guest_scale = 3;
volatile uint32_t guest_bias = 11;
volatile uint32_t guest_counter;
/* The real Horizon named-port ABI uses X1 for the port name. */
__attribute__((used)) const char guest_sm_port[] = "sm:";
__attribute__((noinline,used)) uint32_t mix(uint32_t value, uint32_t round) {
    return (value ^ (round * 3u)) + (round << 2u);
}
__attribute__((noinline,used)) uint32_t demo_entry(uint32_t seed) {
    volatile uint32_t scratch[4];
    for (uint32_t i=0;i<4;i++) scratch[i]=seed + 3u*i;
    uint32_t result=0;
    for (uint32_t i=0;i<4;i++) result += mix(scratch[i], i+1);
    guest_counter++;
    register uint64_t input __asm__("x0") = (uint64_t)(result ^ seed);
    __asm__ volatile ("svc #0x7e" : "+r"(input) :: "memory");
    uint32_t result=(uint32_t)input + guest_scale + guest_bias + guest_counter;
    /*
     * These are REAL Horizon ARM64 SVC instruction numbers/ABIs:
     *   0x1f ConnectToNamedPort: X1 -> port name, W0 -> Result, W1 -> Handle.
     *   0x16 CloseHandle:       W0 -> Handle, W0 -> Result.
     * The Android test implements only a private mock service manager,
     * not the actual Switch kernel/SM IPC protocol.
     */
    register uint64_t connect_result __asm__("x0") = 0;
    register uint64_t port_or_handle __asm__("x1") = (uint64_t)guest_sm_port;
    __asm__ volatile("svc #0x1f"
                     : "+r"(connect_result), "+r"(port_or_handle)
                     : : "memory");
    if ((uint32_t)connect_result != 0) return 0xffffffffu;
    register uint64_t close_result __asm__("x0") = (uint32_t)port_or_handle;
    __asm__ volatile("svc #0x16" : "+r"(close_result) : : "memory");
    if ((uint32_t)close_result != 0) return 0xfffffffeu;
    return result;
}
