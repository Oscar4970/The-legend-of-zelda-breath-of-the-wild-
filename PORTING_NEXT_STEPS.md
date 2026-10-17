# Próxima etapa: analizar módulos auténticos de Switch (solo lectura)

La prueba **v0.8** confirmó llamadas y referencias ARM64 únicamente en un programa ficticio integrado en el APK. No procesa todavía las reubicaciones ni las dependencias reales de un NSO comercial.

## Hitos comprobables

1. Localizar el encabezado ModuleHeaderLocation y validar la firma MOD0 de un NSO0 sin cifrar.
2. Leer el desplazamiento de `.dynamic` y comprobar que cae dentro de una sección reconstruida.
3. Enumerar entradas ELF64 Dynamic de manera acotada, sin ejecutar el archivo.
4. Interpretar las tablas RELA/REL y registrar tipos AArch64 (RELATIVE, GLOB_DAT, JUMP_SLOT), rechazando estructuras malformadas.
5. Inventariar símbolos y dependencias externas, indicando qué servicios no existen en Android.
6. Ejecutar pruebas sintéticas de límites, truncamiento, hashes y fuzzing.

### Obstáculos aún sin resolver

Una futura ejecución real de BOTW requeriría archivos del juego obtenidos legítimamente, un cargador robusto, enlaces e importaciones reales, servicios de Nintendo Switch, la adaptación de NVN a la API gráfica de Android, memoria, hilos, audio, entrada, sistema de archivos y depuración del motor.

**No hay todavía un port ni garantías de conseguirlo.** Una función ARM64 inventada no constituye una prueba de que el juego original pueda arrancar.

Documentación técnica pública:
- https://www.switchbrew.org/wiki/MOD
- https://switchbrew.org/wiki/NSO
