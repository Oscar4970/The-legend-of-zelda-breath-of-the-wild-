# BOTW Switch → Android: port nativo recompilado

**Estado: objetivo BOTW de código abierto y herramienta privada de compilación creados. TODAVÍA NO JUGABLE.** El motor base procede de SwitchGamesRecomp (MIT), que reimplementa partes de Horizon y NVN/Vulkan. Su soporte existente de otros juegos no garantiza funcionamiento con Breath of the Wild.

El objetivo es traducir **los módulos auténticos** de BOTW de Switch (AArch64) a C++ y compilar una biblioteca de Android arm64-v8a. Necesita archivos propios y legítimos extraídos y descifrados: una carpeta ExeFS que contenga **main** y **sdk** (NSO0), y la carpeta RomFS con los recursos. Un archivo NSP o XCI directamente no sirve como NSO; no redistribuimos juegos, claves, actualizaciones ni firmware.

En Windows, desde el directorio raíz del proyecto:

    py -m pip install capstone==5.0.6 lz4==4.4.4
    py port_botw/build_port.py check --exefs "D:\BOTW\exefs" --romfs "D:\BOTW\romfs"
    py port_botw/build_port.py android --exefs "D:\BOTW\exefs" --romfs "D:\BOTW\romfs" --jobs 1

El segundo comando completo ya realiza la recompilación; no ejecutes una etapa prepare adicional para evitar trabajo duplicado. Necesita Python 3, Git, CMake >=3.25, Ninja y, para Android, SDK con NDK **28.2.13676358**. Con Ryzen 3 3200G y 8 GB de RAM, limita a un trabajo de compilación, pero es posible que ni así alcancen RAM, tiempo y almacenamiento.

El código del motor se obtiene a un commit fijo de SwitchGamesRecomp dentro del directorio ignorado privado/botw/engine. Allí se inserta el objetivo botw (program ID Switch 01007EF00011E000), se genera el C++ desde los NSO del usuario, se escriben imágenes de memoria privadas y se intenta compilar libbotw.so para Android. Todo permanece **local**. Ningún dato del juego se envía a GitHub Actions. El proyecto público solo contiene el código libre del objetivo y del constructor.

Si logra compilar la biblioteca, se coloca exclusivamente en app/src/main/jniLibs/arm64-v8a, que está ignorado por Git. Un Gradle local puede preparar un APK personal. La app solo habilita la acción de iniciar cuando el APK contiene esa biblioteca y hay datos propios en el directorio externo privado de la app: analysis/main.bin, analysis/sdk.bin, extract/romfs. **La compilación y el botón habilitado NO confirman que el juego arranque.** La biblioteca puede fallar por servicios Horizon, reubicaciones, SDK, memoria o funciones NVN faltantes.

**Obstáculos reales pendientes:** no hemos procesado todavía un ejecutable BOTW; no hemos adaptado su inicialización, sus importaciones ni la traducción de sus shaders Maxwell a SPIR-V. No existe cifra de FPS ni prueba de estabilidad en Zelda; nuestras pruebas Vulkan y ARM64 previas solo validan componentes.

No publiques archivos comerciales originales, C++ recompilado a partir de ellos, ni un APK personal que contenga código de Nintendo.