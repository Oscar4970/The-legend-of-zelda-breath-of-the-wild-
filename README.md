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
