# Ruta rápida: primera ejecución de BOTW en Android (sin fingir avances)

Actualizado: 2026-10-17. **Objetivo**: ejecutar código auténtico de The Legend of Zelda: Breath of the Wild (Switch) en Android; a largo plazo, lograr el port nativo recompilado y jugable.

**Estado real**: la v0.17 es una prueba gráfica de Vulkan; no carga, recompila ni ejecuta ningún componente de BOTW. No debe contarse como prueba del juego. Este documento sustituye la práctica de aumentar números de versión por cada diagnóstico gráfico.

## Referencias verificadas

1. [SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp): su frontend AArch64→C++ y su runtime Horizon/NVN→Vulkan son referentes de ejecución nativa. A diferencia de BOTW, Wonder cuenta con adaptación **específica de juego**; no es un APK que abra cualquier ROM.
2. [zeldaret/botw](https://github.com/zeldaret/botw): decompilación experimental del ejecutable **Switch v1.5.0**, sin recursos ni RomFS, y el repositorio advierte que no puede utilizarse para jugar. Tiene información de funciones útil como referencia; verificar licencia y permisos antes de reutilizar archivos.
3. [Eden](https://github.com/eden-emulator/mirror): alternativa de emulación de Switch con NCE Android, **distinta del port recompilado nativo** y bajo GPLv3. Posibilita un intento de arranque con copia legítima mucho antes que una recompilación completa, pero el desempeño con Samsung Xclipse 550 es incierto y no valida nuestro APK.

## Cambio de enfoque (prioridades innegociables)

**No construir más APKs de diagnósticos sintéticos hasta que haya un hito de ejecución externa reproducible**, salvo que un fallo concreto del driver/runtimes lo requiera.

### Hito A: ejecución de código AArch64 externo de origen abierto

- Auditar de forma específica el `recomp/` de Wonder y su licencia MIT, licencias de terceras dependencias y construcción Android. No pegar código de terceros sin revisión.
- Crear un programa **homebrew propio** mínimo desde fuente y obtener su NRO/NSO usando herramientas abiertas.
- Ejecutar el recompiler AArch64 sobre *el mismo ejecutable homebrew*, producir C++ traducido y compilarlo a biblioteca ARM64 Android; debe poder ejecutar el punto de entrada y devolver una señal verificable de finalización.
- Validar ABI (X0, X1), stack, TLS, manejo de SVC/IPC y seguridad del host. No ejecutar código externo arbitrario con privilegios de la aplicación.
- **Éxito** = log reproducible que pruebe que instrucciones originales recompiladas se ejecutaron y completaron la tarea; no basta con leer un archivo o una función sintética ya compilada.

### Hito B: evaluación específica BOTW con entrada legítima y privada

**Entrada bloqueante**: hasta ahora el proyecto no tiene el ejecutable real ni los recursos de Zelda. Para trabajo con la versión Switch, necesitaríamos de una copia propia obtenida legalmente la estructura de ejecutables (por ejemplo `exefs/main`), su versión, los módulos correspondientes y, más adelante, sus recursos. **No subir ROMs, archivos de código comercial, keys ni firmware a este repositorio público ni al CI.** Procesar datos privados de forma local, no publicarlos en Actions.

- Empezar por inventario reproducible del ejecutable original que el propietario posea; comparar con información documentada de Switch v1.5.0 en zeldaret sin asumir versiones equivalentes.
- Evaluar el rango de instrucciones, TLS, tablas de saltos, funciones, importaciones y librerías y el tamaño de la salida C++.
- Recompilar solo una ruta mínima necesaria para demostrar inicialización controlada. **No** afirmar que un juego grande puede ejecutarse solo porque el recompiler genera C++.
- Implementar Horizon/IPC **en base a errores del ejecutable** con pruebas. La meta inicial es llegar al primer código del juego y registrar el primer fallo real.
- Después llegar a inicio de vídeo, primer menú, frame dibujado mediante NVN y controles; esto puede requerir muchísimo desarrollo adicional.

### Hito C: gráficos y estabilidad en el Samsung

La prueba v0.16 pasó en el SM-A576B: Xclipse 550, Vulkan 1.4.304, dynamicRendering, readback RGBA correcto. Esto **no predice FPS** ni confirma el pipeline NVN. BC1 no tiene soporte nativo, pero ASTC sí; D24/S8 falta, pero D32/S8, D32 y D16 están disponibles. Probar rutas de compatibilidad Vulkan **cuando las requiera el runtime BOTW**, sin seguir creando demos no relacionadas.

### Ruta alternativa para ARRANCAR EL ORIGINAL pronto, sin prometer port

Una app de emulación legalmente obtenida (por ejemplo un build del proyecto Eden desde su canal oficial), con archivos propios y sin terceros sospechosos, permite intentar ejecutar el juego de Switch antes de terminar una recompilación. Esto **no cumple** la meta de port nativo; el usuario decide si quiere hacer esa prueba. No redistribuir dumps ni claves. Compatibilidad y rendimiento de Xclipse aún no confirmados.

## Bloqueo actual y próxima decisión

- El primer bloqueador del **port auténtico** es hacer que un recompiler produzca código nativo Android **de un programa externo**, con un runtime básico; la v0.17 todavía no hace eso.
- El primer bloqueador de la **prueba con Zelda original** es disponer legalmente de la copia y ejecutables auténticos, manejados **solo en el dispositivo/PC del propietario**.
- Necesitamos confirmar **si el usuario posee BOTW para Switch o Wii U** y si puede obtener los archivos originales desde su propia copia. Sin ello no se puede probar ni verificar ejecución de BOTW; se puede seguir con el homebrew abierto.

Los hitos son condiciones de éxito técnico, no porcentajes ni plazos. Los fallos deben registrarse sin inferir compatibilidad de BOTW de una prueba de shaders o lectura de archivos.
