# Decisión técnica: adoptar una arquitectura tipo GTA IV/Winlator para BOTW de Switch en Android

Fecha: 2026-10-17
Estado: propuesta de arquitectura; **NO implementada ni jugable**.

## Qué significa "el método GTA IV"

Cuando GTA IV se ejecuta mediante **Winlator** en Android, la idea no es un port de código fuente del juego:
- Un APK contiene un **entorno de compatibilidad**.
- Wine implementa interfaces de Windows; Box86/Box64 ayudan a ejecutar el código x86/x64 en Android ARM.
- DXVK y otros componentes traducen la API gráfica del juego.
- El usuario aporta legalmente los archivos originales, y el sistema de compatibilidad trata de ejecutar ese programa.

Fuentes: https://github.com/brunodev85/winlator-app/ y https://www.winlator.org/

No está verificado que el GTA IV concreto observado por el usuario se ejecute precisamente así: podría ser emulación, streaming u otro mecanismo. Para determinar el caso exacto hace falta su nombre/enlace/archivo de origen.

## Adaptación correspondiente: BOTW (Switch) -> Android

| Capa | Análogo Windows/Winlator | Equivalente Switch |
| --- | --- | --- |
| APK/interfaz | Lanzador Android, controles y archivos | Lanzador Android, selector de contenido legal, controles, registro y telemetría |
| Código de CPU | Box64 (traducción x86-64 a ARM64) | AArch64 directo **solo cuando seguro y técnicamente viable**; emulación/JIT para las instrucciones y patrones que no pueden ejecutarse directamente |
| Entorno de SO | Wine / interfaces Win32 | Horizon ABI, kernel SVC, handles, memoria, scheduler, eventos, hilos, archivos, IPC y servicios |
| Formato binario | PE/Windows | NRO/NSO y otros contenedores relevantes, MOD0, símbolos, REL/RELA/RELR, TLS, dependencias |
| Gráficos | DirectX -> DXVK -> Vulkan | NVN/Maxwell -> traducción gráfica -> Vulkan; shaders, sincronización, cachés |
| Audio/entrada | API Windows / XInput | Servicios de audio y HID de Switch -> APIs Android |
| Compatibilidad | Versiones de Windows, DLL | Versiones y servicios Horizon, módulos del juego y recursos originales |

**Compartir ARM64 no equivale a poder ejecutar BOTW como aplicación Android.**
La ABI, las llamadas SVC, el espacio virtual, la seguridad del host y NVN son distintos.

## Aprovechamiento de código abierto

- **Skyline** investigó ejecución de Switch para Android ARMv8, HLE y gráficos. Su proyecto oficial fue descontinuado y retiró el código de ramas controladas por los autores. Los mirrors requieren revisión individual de origen, estado, dependencias y cumplimiento de licencia MPL-2.0; no asumir seguridad ni compatibilidad.
  - Histórico oficial: https://github.com/skyline-emu/skyline
- Proyectos activos o forks de la familia de emulación Switch para Android pueden servir para **evaluación comparativa**. No integrar nada sin analizar seguridad, licencias, mantenimiento y coste de mantenimiento.
- **Winlator** es una referencia para arquitectura de compatibilidad y lanzamiento, **no un cargador de Switch**. No copiar Box64, Wine o DXVK al proyecto sin una necesidad comprobable; no resuelven Horizon/NVN.

## Plan de aceptación, no de numeración de APK

1. Conservar los lectores actuales como una utilidad diagnóstica, con fuzzing y análisis de límites.
2. Comparar contra una base de ejecución Switch ARM64 de código abierto y auditable: licencias, pruebas, servicios, GPU, logs, build Android.
3. Crear una **muestra homebrew propia mínima y compilada desde fuente**, con una interfaz de terminación/resultados controlada; validar que el ejecutable de Switch original corre en un entorno aislado Android.
4. Instrumentar la primera llamada no soportada (Horizon SVC / IPC) y resolverla en un emulador de servicios. Nada de desactivar seguridad de Android ni ejecutar binarios externos no confiables sin aislamiento adecuado.
5. Medir compatibilidad gráfica y estabilidad en el Samsung SM-A576B (Xclipse 550) usando un renderer Vulkan real; no inferir rendimiento de BOTW a partir de demos Vulkan.
6. Solo después evaluar un dump propio y legítimo de BOTW; sin ROMs, keys, firmware ni contenido del juego dentro del repositorio.

### Obstáculos, riesgos y límites

- Nuestro APK v0.14 aún no implementa un runtime Horizon y **NO arranca un homebrew real**.
- Un ejecutable de Switch, aun si es ARM64, puede fallar con SVC/IPC/TLS/memoria/scheduler/GPU incompatibles.
- La simulación RELR de 769 direcciones en una copia **no es ejecución**.
- Un juego comercial grande requiere compatibilidad Horizon + NVN/Maxwell -> Vulkan. El coste técnico es comparable al de un proyecto de emulación sustancial, no a convertir unas ROMs en APK.
- Respetar las licencias de los proyectos reutilizados y utilizar solo archivos del juego obtenidos legalmente.

## Próxima decisión

No seguir multiplicando APK que solo cuentan metadatos. Priorizar un **spike de runtime homebrew sin GPU** con criterios de éxito: ejecución controlada, resultado reproducible, diagnósticos de SVC e IPC y aislamiento. Si una plataforma open-source existente ya ofrece esa base, evaluar derivar el proyecto de ella en vez de rehacer todo el sistema.
