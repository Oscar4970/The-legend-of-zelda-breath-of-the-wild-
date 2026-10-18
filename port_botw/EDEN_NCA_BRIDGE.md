# Puente Eden → Port nativo BOTW: primer hito comprobable

**No es Zelda jugable, no es ExeFS extraído y no reemplaza Eden.**

El usuario ya ha verificado en su Galaxy A57 que el NSP de BOTW puede ser
inventariado sin mostrar claves. Para reducir incertidumbre antes de intentar
recompilar el juego, esta versión añade una **prueba local de cabecera NCA**.

1. La app permite seleccionar NSP con el selector seguro de Android.
2. Se abre un segundo selector para el archivo de claves **local del usuario**.
3. El código C++ reutiliza el analizador PFS0 que ya teníamos, leyendo
   un máximo de 32 cabeceras NCA de **solo 1 KiB cada una**.
4. Java busca la entrada \`header_key\` en un archivo de máximo 128 KiB,
   únicamente en memoria y sin enviarla a GitHub, a un servidor ni
   incluirla dentro del APK.
5. Se comprueba localmente el descifrado **AES-128-XTS**, con sector de
   512 bytes y la cabecera estándar NCA3/NCA2.
6. El informe solo muestra tipo del NCA, ID de título y número de NCA
   reconocidos. Destruye los buffers de la cabecera descifrada y el
   \`header_key\` transitorio de los arrays de trabajo.

**Limitaciones:** la prueba no autentifica que todas las claves sean correctas,
no descifra secciones del NCA, no obtiene ExeFS/RomFS y **no es un port
nativo**. Los archivos comerciales se procesan solo en el dispositivo, nunca
en GitHub Actions. Las pruebas automáticas utilizan datos NCA sintéticos
y una clave inventada conocida, además de comparar resultados XTS frente a
un vector generado de forma independiente con una implementación estándar.

**Seguridad:** no almacena ni comparte el contenido de \`prod.keys\`; el
reporte solo contiene metadatos no secretos. No incluir valores de claves,
ROM, contenido NCA, ExeFS ni firmware en GitHub o en informes públicos.
La implementación no intenta modificar o instalar controladores Samsung.

La ruta de port ya existente sigue siendo \`port_botw\` (AArch64 → C++,
servicios Horizon y NVN→Vulkan). Este puente es una prueba de
compatibilidad del contenedor, no una forma de saltarse las otras etapas.
