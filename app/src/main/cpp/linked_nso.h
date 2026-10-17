#pragma once
// Fictional AArch64 NSO module with two linked functions and a PC-relative constant.
// Contains no Nintendo instructions/assets. Only this exact internal sample is executable.
#include "nso_memory.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace linktest {
inline constexpr std::uint32_t kTextWords[9] = {
  0xa9bf7bfdu, // stp x29,x30,[sp,#-16]!
  0x910003fdu, // mov x29,sp
  0x94000000u, // bl second_function - patched before execution
  0xa8c17bfdu, // ldp x29,x30,[sp],#16
  0xd65f03c0u, // ret
  0x18000002u, // ldr w2, pc_relative_literal - patched before execution
  0x0b010000u, // add w0,w0,w1
  0x0b020000u, // add w0,w0,w2
  0xd65f03c0u  // ret
};
inline constexpr std::uint32_t kFunctionOffset=20;
inline constexpr std::uint32_t kBranchOffset=8;
inline constexpr std::uint32_t kLiteralOffset=20;
inline constexpr std::uint32_t kRoOffset=0x10000;
inline constexpr std::uint32_t kDataOffset=0x20000;
inline constexpr std::uint32_t kConstant=7;

inline std::uint32_t read32(const std::uint8_t* p) {
  return std::uint32_t(p[0]) | std::uint32_t(p[1])<<8
       | std::uint32_t(p[2])<<16 | std::uint32_t(p[3])<<24;
}
inline void write32(std::uint8_t* p,std::uint32_t word) {
  for(int i=0;i<4;i++)p[i]=static_cast<std::uint8_t>(word>>(i*8));
}
inline void write32(std::vector<std::uint8_t>& p,std::size_t off,std::uint32_t word){
  write32(p.data()+off,word);
}
inline std::array<std::uint8_t,36> textBytes(){
  std::array<std::uint8_t,36> out{};
  for(int i=0;i<9;i++)write32(out.data()+i*4,kTextWords[i]);
  return out;
}
inline std::vector<std::uint8_t> makeNso(){
  std::vector<std::uint8_t> nso(0x180,0);
  nso[0]='N';nso[1]='S';nso[2]='O';nso[3]='0';
  write32(nso,0x0c,1u|8u|16u|32u);
  write32(nso,0x10,0x100);write32(nso,0x14,0);write32(nso,0x18,36);
  write32(nso,0x20,0x140);write32(nso,0x24,kRoOffset);write32(nso,0x28,4);
  write32(nso,0x30,0x150);write32(nso,0x34,kDataOffset);write32(nso,0x38,4);
  write32(nso,0x3c,8);
  write32(nso,0x60,38);write32(nso,0x64,4);write32(nso,0x68,4);
  const auto code=textBytes();
  nso[0x100]=0xf0; // LZ4 literal >=15 bytes
  nso[0x101]=21;   // 15+21=36 bytes of literal ARM64 code
  std::memcpy(nso.data()+0x102,code.data(),code.size());
  write32(nso,0x140,kConstant);
  write32(nso,0x150,0x12345678u);
  const auto codeHash=digest256::hash(code.data(),code.size());
  const auto roHash=digest256::hash(nso.data()+0x140,4);
  const auto dataHash=digest256::hash(nso.data()+0x150,4);
  std::memcpy(nso.data()+0xa0,codeHash.data(),32);
  std::memcpy(nso.data()+0xc0,roHash.data(),32);
  std::memcpy(nso.data()+0xe0,dataHash.data(),32);
  return nso;
}
inline bool allowed(const nsomemory::Image& image){
  if(!image.ok || image.hashChecks!=3 || image.base!=0 ||
     image.sectionLengths[0]!=36 || image.sectionLengths[1]!=4 ||
     image.sectionLengths[2]!=4 || image.sectionOffsets[0]!=0 ||
     image.sectionOffsets[1]!=kRoOffset || image.sectionOffsets[2]!=kDataOffset ||
     image.bssLength!=8 || image.bytes.size()!=kDataOffset+12)return false;
  const auto code=textBytes();
  if(std::memcmp(image.bytes.data(),code.data(),code.size())!=0)return false;
  if(read32(image.bytes.data()+kRoOffset)!=kConstant)return false;
  if(read32(image.bytes.data()+kDataOffset)!=0x12345678u)return false;
  for(int i=0;i<8;i++)if(image.bytes[kDataOffset+4+i]!=0)return false;
  return true;
}
inline bool patchBl(std::uint8_t* p,std::size_t size,std::size_t at,std::size_t target){
  if(!p || at+4>size || target>=size || at%4 || target%4 ||
     read32(p+at)!=0x94000000u)return false;
  const auto delta=static_cast<std::int64_t>(target)-static_cast<std::int64_t>(at);
  if(delta%4 || delta<-(1LL<<27) || delta>=(1LL<<27))return false;
  const auto immediate=static_cast<std::uint32_t>(delta/4)&0x03ffffffu;
  write32(p+at,0x94000000u|immediate);
  return true;
}
inline bool patchLdrW(std::uint8_t* p,std::size_t size,std::size_t at,std::size_t target){
  if(!p || at+4>size || target+4>size || at%4 || target%4 ||
     read32(p+at)!=0x18000002u)return false;
  const auto delta=static_cast<std::int64_t>(target)-static_cast<std::int64_t>(at);
  if(delta%4 || delta<-(1LL<<20) || delta>=(1LL<<20))return false;
  const auto immediate=static_cast<std::uint32_t>(delta/4)&0x7ffffu;
  write32(p+at,0x18000002u|(immediate<<5));
  return true;
}
inline bool applyFixups(std::uint8_t* p,std::size_t size){
  return patchBl(p,size,kBranchOffset,kFunctionOffset) &&
         patchLdrW(p,size,kLiteralOffset,kRoOffset);
}
}
