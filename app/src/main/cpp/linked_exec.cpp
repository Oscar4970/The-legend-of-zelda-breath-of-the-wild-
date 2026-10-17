#include "linked_nso.h"
#include <jni.h>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <string>
#if defined(__aarch64__) && defined(__ANDROID__)
#include <sys/mman.h>
#include <unistd.h>
#endif

extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeRunLinkedArm64(JNIEnv* env,jclass){
#if !defined(__aarch64__) || !defined(__ANDROID__)
  return env->NewStringUTF("Esta prueba solo es compatible con Android ARM64.");
#else
  // Always use a known built-in fixture, not a file, a ROM, or external instructions.
  const auto file=linktest::makeNso();
  const auto image=nsomemory::reconstruct(file.data(),file.size());
  if(!linktest::allowed(image))
    return env->NewStringUTF("Prueba cancelada: modulo ficticio no autorizado.");

  const long page=sysconf(_SC_PAGESIZE);
  if(page<4096 || page>65536 || (65536%page)!=0)
    return env->NewStringUTF("Prueba cancelada: tamano de pagina incompatible.");
  const std::size_t mapSize=3*65536;
  void* raw=mmap(nullptr,mapSize,PROT_READ|PROT_WRITE,
                 MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(raw==MAP_FAILED)return env->NewStringUTF("Android denego reservar memoria de prueba.");

  auto* memory=static_cast<std::uint8_t*>(raw);
  std::memcpy(memory,image.bytes.data(),image.bytes.size());

  if(!linktest::applyFixups(memory,image.bytes.size())){
    munmap(raw,mapSize);
    return env->NewStringUTF("No se pudieron aplicar los enlaces ARM64 de prueba.");
  }
  __builtin___clear_cache(reinterpret_cast<char*>(memory),
                          reinterpret_cast<char*>(memory+36));
  // W^X: text RX, const RO, data/BSS RW; 64-KiB-aligned boundaries.
  if(mprotect(memory,65536,PROT_READ|PROT_EXEC)!=0){
    const int code=errno;
    munmap(raw,mapSize);
    std::ostringstream msg;
    msg<<"Android no permitio proteger codigo RX (errno "<<code<<").";
    const auto out=msg.str();
    return env->NewStringUTF(out.c_str());
  }
  if(mprotect(memory+65536,65536,PROT_READ)!=0){
    const int code=errno;
    munmap(raw,mapSize);
    std::ostringstream msg;
    msg<<"Android no permitio proteger .rodata (errno "<<code<<").";
    const auto out=msg.str();
    return env->NewStringUTF(out.c_str());
  }

  using Function=std::uint32_t(*)(std::uint32_t,std::uint32_t);
  auto function=reinterpret_cast<Function>(memory);
  const std::uint32_t result=function(19,23);
  munmap(raw,mapSize);

  std::ostringstream report;
  report<<"NSO ficticio: LZ4 y 3 SHA-256 correctos\n"
        <<"Enlace BL entre funciones: aplicado\n"
        <<"Referencia LDR a .rodata: aplicada\n"
        <<"Proteccion: .text RX, .rodata R, .data RW\n"
        <<"Ejecucion de dos funciones: 19 + 23 + 7 = "<<result<<"\n"
        <<"Resultado: "<<(result==49?"PRUEBA CORRECTA":"PRUEBA FALLIDA")
        <<"\n\nSolo codigo sintetico integrado. No ejecuta BOTW ni ROMs.";
  const auto s=report.str();
  return env->NewStringUTF(s.c_str());
#endif
}
