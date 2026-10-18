# Recopilacion reproducible de Eden para investigar un port BOTW nativo

**Estado: herramienta de investigacion; NO es Zelda nativo, NI un port jugable.**

La ruta ya existente [port_botw](README.md) intenta usar recompilacion estatica
AArch64 a C++ y un runtime basado en SwitchGamesRecomp (MIT). Esa ruta sigue siendo
la candidata a port. Esta recopilacion **no sustituye** el recompiler ni agrega
los servicios Horizon/NVN que faltan.

Eden es un **emulador Switch GPLv3**, no contiene el motor ni el ejecutable de
Breath of the Wild. Tomar el APK de Eden, o copiar todo su codigo C++, no
convierte Zelda en una aplicacion nativa Android. El codigo fuente de referencia
se obtiene del commit exacto `de7e6e01d43e8cf2ec193d74fa053b0d7949955f`,
que corresponde al motor de las pruebas Eden del Galaxy A57.

## Que partes de Eden estamos evaluando

| Componente de Eden | Aplicacion potencial | Estado |
| --- | --- | --- |
| Deteccion Vulkan (`vulkan_device`) | Comprobar capacidades reales Xclipse 550 y seleccionar fallbacks | Referencia, NO integrado |
| Cache y pipelines Vulkan (`vk_pipeline_cache`) | Entender compilacion y memoria de shaders | Referencia; depende de multiples subsistemas del emulador |
| Decodificador Maxwell + SPIR-V (`shader_recompiler`) | Estudiar el tratamiento de shaders de Switch | Referencia; exige frontend y backend completos y adaptador NVN |
| Ventana Android (`emu_window`) | Ciclo de vida/Surface Android | Referencia; depende de Eden |
| `GpuDriverHelper.kt` | Registrar controlador de sistema | Referencia, NO sustituir el driver Samsung |

**No se incorpora FSR 3 ni generacion de fotogramas.** FSR 1 ya existe
en Eden, pero no arregla el problema de servicios Horizon o traduccion NVN.

## Generar ZIP de archivos reales de Eden (licencias conservadas)

```bash
git clone --filter=blob:none --no-checkout https://github.com/eden-emulator/mirror.git eden-fuente
git -C eden-fuente fetch --depth 1 origin de7e6e01d43e8cf2ec193d74fa053b0d7949955f
git -C eden-fuente checkout --detach de7e6e01d43e8cf2ec193d74fa053b0d7949955f
python3 port_botw/collect_eden_reference.py --eden-root eden-fuente --output Eden_BOTW_Native_Reference_Kit.zip
```

El script comprueba tanto el commit como los SHA Git de cada archivo;
rechaza alteraciones y rutas fuera de la lista. Incluye licencia GPLv3,
manifest de origen, codigo abierto del prototipo `port_botw` y solo las
fuentes de Eden expresamente autorizadas. **No copia ROM, ExeFS, RomFS,
claves, firmware ni un APK comercial.** ZIP no compilable por si solo.

### Licencias

La redistribucion o integracion de archivos GPLv3 de Eden en un programa
requiere cumplir la GPLv3 en la obra derivada y mantener sus avisos.
El runtime basado en SwitchGamesRecomp usa codigo MIT propio que puede
licenciarse en condiciones compatibles; la integracion se revisara modulo
por modulo y no se mezclaran fuentes de Nintendo en GitHub publico.

### Hito siguiente real

1. Probar un homebrew propio ARM64 ejecutando codigo recompilado y llamando
   servicios Horizon del runtime desde Android.
2. Integrar graficos minimos NVN->Vulkan de ese homebrew sin importar todo el
   GPU emulator de Eden.
3. Solo con una copia legal privada de BOTW, comprobar importaciones,
   reubicaciones y primer entrypoint autentico del juego.
4. Llegar a un frame real, despues a gameplay, y solo entonces optimizar
   para el Samsung Galaxy A57 y medir FPS.

Los resultados de Eden (10 FPS, cierres en shaders) no prueban ni predicen
el rendimiento de una hipotetica recompilacion nativa.
