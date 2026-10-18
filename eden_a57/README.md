# Eden Xclipse 550 — capa de compatibilidad interna (prototipo, GPLv3)

**NO ES UN DRIVER DE GPU**. La GPU Samsung Xclipse 550 utiliza un driver Vulkan
del sistema Android. La aplicación no puede reemplazar su driver kernel/firmware
sin soporte específico del fabricante. Este trabajo se implementa *dentro de Eden*:
usa la API pública Vulkan, selecciona ajustes seguros para el Galaxy A57,
registra las capacidades reales y deja intactos firmware, kernel y drivers.

## Qué hace ahora

- Detección estricta de Galaxy A57 \`SM-A576*\`, fabricante Samsung. **No** se
  aplica automáticamente a otras variantes Exynos, Galaxy S22, Adreno, etc.
- En primera ejecución exclusivamente: CPU NCE; Vulkan; perfil portátil;
  resolución 0.75×; filtro bilineal; sin antialiasing ni interpolación de
  cuadros; VRAM conservadora; caché de shaders, shaders y GPU asíncronos.
- Preserva los valores anteriores, deja al usuario modificar ajustes después
  y contiene una función de restauración para futuros controles de UI.
- Instrumenta el constructor del dispositivo Vulkan para registrar la API, ID
  de driver, ASTC, D24S8 y posibilidad de blits D32S8 en Xclipse 550.
- No finge características Vulkan, no desactiva comprobaciones de seguridad, no
  instala Turnip (solo Adreno), no sobreacelera CPU/GPU y no toca Samsung Pay
  ni otros ajustes del sistema.
- La ruta de profundidad sin D24/S8 **ya** tiene alternativas en Eden.
  Crear la misma lógica de nuevo no es una mejora real.

Este prototipo **NO se ha compilado ni medido dentro de Eden en Android** y
**no añade FPS acreditables** todavía. Solo hemos confirmado en el Galaxy A57
la v0.18 de nuestro laboratorio de recompilación y el diagnóstico Vulkan,
que no mide BOTW ni el rendimiento de Eden.

## Uso para desarrolladores

Obtén una copia local del emulador desde
https://github.com/eden-emulator/mirror (revisión auditada
\`de7e6e01d43e8cf2ec193d74fa053b0d7949955f\`).

Desde la raíz del proyecto BOTW Android:

\`\`\`bash
python3 eden_a57/apply_to_eden.py /ruta/a/eden
cd /ruta/a/eden
git diff
\`\`\`

Compila el emulador desde \`src/android\` siguiendo su documentación,
con JDK 17+, Android NDK r28, Gradle, CMake y dependencias externas.
**No instalar builds arbitrarios ni blobs de drivers de terceros.**
El emulador sigue requiriendo tus propios datos de juego legalmente
obtenidos y el software de sistema correspondiente.

La modificación está aislada de nuestro proyecto v0.18. Para
crear un paquete distribuible será necesario completar la compilación
de Eden, verificar dependencias/licencias GPLv3 y publicar el código
fuente de la variante si se distribuye. No introducir archivos Nintendo.

## Plan de medición real y criterios de una futura mejora

1. Compilar Eden original y versión con compatibilidad Galaxy A57.
2. Probar primero una aplicación homebrew legal con las mismas condiciones,
   luego BOTW exclusivamente si se tienen sus datos legítimos.
3. Capturar driverID, Vulkan capabilities, fallos por VK_ERROR_DEVICE_LOST,
   memoria RAM, consumo térmico, velocidad promedio y p95 de tiempo de frame.
4. Desarrollar workarounds nativos para defectos concretos del driver y
   repetir la comparación A/B. Descartar cualquier workaround que empeore
   compatibilidad, imagen o latencia; no aplicar parches solo por 'ser Exynos'.

**No garantiza que BOTW arranque o alcance 30 FPS.** Un perfil inicial de
540p es un objetivo de ensayo, no un benchmark ni una promesa.

Referencia: Eden GPLv3 / código fuente GNU GPL.
Eden ya proporciona sustitutos de profundidad D24/S8
en \`src/video_core/vulkan_common/vulkan_device.cpp\`.


## v2 - Perfil de baja memoria (18 octubre 2026)

Se limita a 2 el numero de trabajadores de compilacion Vulkan mediante
`ANDROID_PIPELINE_WORKERS` y se fija 4 GiB de memoria emulada con
`MEMORY_LAYOUT`. No se modifican driver, kernel ni datos de juego.

Esta version migra el perfil anterior v1 (solo los dos ajustes nuevos), mantiene
la resolucion y preferencias personalizadas y guarda los dos valores anteriores
para restauracion. Menor concurrencia puede aumentar los tiempos de compilacion
inicial. No hay medicion de FPS ni prueba de ejecucion real certificada.


## v3 - Limite nativo de shaders en Xclipse 550

Eden ya trae 2 trabajadores por defecto en Android: esa parte de v2 era preventiva y por si sola normalmente no cambiaba el rendimiento. El parche nativo v3 establece **un solo trabajador de compilacion Vulkan** cuando el nombre real de la GPU contiene Xclipse 550. Otras GPU conservan el comportamiento original. Compilar mas lentamente puede reducir el pico de recursos, pero aun se necesita probar BOTW, Vulkan y RAM para verificar su estabilidad.
