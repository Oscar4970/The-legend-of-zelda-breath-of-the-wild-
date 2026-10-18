# Prueba de compatibilidad y rendimiento de BOTW — Galaxy A57 / Xclipse 550

**Propósito:** comprobar por primera vez si Breath of the Wild (versión Switch) **arranca** con Eden en el Galaxy A57; solo después, medir rendimiento. No hay resultados de BOTW todavía. El código de este repositorio no contiene la ROM, firmware ni claves de Nintendo.

## Fuente segura del emulador y versión de comparación

- Eden upstream: https://git.eden-emu.dev/eden-emu/eden/releases (usar **Standard APK**, no **Genshin Spoof** ni **Legacy** como base del Galaxy A57).
- Variación experimental Galaxy A57: compilar el código en `.github/workflows/eden-a57-build.yml` desde el mirror de Eden fijado a `de7e6e01d43e8cf2ec193d74fa053b0d7949955f`, aplicar `eden_a57/apply_to_eden.py` y usar el artefacto si su build termina con éxito.
- **No asumir que los dos builds son equivalentes:** la revisión fijada puede diferir de la estable oficial; variaciones de FPS pueden provenir de otros cambios de Eden. Para A/B estricto compilar versión de referencia y parcheada desde el *mismo commit*, con mismas opciones.
- Si el resultado personalizado falla, usar solo el Standard APK de la página de versiones oficial y anotar la versión probada.
- Ambas variantes requieren archivos de un juego legítimamente adquirido y sus prerrequisitos legales; **no subir archivos de Nintendo al repositorio**, ni compartir ROM o claves como captura.

## Ajustes iniciales sugeridos (no benchmarks)

| Ajuste | Valor de arranque | Motivo y reservas |
| --- | --- | --- |
| CPU backend | **NCE** (ARM64), si está disponible | Reutiliza arquitectura AArch64 y reduce parte del costo de traducción; no garantiza estabilidad |
| Gráficos | **Vulkan**, driver stock Samsung | Confirmado Vulkan 1.4.304 en el móvil; no usar Turnip/Adreno |
| Modo | **Portátil** | Resolución nativa de Switch portátil menor que dock |
| Escala de resolución | **0.75×** (nominal 540p en modo portátil) | Reducir carga de píxeles; no implica automáticamente que FPS suban |
| Filtro | **Bilineal** | Evita filtros de escalado caros para el primer ensayo |
| Antialiasing | **Ninguno** | Evita overhead, se puede comparar después |
| GPU y shaders asíncronos | **Activados al inicio** | Para evitar algunos bloqueos por compilación; si hay artefactos, repetir con shaders asíncronos desactivados |
| Caché de shaders en disco | **Activada** | Limita la recompilación en usos posteriores |
| Generación de frames | **Desactivada** | Los FPS obtenidos deben ser frames realmente renderizados |
| Memoria VRAM | **Conservadora** | El Galaxy A57 tiene ~7.25 GiB de RAM reportada y la disponibilidad varía |
| Drivers personalizados | **Ninguno** | Xclipse no es GPU Adreno; los fallbacks de D24 pueden variar entre drivers |
| Velocidad emulada / limitador | **100 %**, no 200 % | Para no confundir exceso de velocidad con rendimiento |

**Precaución:** el parche Galaxy A57 establece varios ajustes en el *primer arranque* solo en SM-A576*. No certifica que todas esas opciones mejoren BOTW; mantiene un mecanismo para restaurar preferencias anteriores.

## Secuencia para capturar evidencia

1. Instalar el APK oficial o el experimental **si ya está compilado y verificado**. Permitir únicamente los accesos imprescindibles. No instalar builds de páginas dudosas.
2. Abrir Eden y anotar la versión y el dispositivo Vulkan que detecta. Si la ventana queda negra o hay error Vulkan, compartir solo el texto o captura (nunca claves).
3. Comprobar manualmente los ajustes de arriba; en Menú de rendimiento activar indicador **FPS/tiempo de frame** cuando el emulador lo permita. No confundir frecuencia de pantalla con FPS de render.
4. **Opcional, antes de Zelda:** usar solo homebrew autorizado y compatible para probar que el emulador carga una aplicación Switch. Un homebrew ligero no predice FPS de BOTW.
5. Cuando se disponga legalmente de BOTW Switch, importar el juego en el emulador **localmente**, sin subirlo al repositorio. Si se requiere actualización oficial, documentar edición/versión y usar solamente recursos propios.
6. Intentar arrancar y clasificar el punto máximo alcanzado: pantalla negra, logo, introducción, título, partida guardada, mundo abierto. Guardar captura/error/versión.
7. **Solo si hay gameplay:** recoger FPS reales, tiempo de frame, uso de memoria, temperatura de batería y tirones. Realizar 3 minutos de calentamiento de cachés y luego 5 minutos de recorrido comparable; probar después 15–30 minutos si funciona de manera estable.
8. Si hay fallos, desactivar **shaders asíncronos** y repetir sin cambiar simultáneamente resolución, driver o RAM. Comparar una variable cada vez.
9. No intentar desbloquear frecuencias ni usar software root/driver del sistema. Aislar errores del emulador, no tocar Samsung Pay, permisos privilegiados ni almacenamiento ajeno.

## Informe mínimo para compartir

- APK: nombre exacto + versión; APK oficial o parche Galaxy A57.
- Chip/driver: Xclipse 550 + Vulkan que muestre la pantalla.
- Emulador detecta archivos del juego: sí/no (no compartir los archivos).
- Arranque: **punto alcanzado** y captura del fallo si hay.
- Gameplay visible: sí/no.
- FPS reales mínimo/promedio/máximo (si se midieron), y tiempo de frame.
- Temperatura y RAM aproximada del proceso (si la app lo muestra).
- Ajustes de la prueba: 0.75×, NCE, Vulkan, portátil, asíncronos, cachés.
- ¿Se bloquea, cuelga, pierde imagen, se calienta o cierra por falta de memoria?

## Criterios de éxito y limitaciones

El benchmark de Zelda **no puede realizarse ahora** porque no hay una copia legítima disponible y **no se ha ejecutado BOTW** en el Galaxy A57. Compilar un APK de Eden solamente significa disponer de una plataforma de pruebas, no que BOTW cargue o mantenga 30 FPS.

Un APK adaptado solo puede considerarse mejor que Eden stock si los datos de comparación en el **mismo dispositivo y misma revisión del emulador** muestran menos fallos o mejores tiempos de frame sin regresiones. El perfil por sí solo no constituye una optimización demostrada.


## Prueba v4 tras el primer gameplay (~10 FPS)

1. Conservar copia de los archivos de guardado; no limpiar datos de Eden ni
   borrar cachés innecesariamente. No sobrescribir apps con otra firma sin
   haber respaldado los datos previamente.
2. Confirmar que los relojes emulados CPU y GPU están en **Normal** y que
   la resolución es 0.5x o 0.25x en portátil.
3. Mantener **FSR 1 y frame generation desactivados** mientras el juego no se
   sostenga al menos un intervalo medible; filtro bilineal para línea base.
4. Repetir desde el mismo punto (santuario inicial / tableta Sheikah),
   anotar FPS mínimo/medio, duración hasta cierre y si vuelve al menú.
5. Guardar registros de Eden y `adb logcat` si es posible. Buscar
   `VK_ERROR_DEVICE_LOST`, `OutOfMemoryError`, `SIGSEGV` o `lowmemorykiller`.
   No compartir rutas privadas, claves, ROM o firmware.
6. Solo con una base estable comparar FSR 1 y, por separado, la generación
   de fotogramas experimental. No confundir fotogramas interpolados con FPS
   reales de emulación.
