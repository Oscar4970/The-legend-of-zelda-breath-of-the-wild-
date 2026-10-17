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


## v0.5 — Descompresión y memoria aislada NSO

El nuevo botón **Reconstruir imagen NSO** lee archivos locales NSO0 de hasta **16 MiB**, reconstruye secciones .text/.rodata/.data en un búfer separado, utiliza un decodificador de bloques LZ4 con comprobaciones de límites y inicializa BSS con ceros. La imagen nunca se marca ejecutable y se descarta tras mostrar un resumen; no implementa carga de relocaciones, servicios de Switch, NVN ni arranque de BOTW. SHA-256 no se verifica. Las pruebas utilizan archivos sintéticos sin contenido de Nintendo.


## v0.6 — Integridad de secciones NSO0

Cuando una cabecera NSO0 marca una sección con hash obligatorio, el laboratorio calcula SHA-256 sobre la sección **ya descomprimida** y rechaza el archivo si el resultado no coincide con el hash de la cabecera. Para mantener el alcance explícito, se rechaza la nueva compresión ZBIC, que no implementamos. Esto **no verifica firmas digitales ni autenticidad del juego**. Incluye pruebas SHA-256 con vectores conocidos, pruebas de corrupción y descompresión previas. Se ajustó la pantalla inicial para respetar los márgenes del sistema en versiones recientes de Android.


## v0.7 — Primera prueba de ejecución de instrucciones ARM64

El botón **Ejecutar función ARM64 ficticia (19 + 23)** genera un **NSO0 sintético integrado en el APK**, con secciones de datos, LZ4 y tres hashes SHA-256. Después de comprobarlos, el programa solo permite ejecutar una **única secuencia de dos instrucciones AArch64 permitida expresamente**: `add w0,w0,w1` y `ret`. El código se copia a una página sin permiso de ejecución, se sincroniza la caché de instrucciones, y se cambia mediante `mprotect` a **lectura + ejecución** (nunca escritura + ejecución simultánea). Se llama con argumentos 19 y 23 y se espera 42. La página se libera después.

**Seguridad:** la función no lee ejecutables proporcionados por usuarios, no ejecuta código arbitrario, no permite instrucciones de ROM, no desactiva controles de Android y no necesita permisos del sistema. Si Android impide `mprotect`, muestra el error. Cualquier fallo o resultado inesperado debe notificarse con una captura, sin seguir insistiendo.

**Alcance:** esta es únicamente una prueba AArch64/ABI con bytes totalmente sintéticos. No es un cargador de BOTW, no implementa bibliotecas de Switch, reubicación dinámica, ASLR del juego, símbolos, NVN ni servicios de Nintendo. Superar la prueba no garantiza ni estima cuánto falta para un port jugable.


## v0.8 — Enlaces relativos y regiones separadas de memoria

La prueba **«Probar enlace ARM64»** reconstruye un archivo NSO0 ficticio que contiene dos funciones AArch64 y un valor constante en `.rodata`. Aplica dos correcciones controladas: una llamada relativa `BL` entre funciones y un acceso `LDR` relativo a una constante. Protege la memoria en tres regiones independientes: código RX, datos constantes R y datos mutables RW. El resultado correcto es `19 + 23 + 7 = 49`.

**Se ejecuta exclusivamente el módulo sintético integrado**, validado por SHA-256 y una lista estricta de instrucciones admitidas. No se ejecutan ROMs o módulos externos. Esta práctica de enlaces relativos no implementa el enlazador dinámico ni los servicios de Switch, y no indica compatibilidad con BOTW.


## v0.9 — Inspección MOD0/ELF64 de Switch (solo lectura)

La función **«Inspeccionar MOD0, bibliotecas y enlaces de Switch»** permite seleccionar un archivo NSO0 *sin cifrar* de origen legítimo. Después de validar sus secciones y hashes requeridos, interpreta la ubicación MOD0, la tabla `.dynamic` (ELF64), dependencias `DT_NEEDED`, nombres de símbolos y entradas de reubicación `RELA`. Informa los tipos AArch64 `R_AARCH64_RELATIVE`, `GLOB_DAT`, `JUMP_SLOT` y `ABS64` sin modificar direcciones ni ejecutar instrucciones.

Este parser es experimental y solo admite archivos locales de hasta 16 MiB con el tipo de información compatible. El formato completo de Switch incluye más casos de los que todavía contemplamos. Incluimos un **NSO ficticio de prueba** y pruebas negativas de límites para evitar que un archivo malformado se procese de manera insegura.

Fuentes de los campos usados: https://switchbrew.org/wiki/MOD y https://switchbrew.org/wiki/NSO0 .

**No es un port ni un emulador jugable**: la inspección de referencias no significa que podamos resolverlas, cargar sus bibliotecas o ejecutar BOTW. No contiene código, ROMs ni claves de Nintendo.


## v0.10 — Soporte de homebrew Nintendo Switch (.nro)

La mayoría de las demostraciones homebrew se distribuyen como archivos `.nro`, mientras que nuestros análisis anteriores usaban `.nso`. Se agregó el botón **Inspeccionar homebrew NRO de Switch (solo lectura)**, que permite examinar la cabecera `NRO0`, las tres secciones, BSS y el identificador de módulo. Si hay recursos opcionales `ASET`, reporta el tamaño de icono, NACP y RomFS. Si el archivo contiene `MOD0`, utiliza el analizador existente para inventariar dependencias y reubicaciones sin ejecutarlas.

**Límites:** archivos locales de máximo 16 MiB, solo NRO convencional sin compresión moderna. MOD0 puede no estar presente o no ser procesable, sin que eso signifique que el NRO es inválido. No autentica archivos, ni ejecuta el código ni utiliza claves de Nintendo. Esta versión sigue siendo una herramienta de compatibilidad experimental, no un port de BOTW.

Documentación: https://switchbrew.org/wiki/NRO0 .


## Verificación con un NRO público independiente

El trabajo automático de GitHub también descarga **`ftpd-classic.nro`**, versión `v3.2.1`, desde el [proyecto abierto ftpd](https://github.com/mtheall/ftpd/releases/tag/v3.2.1), y prueba el lector de NRO de forma **exclusivamente estática**. El binario no se incluye en el APK ni en este repositorio; GitHub Actions lo descarta al finalizar. Se reconoce su estructura, pero **no se ejecuta el programa ni se inicia ningún servidor FTP**. Si el formato no es compatible, la prueba fallará, y la corrección deberá quedar registrada en el repositorio. El código fuente pertenece a sus autores originales.


### Interpretación de BSS en homebrew real

Una prueba de solo lectura con el NRO abierto `ftpd-classic.nro` detectó que algunas cabeceras MOD0 delimitan BSS dentro del tramo total de datos inicializados + BSS, sin coincidir exactamente con los límites indicados por el encabezado NRO. El analizador acepta esa distribución solo si el intervalo está dentro de la imagen reconstruida; no cambia permisos de memoria ni ejecuta el archivo. Se incluyó una prueba de regresión para este caso.


## v0.11 — Inventario REL y etiquetas ELF64

La vista MOD0 ahora también inspecciona las relocaciones `DT_REL` además de `DT_RELA`, mostrando los tipos y valores de todas las etiquetas `.dynamic` encontradas. Esto ayuda a evitar conclusiones incorrectas cuando un módulo no contiene relocaciones RELA. Las nuevas pruebas comprueban una entrada REL válida y un tamaño de entrada inválido. **Es un lector estático: no enlaza ni ejecuta programas.**


## v0.12 — Formato compacto RELR verificado

Gracias a la prueba v0.11 con homebrew real se detectó que el NRO publica las etiquetas `DT_RELRSZ=35`, `DT_RELR=36` y `DT_RELRENT=37`. Se agregó un **decodificador de inventario RELR ELF64**: examina entradas de dirección y sus bitmaps de 63 posiciones para contar los destinos de reubicación, comprobando alineación y límites. Nunca modifica la memoria, nunca ejecuta código externo y mantiene límites estrictos de tamaño. GitHub Actions exige ahora detectar entradas y destinos RELR en `ftpd-classic.nro` de código abierto. Incluye pruebas sintéticas de bitmap malformado y destinos fuera de memoria.

Los números de esas etiquetas se contrastaron con Android Bionic y el algoritmo de decodificación con LLVM. Es un avance en el analizador, no un cargador de juego.


## v0.13 — Reubicaciones RELR simuladas sin ejecutar código

La nueva función **Simular RELR en copia de NRO (NO ejecuta)** analiza un archivo NRO abierto, comprueba su MOD0 y expande su tabla compacta RELR en una **imagen de memoria separada y no ejecutable**. Solo en esa copia suma un desplazamiento de carga simulado `0x100000000` a los punteros indicados; informa el número de punteros modificados y un ejemplo antes/después. El archivo original no se modifica, el módulo no se ejecuta y nunca se crean páginas de memoria ejecutables. Antes de modificar la copia, valida todos los destinos, los rangos y posibles desbordamientos.

GitHub Actions ejecuta **11 pruebas unitarias nuevas** con datos ficticios, incluidas entradas dañadas, y exige que la misma simulación funcione con `ftpd-classic.nro` de código abierto. Esto comprueba el procesamiento de tablas RELR en una copia, **no significa que exista un cargador funcional para Switch o Zelda**. Las importaciones, los servicios Horizon y el motor NVN/Vulkan permanecen pendientes.


## v0.14 — Prefiltro del entorno de ejecución NRO real

Se añadió **«Analizar requisitos para ejecutar NRO real (no ejecuta)»**. Examina la instrucción inicial de salto AArch64 de un NRO real, confirma que el salto apunte al código dentro de `.text`, e inventaría patrones `SVC` (llamadas al kernel Horizon) y `BRK`. Identifica dependencias pendientes, entre ellas el **Homebrew ABI** (X0: puntero LoaderConfig, X1: `-1`), los identificadores de la configuración de lanzamiento, TLS y el mecanismo de salida del programa. El recuento de `SVC` es **heurístico**: algunos bytes podrían pertenecer a datos y no se garantiza que las instrucciones sean alcanzables.

Se añadió una estructura C++ de `LoaderConfigEntry` (24 bytes) y validación sintética de las entradas obligatorias MainThreadHandle, AppletType y EndOfList. Eso verifica la **estructura del protocolo, no implementa todavía los servicios ni fabrica handles Horizon**. Este prefiltro nunca ejecuta un NRO externo. GitHub Actions comprueba el salto inicial y su ubicación en un homebrew publicado de manera independiente.

Fuentes: https://switchbrew.org/wiki/Homebrew_ABI y https://switchbrew.org/wiki/SVC y https://github.com/switchbrew/nx-hbloader .


## v0.15 — Diagnóstico de GPU para recompilación nativa de Switch

El botón **«Diagnosticar compatibilidad gráfica Vulkan para Switch»** consulta el **driver real del teléfono**, sin ROM ni código de juego. Comprueba Vulkan 1.3, `VkPhysicalDeviceVulkan13Features.dynamicRendering`, disponibilidad de cola gráfica, y realiza una **creación y destrucción de dispositivo lógico Vulkan temporal** con `dynamicRendering` habilitado. También informa ASTC/BC, formatos de profundidad, extensiones opcionales (`VK_EXT_index_type_uint8`, `VK_EXT_depth_clip_control`), propiedades de subgrupo y memoria anunciada por Vulkan y Android. No modifica la configuración del dispositivo ni prueba juegos.

Los criterios de esta prueba se basan en el código de `runtime/src/nvn/nvn_vk.cpp` y el documento `docs/WONDER-ANDROID-OPTIONS.md` del proyecto MIT [SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp). Las funciones opcionales pueden contar con rutas de compatibilidad; no son siempre bloqueos. **Pasar la prueba no asegura rendimiento ni ejecución de Super Mario Wonder o Zelda**: faltan el código recompilado, los servicios Horizon, NVN, los shaders Maxwell→SPIR-V y los archivos legítimos de cada juego. El código de estos proyectos no se ha integrado aún.

Ver [AUDITORIA_WONDER_A_BOTW.md](AUDITORIA_WONDER_A_BOTW.md) para decisiones arquitectónicas, dependencias y criterios de éxito.


## v0.16 — Prueba de renderizado Vulkan real y lectura de píxeles

Tras la prueba v0.15 en el Samsung SM-A576B (Xclipse 550, Vulkan 1.4.304), agregamos **«Probar GPU Vulkan real: dibujar color y verificar píxeles»**. En lugar de limitarse a leer características declaradas, crea temporalmente una imagen Vulkan RGBA8 de **16 × 16**, utiliza `vkCmdBeginRendering` con `LOAD_OP_CLEAR` para generar color (0.25, 0.5, 0.75, 1), copia los píxeles a un búfer de lectura, espera la finalización y verifica el color **RGBA = (64, 128, 191, 255)** con tolerancia de dos niveles por canal. Libera los recursos gráficos tras la prueba; no modifica configuraciones ni crea archivos permanentes. También consulta si existen alternativas D32/S8, D32 y D16 a la ausencia de D24/S8 vista en el Galaxy. Se ejecuta en un hilo de la aplicación.

**Limitaciones estrictas:** la prueba de GPU tiene que confirmarse en el Samsung; compilar el APK no significa que los comandos Vulkan funcionen allí. No crea una pipeline de shaders, no dibuja triángulos, no prueba NVN ni contenido original de Breath of the Wild. El código no incluye datos ni código propietario. Un problema con el driver debe tratarse como hallazgo de compatibilidad y no como fallo de Zelda.


## v0.17 — Pipeline Vulkan con shaders SPIR-V y triángulo real

El nuevo botón **«Probar TRIANGULO REAL con shaders SPIR-V y Vulkan»** crea una imagen RGBA8 de **64x64** en la GPU, compila dos shaders GLSL originales del repositorio a SPIR-V antes de construir el APK, configura un **VkGraphicsPipeline** con Vulkan 1.3 `dynamicRendering`, realiza `vkCmdDraw(...,3,...)`, copia el resultado a la CPU y comprueba un píxel interior **RGBA 230,51,26,255** y dos píxeles de fondo **RGBA 13,26,77,255** (tolerancia 2). El test usa `gl_VertexIndex` y no necesita archivos externos ni permisos peligrosos.

El build de GitHub instala `glslang-tools` y `spirv-tools`, compila los shaders del directorio `shaders/` con `glslangValidator --target-env vulkan1.3` y ejecuta `spirv-val`. El archivo de cabecera generado no se publica en el repositorio como binario; el script `scripts/generate_triangle_spirv.py` permite recrearlo. Para compilación local hay que ejecutar ese script antes de Gradle.

**La ejecución real del pipeline todavía debe validarse en el Samsung**: las pruebas unitarias de la CI solo comprueban criterios de lectura de píxeles y la correcta construcción del APK. Este triángulo con SPIR-V propio **no traduce NVN ni shaders propietarios de Zelda**, ni ejecuta el juego. Es una prueba del camino Vulkan: shader → pipeline → GPU → píxeles.


## v0.18 — Primer código recompilado desde instrucciones ARM64 reales

**Avance distinto de versiones anteriores:** el APK ahora contiene una función C++ **generada automáticamente por el recompilador de SwitchGamesRecomp**, no una función C++ escrita a mano simulando instrucciones. La entrada es un NSO **propio y diminuto**, con instrucciones AArch64 reales `mov w0,#42; ret`, secciones NSO y un símbolo ELF64 propio. El generador está en `recomp_aot/generate_sample_nso.py`; no hay recursos, firmas ni código de Zelda.

En GitHub Actions se obtiene el recompilador [SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp) de código abierto **MIT**, fijado a SHA `7c2531d9bdce94506bad9bf53d8a9848b240f111`. Se generan `funcs.h` y `code_000.cpp`, se ejecuta el C++ traducido como test de host y se incorpora al APK arm64-v8a. El botón **«Ejecutar código ARM64 RECOMPILADO REAL (muestra propia)»** llama a esa función y debe devolver el resultado 42.

La app incluye los avisos de licencia y autoría de SwitchGamesRecomp. Los archivos generados no se incorporan al repositorio. **Es la primera prueba del método de recompilación estática real que queremos adaptar de GTA IV a Switch**, aunque todavía no procesa BOTW ni ejecuta sus recursos. El port completo requiere código original legítimo, grandes adaptaciones a Horizon, NVN y compilación de shaders.
