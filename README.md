# BOTW Android — laboratorio de compatibilidad

Este repositorio alberga una aplicación de **prueba técnica Android**, creada para comprobar renderizado OpenGL ES 3 y controles táctiles en teléfonos ARM64.

**No contiene ni ejecuta The Legend of Zelda: Breath of the Wild.** Todavía no es un port del juego. El objetivo es validar la compilación y el comportamiento del hardware antes de avanzar en una adaptación nativa.

## Compilar APK sin computadora

1. Abre la pestaña **Actions** del repositorio.
2. Entra a **Android test APK** y pulsa **Run workflow** (si no se ha ejecutado automáticamente).
3. Espera a que el trabajo de compilación termine con una marca verde.
4. Abre la ejecución, busca **Artifacts** y descarga **BOTW-Android-TestLab-NOT-GAME**.
5. Extrae el ZIP y abre `app-debug.apk` desde el administrador de archivos de Android.

Si el trabajo falla, no habrá APK válido. Comparte una captura del error de la ejecución y corregiremos la compilación.

La aplicación muestra información de CPU y detección de Vulkan, permite abrir un escenario simple OpenGL ES 3 y moverlo al arrastrar el dedo. **Los FPS mostrados son de esta prueba, nunca del Zelda original.**

No incluye ROMs, claves ni archivos de Nintendo. **Este repositorio está público**, así que no subas claves, ROMs ni información personal.