/* Own freestanding test program. No Nintendo code or game data. */
#include <stdint.h>
volatile uint32_t guest_scale = 3;
volatile uint32_t guest_bias = 11;
volatile uint32_t guest_counter;
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
    return (uint32_t)input + guest_scale + guest_bias + guest_counter;
}
