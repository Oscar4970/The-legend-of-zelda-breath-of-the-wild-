#include "../app/src/main/cpp/linked_nso.h"
#include <cassert>
#include <iostream>
#include <cstring>

int main(){
  const auto file=linktest::makeNso();
  const auto header=nso::inspect(file.data(),nso::kHeaderSize,file.size());
  assert(header.valid && header.sections[0].compressed && header.sections[0].memSize==36);
  std::cout<<"PASS: synthetic linked NSO0 header\n";
  auto image=nsomemory::reconstruct(file.data(),file.size());
  assert(linktest::allowed(image));
  assert(image.hashChecks==3 && image.bytes.size()==0x2000c);
  std::cout<<"PASS: LZ4 and 3 hashes, text/rodata/data/BSS memory layout\n";
  assert(linktest::applyFixups(image.bytes.data(),image.bytes.size()));
  assert(linktest::read32(image.bytes.data()+8)==0x94000003u);
  assert(linktest::read32(image.bytes.data()+20)==0x1807ff62u);
  std::cout<<"PASS: AArch64 BL and PC-relative LDR fixups\n";
  assert(!linktest::applyFixups(image.bytes.data(),image.bytes.size()));
  std::cout<<"PASS: already patched instructions cannot be patched again\n";
  assert(!linktest::patchBl(image.bytes.data(),image.bytes.size(),8,1));
  assert(!linktest::patchLdrW(image.bytes.data(),image.bytes.size(),20,0x20000));
  std::cout<<"PASS: reject misaligned and out-of-range fixups\n";
  auto corrupt=file;
  corrupt[0x107]^=1;
  assert(!nsomemory::reconstruct(corrupt.data(),corrupt.size()).ok);
  std::cout<<"PASS: reject corrupted executable section by SHA-256\n";
  corrupt=file;
  corrupt[0x107]^=1;
  const auto forged=digest256::hash(corrupt.data()+0x102,36);
  std::memcpy(corrupt.data()+0xa0,forged.data(),32);
  const auto alternate=nsomemory::reconstruct(corrupt.data(),corrupt.size());
  assert(alternate.ok && !linktest::allowed(alternate));
  std::cout<<"PASS: checksum alone cannot bypass strict fixture allowlist\n";
  std::cout<<"All 7 linked-NSO tests passed. Actual ARM64 code execution requires Android device.\n";
}
