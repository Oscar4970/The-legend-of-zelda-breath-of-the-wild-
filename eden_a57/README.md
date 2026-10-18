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


## v4 — estabilidad y FPS reales antes que interpolación (18 octubre 2026)

Informe del usuario: BOTW Switch llega a controlar a Link, aproximadamente
10 FPS, con resolución reducida y relojes emulados modificados. Se cierra el
juego y vuelve a Eden; todavía no tenemos `logcat` para saber por qué.

**Cambios activos solo en SM-A576***: primera apertura de v4 activa el
perfil de rendimiento una vez, incluso para usuarios de v1/v2/v3:
- Máximo 0.5x de resolución interna portátil (no aumenta 0.25x elegida por el usuario)
- NCE; GPU accuracy Low; CPU accuracy Auto
- Valores normales de reloj **emulado** CPU/GPU, sin max-clock forzado
- Vulkan, VRAM conservadora, memoria emulada 4 GiB, caché de shaders en disco
- Shaders/GPU asíncronos, filtro bilineal económico y sin frame generation
- Registra un único snapshot informativo de RAM libre al iniciar la app

El parche nativo v3 de un solo trabajador Vulkan en Xclipse 550 se mantiene.
El filtro **FSR 1** ya está en Eden y puede seleccionarse desde ajustes
(escalado FSR, valor 6). No es FSR 3 ni AFMF, no genera fotogramas, y
consume recursos adicionales respecto al filtro bilineal. Los algoritmos
`frame_gen` de Eden no están validados en el Galaxy A57. Con unos 10 FPS
reales la interpolación 2x no haría que la lógica del juego corriera a 20 FPS.

No se ha comprobado que esta versión mejore FPS ni resuelva el cierre.
Se requiere prueba A/B frente a v3, sin overclock ni cambios múltiples.
Si la GPU muestra defectos con GPU accuracy Low, cambiar High manualmente.
Los controles modificados son internos de Eden; no se toca Samsung PayJoy,
Android, firmware, kernels ni controladores propietarios.


## v5 — rollback de la regresión en carga de partidas (2026-10-18)

La v4 regresó de "Link camina unos segundos, ~10 FPS" a "no carga
la partida". Fue incorrecto modificar de una sola vez precisión y relojes
emulados, junto con otros ajustes, antes de tener un registro del cierre.

- Para instalaciones que ya ejecutaron v4, v5 **restaura exactamente una
  vez** los valores guardados de CPU accuracy, GPU accuracy, reloj CPU
  emulado, reloj GPU emulado y "force max clock". Esto permite volver a
  comparar con el estado previo de la v4. Los valores de relojes son
  **del emulador**, no overclock del SoC físico Samsung.
- No eleva ni fuerza la resolución: v4 no guardó su valor anterior y
  aumentarla sin preguntar podría interferir con las pruebas.
- Para nuevas instalaciones, utiliza la configuración inicial v3/v2.
- Conserva el parche Vulkan de v3 y el control de memoria de v2.
- No toca partidas, cachés, controladores, firmware, ni Android.
- No incorpora FSR 3 ni activa generación de fotogramas.
- Esta compilación **NO demuestra** mejora de FPS o corrección del cierre.
  Hace falta log real al fallar y test A/B en el dispositivo.

Nota: el registro `eden_log.txt.old.txt` recibido anteriormente terminaba
a 1.5 segundos de inicio y no incluía el fallo, por lo que no sustenta una
hipótesis precisa sobre la causa raíz del cierre del juego.
