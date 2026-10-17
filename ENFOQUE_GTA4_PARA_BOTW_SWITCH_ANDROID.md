# Enfoque de port: recompilación estática como GTA IV — actualizado 2026-10-17

**Estado real del proyecto:** APK v0.14 = herramientas de inspección NSO/NRO/MOD0, simulación de reubicaciones y pruebas ARM64 sintéticas. **Todavía no ejecuta homebrew externo ni BOTW.** No presentarlo como port funcional.

## Lo que revela el video de GTA IV del usuario

En el video de 58 segundos se observan Rockstar, GTA IV en pantalla, juego de Niko y una capa de controles táctiles de Xbox. La imagen del juego aparece como una franja horizontal dentro de una grabación vertical. **El video no muestra el identificador del APK**; por lo tanto no acredita de forma concluyente ejecución local, nombre de la aplicación ni cuál edición de GTA IV se usa.

Hay una coincidencia muy fuerte con **LibertyRecompAndroid**, que tiene ese tipo de control táctil y documentó en versiones anteriores un problema de pantalla de teléfono en orientación vertical:

- https://github.com/vaduur/LibertyRecompAndroid
- https://github.com/vaduur/LibertyRecompAndroid/releases
- https://github.com/OZORDI/LibertyRecomp

**Corrección al plan anterior:** LibertyRecompAndroid usa recompilación estática de la versión *Xbox 360* de GTA IV (PowerPC → C++ → aplicación nativa ARM64), una capa de sistema y un renderizador Vulkan. **No usa Wine/Winlator ni emulación completa de Xbox 360.** Winlator sigue siendo otra forma de ejecutar videojuegos PC en Android, pero **NO es la referencia principal de este video**.

El repositorio oficial LibertyRecompAndroid advierte que su renderizador está dirigido a **Adreno + Turnip** y que **Samsung Xclipse está fuera de soporte**. Que el video muestre juego en el Galaxy del usuario no demuestra que ese APK concreto funcione allí; podría ser otro fork, driver o solución. Registrar el nombre y versión reales antes de atribuir el método.

## El equivalente práctico para BOTW de Switch

Nuestro objetivo sería un pipeline de **recompilación estática AArch64 → C++ o LLVM IR → compilación nativa arm64-v8a**, junto con una reimplementación compatible de Horizon y de NVN/Maxwell a Vulkan. Como CPU de Switch y teléfono ya son AArch64, recompilarlo no necesariamente da una gran ventaja de CPU; las dificultades reales son ABI, syscall SVC, TLS, sistema de memoria, sincronización, IPC, NVN, shaders y estado del motor.

No es suficiente colocar ROM + APK, ni incorporar el código de ReXGlue para Xbox 360: **ReXGlue fue diseñado para PowerPC/XEX de Xbox**, no para ARM64/NSO. Usarlo requeriría reescribir gran parte de su frontend y runtime.

### Referencias específicas de Switch (estudiar primero)

| Proyecto | Qué aporta | Límites |
| --- | --- | --- |
| [SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp) | Recompiler AArch64→C++, runtime Horizon, traducción NVN→Vulkan, y un proyecto Android real; MIT para su código propio | Es un port de otro juego; exige revisar estabilidad, APIs faltantes, licencias de terceros y compatibilidad Xclipse |
| [LASRecomp](https://github.com/peiikai/LASRecomp) | Recompilación estática de Link's Awakening Switch, runtime nativo y gráficas; código propio MIT | Android no declarado como plataforma oficialmente compatible en el README |
| [mk8-recomp](https://github.com/dougchansan/mk8-recomp) | Variante AOT/híbrida para juegos Switch con HLE heredado de suyu | Orientado a Mario Kart 8; no implica capacidad Android/BOTW |
| [Skyline](https://github.com/skyline-emu/skyline) | Arquitectura histórica de CPU ARM64 nativo con HLE en Android | Descontinuado; licencia y código reutilizable deben revisarse en cada fuente |

**Recomendación de base de estudio:** auditar primero `SuperMarioWonderRecomp/recomp`, `runtime`, `runtime/src/nvn`, `android` y `docs/nvn/ANDROID-PORT.md` para determinar qué puede reutilizarse legal y técnicamente en el teléfono con GPU **Samsung Xclipse 550**.

## Hitos de aceptación

1. **Auditoría de código y licencias.** Compilar componentes propios o librerías abiertas sin juegos comerciales ni claves. Obtener un mapa de componentes reutilizables (AArch64/AOT, ABI, memoria, Horizon HLE, NVN-Vulkan, Android UI), advertencias y pruebas.
2. **Prototipo AOT reproducible de software propio y autorizado.** Compilar una función y un módulo homebrew ARM64 de código fuente propio para que el pipeline los traduzca y ejecute nativamente en Android, no solo interprete metadatos. Comparar resultados contra ejecución en un entorno independiente.
3. **Runtime Horizon mínimo:** configurar ABI, handles, hilos, syscalls y salida, y demostrar que el programa de prueba usa servicios; reportar incompatibilidades sin comprometer seguridad del Android anfitrión.
4. **Gráficos Vulkan en Xclipse:** renderizador compatible, shaders, caché y controles; medir FPS en un homebrew autorizado.
5. **Evaluación BOTW:** solo con una copia legítima de los datos y un runtime comprobado. No publicar ni subir ejecutables recompilados con código comercial, ROMs, keys o firmware.

**No afirmar éxito de BOTW sin iniciar su código original, renderizar gameplay y medir estabilidad.** El port completo podría no ser viable con el tiempo, recursos o compatibilidad disponibles.

### Cambio de prioridad respecto a v0.14
No crear APKs nuevos solo para contar más cabeceras o SVC. La siguiente entrega debe demostrar un **componente real de recompilación/runtime** o un informe técnico de auditoría con pruebas reproducibles y fallos conocidos.

### Enlaces de referencia
- https://github.com/vaduur/LibertyRecompAndroid
- https://github.com/OZORDI/LibertyRecomp
- https://github.com/rexglue/rexglue-sdk
- https://github.com/BelmanteGu/SuperMarioWonderRecomp
- https://github.com/peiikai/LASRecomp
