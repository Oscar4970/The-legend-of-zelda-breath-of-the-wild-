#include "../app/src/main/cpp/vulkan_runtime_probe.h"
#include <cassert>
#include <iostream>
using switchgpu::Capabilities;
int main() {
  Capabilities c{};
  assert(!switchgpu::satisfiesWonderGraphicsBaseline(c));
  std::cout<<"PASS: empty Vulkan capability set not classified as ready\n";
  c.major=1;c.minor=2;c.graphicsQueue=true;c.dynamicRendering=true;c.createdDevice=true;
  assert(!switchgpu::satisfiesWonderGraphicsBaseline(c));
  assert(switchgpu::conclusion(c).find("1.3")!=std::string::npos);
  std::cout<<"PASS: Vulkan 1.2 not misclassified as Wonder 1.3 runtime compatible\n";
  c.minor=3;c.dynamicRendering=false;
  assert(!switchgpu::satisfiesWonderGraphicsBaseline(c));
  std::cout<<"PASS: Vulkan 1.3 without dynamic rendering rejected\n";
  c.dynamicRendering=true;c.createdDevice=false;
  assert(!switchgpu::satisfiesWonderGraphicsBaseline(c));
  std::cout<<"PASS: uncreated Vulkan device not reported as working\n";
  c.createdDevice=true;c.graphicsQueue=false;
  assert(!switchgpu::satisfiesWonderGraphicsBaseline(c));
  std::cout<<"PASS: GPU lacking graphics queue rejected\n";
  c.graphicsQueue=true;
  assert(switchgpu::satisfiesWonderGraphicsBaseline(c));
  std::cout<<"PASS: Vulkan 1.3 dynamic rendering and created device accepted\n";
  c.major=1;c.minor=4;
  assert(switchgpu::satisfiesWonderGraphicsBaseline(c));
  std::cout<<"PASS: Vulkan 1.4 baseline accepted\n";
  c.astc=false;c.bc=false;c.indexUint8=false;c.depthClipControl=false;c.subgroup32=false;
  assert(switchgpu::satisfiesWonderGraphicsBaseline(c));
  std::cout<<"PASS: optional GPU features never misreported as hard mandatory requirements\n";
  assert(switchgpu::conclusion(c).find("NO demuestra compatibilidad")!=std::string::npos);
  std::cout<<"PASS: result explicitly avoids claims of BOTW runnability\n";
  std::cout<<"All 9 Vulkan runtime compatibility evaluation tests passed.\n";
}
