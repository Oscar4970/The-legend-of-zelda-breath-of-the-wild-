# Auditoría técnica: SuperMarioWonderRecomp como base de BOTW Android

Revisión: 17 de octubre de 2026. Basada en código y documentación públicos del repositorio [BelmanteGu/SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp). **No es un port jugable de BOTW, ni contiene assets/código del juego.**

## 1. Diagnóstico

El proyecto de Wonder prueba que una recompilación estática **AArch64 de Nintendo Switch → C++ portable → librería nativa Android** es viable para *un juego específico*. Su versión Android declara funcionar en Galaxy S23, con Vulkan 1.3, controles, audio, present y pipeline cache. En su README dice que se recompilan ~17.8 millones de instrucciones, ~250 mil funciones y ~1.4 GB de C++ generado **del propio Wonder**. Son cifras del proyecto Wonder, **no de Zelda**. El renderer adapta la API gráfica NVN a Vulkan y traduce shaders Maxwell a SPIR-V de forma anticipada. Los detalles de cada motor son diferentes.

El código propio del repositorio principal es **MIT**, pero sus integraciones (traductor y bibliotecas) tienen otras licencias que deben revisarse individualmente. Reutilizar los conceptos y componentes abiertos podría reducir muchísimo la ingeniería requerida; copiar los binarios generados de Wonder o su dump de ROM no ayudaría y no sería apropiado.

## 2. Qué podemos reutilizar y qué no

| Componente | Reutilización realista | Falta para BOTW |
| --- | --- | --- |
| AArch64→C++ (`recomp/`) | Investigar y adaptar frontend de recompiler y generación de C++ | Soporte exacto de instrucciones y patrones de BOTW; descubrimiento de funciones; símbolos; jump tables y TLS |
| Runtime `nx` / Horizon HLE | Analizar protocolos SVC, IPC, handles, hilos y memoria | Servicios nuevos que requiera BOTW, sincronización y ABI de sus módulos |
| Memoria del huésped | Estrategia de regiones virtuales Android y reubicaciones | Direcciones del BOTW real, módulos y robustez en Android/ASLR |
| NVN→Vulkan | Reutilizar conceptos y, tras auditoría legal, componentes portables | Capacidad NVN completa para patrones distintos de Wonder, estado gráfico, buffers, shaders y precisión |
| Shader translator | Analizar pipeline Maxwell→SPIR-V y artefactos de caché | Extraer y recompilar **shaders de BOTW**, validar SPIR-V y sincronización |
| Android y controles | Actividad, superficie, controles, audio, logs, cachés | Conectar el motor concreto, UI estable, compilación y distribución privada |
| Renderizador Vulkan en Samsung Xclipse 550 | Probar capacidades anunciadas y creación de dispositivo | Comprobar compilación SPIR-V real, errores de drivers, rendimiento y estabilidad |

**Conclusión central:** la arquitectura Wonder es una base de investigación útil, pero **no un motor listo para cargar cualquier ROM**. La recompilación es específica de ejecutable/versión, y las llamadas NVN/Horizon pueden diferir.

## 3. Hallazgos del código de Wonder

- `runtime/src/nvn/nvn_vk.cpp` solicita Vulkan **1.3** y utiliza `VkPhysicalDeviceVulkan13Features`, con la característica `dynamicRendering` en el recorrido normal.
- `docs/WONDER-ANDROID-OPTIONS.md` aclara que usa barreras clásicas y semáforos binarios; `synchronization2` y semáforos de línea temporal **no son requisitos absolutos** del código examinado.
- ASTC puede decodificarse a RGBA8 si el formato nativo falta; el código contempla alternativas para otras funciones opcionales. **No informar falsos bloqueos** porque falte un formato/extension opcional.
- Subgrupos de 32 pueden ser seleccionables dependiendo del driver y `subgroupSizeControl`; si no lo son, el runtime estudia soluciones alternativas, cuyo resultado visual debe validarse.
- El renderizador se basa en buffers de subida, shaders traducidos, descompresión de texturas y reejecución de órdenes NVN. Saber que Vulkan existe no valida todos estos pasos.
- El README declara, como ejemplo, 1.1 GB residente en una prueba de Wonder en Galaxy S23, mientras los escenarios Mac ocupan 3.5–4.2 GB: son métricas de **diferentes plataformas, escenas y definiciones de memoria**; no predicen la RAM necesaria para BOTW.
- Su Android usa controles táctiles, superficie y cache de pipeline; puede orientar un APK nativo, pero **no hemos integrado esas implementaciones**.

## 4. Hipótesis de hardware del Galaxy SM-A576B

Las pruebas anteriores de nuestra app detectaron **GPU Samsung Xclipse 550, Vulkan 1.4.304 y ARM64**. Que el número de versión sea 1.4 cumple el mínimo 1.3 del renderer Wonder, **pero** hay que consultar la implementación real de `dynamicRendering`, formatos y extensiones y probar `vkCreateDevice`. El nombre Xclipse no garantiza ni impide soporte de sus shaders.

**El nuevo botón de v0.15**:
1. Consulta el verdadero controlador Vulkan del dispositivo (sin Internet).
2. Comprueba Vulkan ≥1.3, colas gráficas, `dynamicRendering`.
3. Intenta crear un dispositivo lógico temporal usando esa característica, luego lo destruye.
4. Informa ASTC, BC1, D24/S8, extensiones opcionales, subgrupos y memoria reportada.
5. Presenta si se cumple el **mínimo del renderizador de referencia**, dejando explícito que eso NO equivale a BOTW compatible.

No usa `vkCreateSwapchainKHR`, no compila SPIR-V, no renderiza un frame NVN y no mide FPS de un juego. Esos son **hitos distintos**.

## 5. Plan para un port ejecutable real (criterios de aceptación)

**A. GPU:** validar el hardware de Xclipse con la v0.15. Corregir los fallos de driver de forma limitada y reproducible, no alterando Windows/Android ni deshabilitando la seguridad.

**B. Recompiler:** aislar e integrar legalmente el frontend MIT en un ejecutable **propio/homebrew** ARM64 y compilar su salida nativa ARM64 Android. Demostrar que una función y un programa de prueba originales, no solo sintéticos precargados, ejecutan el mismo resultado. Este paso ya es diferente de nuestros lectores NSO/NRO.

**C. Horizon:** ampliar soporte SVC/IPC con tests unitarios y una ejecución de homebrew propia, sin privilegios adicionales, con rastros de fallos y terminación segura.

**D. GPU NVN:** crear comandos mínimos desde el runtime abierto, aplicar en Android y verificar un frame real Vulkan. Validar primero con un programa libre; sin usar shaders propietarios para demostrar la API.

**E. BOTW:** únicamente tras los hitos A–D y con una copia propia legal del juego, investigar módulos, versionado, recursos, servicios y traducción de shaders específicos. Construcción privada por usuario, sin redistribuir código recompilado propietario.

### No interpretar erróneamente

- El test de 769 reubicaciones RELR y la auditoría de SVC prueban parsing, **no que el módulo esté ejecutándose**.
- BOTW no es Wonder: los motores y recursos varían, aunque ambos usen Switch.
- Recompilar ARM64 a ARM64 no elimina la necesidad de Horizon/NVN ni asegura ganancia de rendimiento.
- No estimar FPS, fechas o probabilidad de éxito sin una ejecución real de gameplay y una batería de mediciones.

## Fuentes directas

- https://github.com/BelmanteGu/SuperMarioWonderRecomp
- https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/main/LICENSE
- https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/main/runtime/src/nvn/nvn_vk.cpp
- https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/main/docs/WONDER-ANDROID-OPTIONS.md
- https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/main/docs/nvn/ANDROID-PORT.md
- https://github.com/BelmanteGu/SuperMarioWonderRecomp/blob/main/docs/FRAMEWORK.md
