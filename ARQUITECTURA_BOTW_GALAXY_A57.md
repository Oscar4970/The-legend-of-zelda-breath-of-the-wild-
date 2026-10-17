# Decisión de arquitectura: BOTW Switch recompilado para Galaxy A57

Fecha: 2026-10-17. Estado: **DECIDIDO como línea principal de investigación; no existe aún port jugable**.

## Decisión

Priorizar la edición de **Nintendo Switch** de `The Legend of Zelda: Breath of the Wild` mediante **recompilación estática AArch64 → C++ → código nativo Android arm64-v8a**, con adaptación de servicios Horizon y **NVN/Maxwell → Vulkan**. El resultado buscado es un **APK nativo** que cargue datos de una copia obtenida legalmente por el usuario, similar al enfoque de recompilación nativa empleado por LibertyRecompAndroid para GTA IV. **No** significa usar el motor de GTA IV ni emular una consola Switch completa. La correspondencia AArch64 de la consola y teléfono reduce traducción ISA, pero la recompilación no garantiza rendimiento ni compatibilidad.

**Biblioteca de referencia para estudiar e incorporar legalmente:** https://github.com/BelmanteGu/SuperMarioWonderRecomp, MIT, compilador `recomp/`, runtime `runtime/`, `runtime/src/nvn/` y base Android. El recompiler se fijó a `7c2531d9bdce94506bad9bf53d8a9848b240f111` para nuestra prueba reproducible v0.18. No copiar aplicaciones/ejecutables ni recursos de Nintendo ni componentes de terceros sin verificar sus licencias. Trabajar desde hitos comprobables y actualizar la referencia solo tras pruebas.

**Segundo camino, únicamente de evaluación:** BOTW de Wii U mediante recompilación PowerPC→ARM64 y GX2→Vulkan. https://github.com/hardkiller2565123123/CafeRecomp estudia concretamente BOTW Wii U, pero su README no declara todavía port Android jugable. https://github.com/GreenNaugahyde/ZeldaWWHDRecompAndroid sí ejecuta *Wind Waker HD* recompilado en Android; demuestra que esta vía es posible para **otro** juego, no BOTW. La ruta Wii U requiere traducir PowerPC, manejar orden de bytes big-endian y Cafe OS/GX2. **No** mezclar implementaciones de licencias incompatibles. CafeRecomp tiene subcomponentes con licencia GPL-3.0 y MPL-2.0: auditar licencias y autorización antes de incorporar código. Reevaluar solo si aparece una base BOTW Wii U para Android considerablemente más avanzada y verificable, o se bloquea la ruta Switch de manera fundamental.

**Emuladores Switch/Wii U:** únicamente pruebas de referencia, no la arquitectura del port recompilado. No asumir que el rendimiento en emulación sea representativo del ejecutable nativo.

## Evidencia en el teléfono — no extrapolar FPS

| Comprobación Samsung Galaxy A57 SM-A576B | Estado |
| --- | --- |
| Android ARM64 | Confirmado |
| Vulkan Xclipse 550 1.4.304, dynamicRendering | Confirmado por APK v0.15 |
| Comandos GPU, creación de imagen y readback RGBA | Confirmado por captura APK v0.16 |
| Formatos ASTC, uint8, subgroup32 | Confirmado por v0.15 |
| BC1 nativo | No |
| D24/S8 | No; D32/S8, D32 y D16 disponibles en v0.16 |
| RAM Android total | 7247 MiB detectados |
| RAM disponible durante prueba | 2908 MiB, cambia constantemente |
| Código C++ generado por recompiler MIT desde nuestro NSO ARM64 | **Confirmado por captura APK v0.18**: `42` |
| Pipeline SPIR-V v0.17 ejecutado en Samsung | **Aún sin captura confirmatoria** |
| Homebrew completo recompilado con Horizon/HLE | No |
| Código original de BOTW procesado/ejecutado | No |
| FPS, estabilidad térmica o jugabilidad BOTW | **Sin datos** |

La v0.18 es una **muestra propia mínima** con `MOV W0,#42; RET`, no un NSO comercial, ni demuestra ABI completo, sistema de archivos, hilos, audio, gráficos NVN o funcionamiento del juego.

## Optimización prevista y pruebas de aceptación

**Prioridad 1: corrección y estabilidad.** No reducir resolución hasta que haya un frame correcto. Crash logs, seguimiento del ciclo de vida, pérdida/recuperación de contexto Vulkan, manejo explícito de errores y memoria disponible.

**Perfil inicial de investigación, no promesa de rendimiento:**
- Objetivo de experiencia: intentar **30 FPS estables** antes que intentar 60, sujetos a pruebas reales en juego. Frecuencia estable y latencia apropiada importan más que picos de FPS.
- Probar **540p** como resolución inicial de renderizado *cuando funcione el motor*. Permitir ajuste posterior (p. ej. menos de 540p ante cuellos de botella o 720p si el rendimiento lo permite). La interfaz se escala a pantalla manteniendo proporciones.
- ASTC nativo cuando corresponda; convertir o transcodificar BCn **si el propio juego y el driver requieren ese formato**. No afirmar soporte BC1 nativo. Evaluar alternativa D32/S8 para usos D24/S8, con pruebas de equivalencia de profundidad/esténcil.
- Shader Maxwell→SPIR-V con trabajo/caché fuera del hilo de render; caché persistente de pipelines compatible con versión de controlador. Compilar shaders del juego **solo con entrada legítima y en entorno privado**. La recompilación de CPU y traducción de shaders offline preferiblemente fuera del teléfono si las herramientas ocupan mucha RAM.
- Medir RAM real: picos, procesos cerrados, estrés de shaders/streaming y memoria disponible. **8 GB de RAM no garantiza** que BOTW quede completamente alojado ni rápido.
- Medir temperaturas, memoria y percentiles de latencia durante al menos 20-30 min **después** de conseguir gameplay; no inferir resultados del triángulo Vulkan, la muestra 42 o Wonder en un Snapdragon.

## Hoja de ruta por criterios de finalización (sin fechas ni porcentajes falsos)

1. **Superado:** v0.18: código AArch64 pequeño propio recompilado vía upstream a C++, ejecutado en host y Samsung, con prueba verificable de 42.
2. **Próximo hito prioritario:** compilar **un programa homebrew ARM64 externo desde fuentes abiertas/propias reales**, no construir bytes ARM64 a mano. Incluir llamadas entre funciones, .data/.bss, pila, importaciones, relocalizaciones y una llamada controlada a Horizon HLE. El mismo binario debe generar código C++ y ejecutarse en Android; logs con resultados comparables. No confundir con ejecución de instrucciones ARM64 por intérprete.
3. **Runtime reutilizable:** integrar/aislar las partes autorizadas del runtime de SwitchGamesRecomp (CPU ABI, TLS, importaciones y SVC, threads/mutexes, IPC/filesystem). Probar con homebrew que use servicios de manera reproducible. Mantener perímetro de seguridad para entradas no confiables.
4. **Preflight de BOTW**, *cuando se disponga de una copia auténtica obtenida legalmente*: versión precisa, inventario de `main`, `sdk`, otros NSO/archivos, memoria, número de funciones, cobertura de instrucciones y símbolos. Hacerlo **local y privado**, sin subir dumps, keys, firmware, ROM/ExeFS/RomFS ni C++ recompilado del juego a un GitHub público/CI.
5. **Primera ejecución de código del juego:** detectar primer entrypoint alcanzado, registrar servicios faltantes y resolver de forma incremental. Solo declarar \"arranque de BOTW\" cuando se haya ejecutado su propio código y haya evidencia reproducible.
6. **Primer frame correcto y menú**: integrar NVN/Vulkan, shaders, texturas, profundidad, UI y entradas. El demo SPIR-V no sustituye este hito.
7. **Gameplay, audio, guardado, estabilidad y FPS**: después de arrancar, probar de forma prolongada en Galaxy A57, identificar cuellos de botella y optimizar con perfiles reales.

## Dependencia imprescindible

A fecha de esta decisión, el usuario indicó que **no posee copia de Switch ni Wii U de BOTW**. No se puede obtener código ni recursos del juego desde los proyectos abiertos. Hasta disponer de archivos legítimos, el desarrollo puede avanzar en recompilador/homebrew/runtime, pero **no se puede comprobar el arranque ni garantizar un APK jugable de BOTW**. No buscar ni distribuir ROMs piratas, keys o ejecutables de Nintendo.

## Regla de seguimiento

No aumentar porcentajes o describir un APK como port jugable por completar pruebas de hardware o instrucciones sintéticas. Cada avance necesita evidencia reproducible (build, test y, si aplica, captura en el teléfono). Si una evidencia de Wii U demuestra una alternativa objetivamente superior, documentar los criterios y reconsiderar explícitamente esta decisión.

## Fuentes de arquitectura

- https://github.com/BelmanteGu/SuperMarioWonderRecomp
- https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/main/docs/FRAMEWORK.md
- https://github.com/hardkiller2565123123/CafeRecomp
- https://github.com/GreenNaugahyde/ZeldaWWHDRecompAndroid
- https://github.com/zeldaret/botw
- https://github.com/vaduur/LibertyRecompAndroid
