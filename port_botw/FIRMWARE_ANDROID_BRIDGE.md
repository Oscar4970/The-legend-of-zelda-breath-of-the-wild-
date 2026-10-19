# Firmware ZIP → puente de compatibilidad Android (v0.28)

**NO ejecuta Nintendo Switch, firmware ni BOTW. No es un port nativo jugable.**

Esta fase completa una **prueba de inventario del firmware** usando los mismos
principios del lector local NSP/NCA. El ZIP del firmware y el archivo
\`prod.keys\` se eligen **en el propio teléfono**, mediante Android Storage
Access Framework. La APK no incluye claves, firmware ni ROM.

- Lee el ZIP como flujo \`ZipInputStream\` (no crea copia del archivo).
- Lee un máximo de **3 KiB de cabecera cifrada** por archivo NCA, pero recorre
  el contenido del ZIP para alcanzar las siguientes entradas.
- Usa \`header_key\` del archivo local de claves de manera transitoria.
- Intenta reconocer NCA3, tipo y Title ID. Muestra solo estadísticas y una
  lista limitada a 12 metadatos.
- Rechaza ZIP con más de 512 entradas, archivos mayores a 256 MiB o más
  de 1 GiB de contenido descomprimido.
- No guarda, exporta, monta, instala ni intenta ejecutar ningún módulo NCA.
- Todas las pruebas de GitHub Actions utilizan firmware y claves **sintéticas**.

## Qué aporta al objetivo nativo

El inventario puede confirmar que el firmware es legible por nuestra capa
local de contenedores y que existen NCA de tipo PROGRAM/META. Esto permitiría
evaluar versiones y dependencias cuando estén disponibles los módulos
ExeFS/NSO auténticos de una copia legal del juego, y construir funciones
Horizon HLE con código abierto, usando Eden como referencia.

El informe por sí solo **no identifica APIs, servicios IPC ni símbolos
internos**, no verifica firmas de Nintendo y **no demuestra compatibilidad**.
Para ejecutar BOTW nativo todavía falta recompilar sus módulos y adaptar
NVN/Vulkan, HLE, memoria, hilos, entrada, audio y sistemas de archivos.

El ZIP y las claves del usuario no se incorporan al repositorio, artefactos
de CI ni ningún APK. No subir a GitHub el firmware o el código comercial.
