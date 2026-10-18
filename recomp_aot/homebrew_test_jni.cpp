#include "homebrew_test_runtime.h"
#include <jni.h>
#include <sstream>

extern "C" JNIEXPORT jstring JNICALL
Java_org_example_botwlab_MainActivity_nativeRunCompiledHomebrew(JNIEnv* env,jclass){
  const auto r=homebrew_test::run();
  std::ostringstream out;
  out<<"C PROPIO -> ARM64 -> C++ RECOMPILADO -> ANDROID\n"
     <<"Fuente original: programa C independiente compilado con Clang.\n"
     <<"Incluye BL, pila, .data, .bss y SVC de prueba.\n"
     <<"Horizon ABI real: svc 0x1F (ConnectToNamedPort), 0x16 (CloseHandle).\n"
     <<"Servidor sm: SIMULADO localmente; todavia no IPC real.\n\n"
     <<"Prueba 1 (seed=7): "<<r.got_first<<" / "<<r.expected_first<<"\n"
     <<"Prueba 2 (seed=19): "<<r.got_second<<" / "<<r.expected_second<<"\n"
     <<"Contador BSS: "<<r.final_guest_counter<<" / 2\n"
     <<"Llamadas al servicio de PRUEBA: "<<r.service_calls<<" / 2\n"
     <<"Horizon: conectar sm: "<<r.horizon_connects<<" / 2\n"
     <<"Horizon: cerrar handle "<<r.horizon_closes<<" / 2\n"
     <<"Casos invalidos rechazados "<<r.rejected_calls<<" / 2\n"
     <<"Handles sin cerrar "<<r.handles_alive<<" / 0\n"
     <<(r.ok?"RESULTADO: EXITO; programa C recompilado.\n":"RESULTADO: FALLO; revisar diagnostico.\n")
     <<"El ABI SVC es real; los servicios IPC son pruebas locales, NO Horizon completo.\n"
     <<"No ejecuta Zelda.\n";
  if(!r.diagnostic.empty())out<<"Error: "<<r.diagnostic<<"\n";
  return env->NewStringUTF(out.str().c_str());
}
