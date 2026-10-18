# Perfil Eden Vulkan / Galaxy A57 — aplicación en el emulador

## Qué es

Esta carpeta contiene una **modificación de espacio de usuario** para el emulador de Nintendo Switch [Eden](https://github.com/eden-emulator/mirror), GPLv3 o posterior, compatible con las licencias de Eden.

No es un driver gráfico de Samsung, **no sustituye** el kernel o los módulos propietarios del teléfono y no requiere root. El perfil no contiene ROMs, claves, firmware ni ejecutables comerciales. El código se aplica al **código fuente de Eden** y requiere compilar ese emulador por separado.

## Hardware verificado y selección

Solo se activa al detectar `Build.MANUFACTURER == samsung` y `Build.MODEL == SM-A576B`: el modelo usado en nuestras pruebas de Galaxy A57 con Xclipse 550. **No se ejecuta en otros Samsung**, ni modifica los ajustes de un Eden que ya tenga `config/config.ini`. Conserva la posibilidad de cambiar los parámetros después desde el menú de Eden.

La detección de modelo no sustituye a identificar el dispositivo Vulkan real. Nuestra prueba v0.15 detectó GPU Xclipse 550, Vulkan 1.4.304, ASTC nativo, D24/S8 no disponible y BC1 sin soporte nativo. Es necesario ensayar en el controlador de Samsung antes de implementar optimizaciones del backend Vulkan.

## Ajustes de primera instalación

| Ajuste interno Eden | Perfil | Motivo |
|---|---|---|
| CPU backend | `Nce (1)` | ARM64 nativo si el runtime lo admite; fallback a Dynarmic |
| Fastmem | `true` | Necesario para NCE cuando esté disponible |
| Renderer Vulkan | Valor por defecto | Mantener driver de Samsung del sistema |
| Resolución | `Res3_4X (2)` | 75 % de 720p en modo portátil ≈ **540p** |
| Modo Dock | `false` | Perfil portátil |
| VRAM | `Conservative (0)` | Evitar perfil de alto consumo de memoria |
| ASTC | `Gpu (1)` | Usar decodificación/aceleración nativa si es funcional |
| Cache shaders | `true` | Reducir recompilación durante sesiones posteriores |
| Async GPU | `true` | Mantener flujo asíncrono predeterminado |
| Async shaders | `false` | No omitir dibujos por compilación asíncrona en prueba inicial |
| Frame generation | `false` | Evitar carga extra y confundir FPS generados |
| Force max clock | `false` | No forzar GPU/CPU ni exigir root |

El perfil **no modifica velocidad de reloj, límites térmicos, sistema operativo ni drivers** y no promete FPS. Para BOTW se necesita una copia propia y firmware/archivos requeridos obtenidos legalmente. Los resultados dependen del estado real del emulador y de la GPU, no de nuestro laboratorio de Vulkan.

## Cómo integrarlo en una copia local autorizada de Eden

En tu PC con Python 3 y git:

```bash
git clone https://github.com/eden-emulator/mirror.git eden-xclipse
cd eden-xclipse
git checkout de7e6e01d43e8cf2ec193d74fa053b0d7949955f
cd ..
python3 ruta/a/tu_repo/emulador_xclipse550/apply_xclipse550_to_eden.py eden-xclipse
```

Después, compilar con el Android SDK/NDK documentado por Eden. **No tenemos todavía un APK personalizado compilado ni un resultado de BOTW en SM-A576B.** Este complemento requiere probarse en un checkout real y construir el emulador completo; no instalar solo el archivo Kotlin. Al publicar una variante, cumplir licencias GPL y avisos de copyright.

## Cómo saber si funciona y optimizar de verdad

La evaluación necesita al menos una sesión real de un programa homebrew (legal) y, más adelante, BOTW de copia propia: registrar FPS promedio/1% low, memoria real, estado NCE, errores Vulkan en Xclipse, consumo de batería y temperatura durante sesiones sostenidas. Ante fallos Vulkan, cambiar **una sola** opción cada vez y comparar. No instalar drivers Turnip de Adreno en la Xclipse 550.

## Próxima etapa de ingeniería

Una vez funcione esta integración, investigar en el renderer Vulkan de Eden:
- rutas explícitas para ausencia de BC1 y D24/S8, después de confirmar formatos que el juego/emulador solicitan realmente;
- creación y sincronización de pipelines con estadísticas de compilación de shaders;
- límites de almacenamiento de caché y sobreuso de VRAM;
- rutas Vulkan normales del sistema, sin modificación del controlador Samsung.

Estas mejoras dependerán de perfiles de ejecución reales y no deben inventarse como resultados confirmados.
