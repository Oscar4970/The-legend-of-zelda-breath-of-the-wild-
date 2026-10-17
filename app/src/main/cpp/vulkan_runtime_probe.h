#pragma once
// Hardware-only compatibility evaluation for an NVN -> Vulkan runtime.
// This does NOT assess BOTW game compatibility or infer expected FPS.
#include <cstdint>
#include <string>
namespace switchgpu {
struct Capabilities {
  std::uint32_t major=0,minor=0;
  bool graphicsQueue=false;
  bool dynamicRendering=false;
  bool createdDevice=false;
  bool astc=false, bc=false;
  bool subgroup32=false;
  bool indexUint8=false, depthClipControl=false;
};
inline bool versionAtLeast(const Capabilities& c,std::uint32_t major,std::uint32_t minor){
  return c.major>major || (c.major==major && c.minor>=minor);
}
inline bool satisfiesWonderGraphicsBaseline(const Capabilities& c){
  // Checked against upstream runtime/src/nvn/nvn_vk.cpp on 2026-10-17.
  return versionAtLeast(c,1,3) && c.graphicsQueue
         && c.dynamicRendering && c.createdDevice;
}
inline std::string conclusion(const Capabilities& c){
  if(!versionAtLeast(c,1,3))
    return "El driver no alcanza Vulkan 1.3, requisito del runtime Wonder revisado.";
  if(!c.graphicsQueue)
    return "No se encontro una cola grafica Vulkan compatible.";
  if(!c.dynamicRendering)
    return "Falta dynamicRendering, usado por el renderizador de referencia.";
  if(!c.createdDevice)
    return "La GPU anuncia funciones, pero no pudo crear el dispositivo de prueba.";
  return "BASE GRAFICA DE REFERENCIA: compatible con los requisitos Vulkan comprobados. "
         "NO demuestra compatibilidad con BOTW/NVN, juego, FPS o estabilidad.";
}
} // namespace switchgpu
