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

## Prueba v0.3 (ARM64 + Vulkan)

La pantalla inicial consulta **Vulkan nativo** mediante una biblioteca C++ enlazada con libvulkan, además de la información del sistema. El escenario 3D tiene vista de pantalla completa y un selector **Carga 1× / 4× / 12× / 24×**. Cada nivel repite el dibujo de 10,608 triángulos para comparar FPS; la geometría permanece visualmente igual. La prueba no mide el rendimiento de ningún juego comercial. Evita pruebas prolongadas en el nivel máximo para no calentar innecesariamente el teléfono.

Se necesita NDK y CMake para compilar. GitHub Actions instala ambos y verifica que el APK contiene `lib/arm64-v8a/libbotwgpu.so`.

## v0.4: Analizador NSO0 para Switch

Se agregó el botón **Analizar NSO de Switch (solo metadatos)**. Usa el selector de archivos de Android y lee únicamente la cabecera de 256 bytes del NSO mediante `pread` en el descriptor proporcionado por el sistema.

Identifica la firma NSO0, nombre de secciones, tamaños, offsets, flags de compresión y hash, BSS y Module ID. Valida límites y superposición de segmentos. No intenta ejecutar ni descifrar archivos, descomprimir secciones ni verificar hashes o autenticidad.

El resultado se muestra solo en la pantalla y el archivo no se sube, almacena ni modifica. Por seguridad, el analizador limita los archivos a 512 MiB. Si se elige un archivo sin descifrar, se rechazará. Los tests automáticos usan un NSO **sintético**, sin ninguna ROM.

**Limitación:** este avance analiza el formato de ejecutable de Switch; no implementa los servicios del sistema, el cargador de memoria, NVN/Vulkan ni el motor BOTW. No es un port jugable.
