# BOTW → Android: sistema de archivos virtual NSP para el runtime

**Estado: interfaz de lectura de contenedor lista para pruebas; NO ejecuta el juego.**

Esta etapa es distinta del inventario de metadatos: \`nspvfs::Mount\`
ofrece una interfaz C++ reutilizable para que un futuro runtime pueda
\`exists(nombre)\`, \`sizeOf(nombre)\` y
\`read(nombre, offset, buffer, bytes)\` sobre las entradas del NSP seleccionado.
No copia los 13.8 GiB del archivo ni exporta NCA, y preserva el descriptor
abierto aunque el selector de Android cierre su referencia original.

## Funcionamiento

1. Android solicita mediante \`ACTION_OPEN_DOCUMENT\` un NSP local.
2. \`nativeMountNspVfs\` valida PFS0, crea un \`dup\` del descriptor,
   comprueba límites y conserva el montaje mientras la app está activa.
3. C++ permite lecturas aleatorias estrictamente acotadas a cualquier
   entrada del archivo. Solo requiere que el proveedor Android soporte
   acceso aleatorio.
4. \`nativeProbeNspVfs\` demuestra tres lecturas por separado
   (principio/mitad/final del NCA de mayor tamaño), sin imprimir bytes.
5. \`nativeUnmountNspVfs\` cierra el descriptor cuando se desmonta.
6. \`nspvfs::currentMount()\` es el punto de integración C++ para la
   futura capa de descifrado y los sistemas ExeFS/RomFS.

No abre archivos externos indicados por el contenido del NSP. Sus nombres
son virtuales y las lecturas se rechazan cuando quedan fuera de los
rangos declarados. La aplicación no guarda rutas, firmware ni claves.

Las pruebas sintéticas usan un NSP *sparse* de **14 GiB virtuales**;
su lectura no requiere ocupar 14 GiB reales. Verifican acceso aleatorio
más allá de 4 GiB, retención de descriptores, rechazo de rutas no
reconocidas, offsets fuera de límites y nombres duplicados. Los archivos
de juegos reales no se suben a GitHub Actions.

## Bloqueadores para ejecutar Zelda

**Estos archivos virtuales son los NCA originales cifrados.** Todavía falta
la capa que interprete/desencripte *secciones* NCA (no solo sus cabeceras),
monte PartitionFS ExeFS y RomFS sobre ellas y exponga \`main\`/\`sdk\` al
proceso de recompilación AOT. También faltan Horizon IPC/SVC, NVN → Vulkan,
audio, memoria, entrada y multitud de dependencias.

El firmware ZIP se usa como fuente local de metadatos y estudio de
compatibilidad, pero **no se ejecuta como sistema operativo** dentro
de Android y no se incluye en el APK.
