# Validación y reproducción de la versión 1.4

Fecha de esta revisión: **23 de noviembre de 2026**.

## Referencia probada en Saturn

La confirmación del responsable del proyecto es: **«SuperPang 1.4 ya funciona en Saturn»**. Se registra como confirmación de funcionamiento de la entrega original en una consola real. No se han añadido a esa declaración un modelo de consola, una duración de sesión ni una lista de fases o periféricos que no se hayan especificado.

La imagen original tiene esta huella:

```text
SuperPang 1.4.bin
SHA-256 60b99dffaaa95b1304bbb1c2c6b910ebc8b407c12eb7dbeb3b4778a112712d15
```

El repositorio conserva las identidades en [`resources/reference-release.json`](../resources/reference-release.json), sin incluir la imagen ni su ejecutable.

## Evidencia que acompañaba a la entrega

El paquete de evidencias se preparó antes de recibir la confirmación física. Por eso sus informes conservan `physical_saturn_tested: false`. Ese campo describe el alcance de aquellas pruebas; no se ha reescrito para presentar una prueba de ordenador como una prueba de consola.

Entre las comprobaciones de referencia figuran:

- Compilación de la candidata sin avisos en el entorno de origen.
- Revisión de 96.395 sectores de la imagen original, sin sectores defectuosos.
- Modelo del asignador sin fallos en las secuencias representadas, con los límites de memoria de la entrega.
- Pruebas modeladas de Panic, guardado y coordinación del SMPC.
- Sesiones de Mednafen con título, Panic, regreso al título y salida a la BIOS.

Se publican dos resúmenes originales que no contienen recursos del juego:

- [`reference-memory.json`](evidence/reference-memory.json): secciones, reservas y resultado del modelo de memoria.
- [`reference-production-comparison.json`](evidence/reference-production-comparison.json): archivos de producción que cambiaron entre 1.3 y 1.4.

Los registros completos, estados de emulador, imágenes y ejecutables del archivo de trabajo no forman parte de la publicación. El alcance de las pruebas históricas está desarrollado en [Estado](ESTADO.md).

## Comprobaciones de la publicación

Las fuentes seleccionadas se cotejaron con el paquete 1.4 mediante SHA-256. El código de juego, del motor y los límites del enlazador conservan el contenido de esa referencia. Las incorporaciones del repositorio son la documentación y las herramientas de preparación y verificación.

El importador validó los **199 archivos externos**, con un total de **198.677.280 bytes**. El conjunto publicado no incluye ninguno de esos archivos. También se revisaron las cabeceras y fuentes con datos incrustados, además de las extensiones de los archivos.

Las pruebas de las herramientas cubren importaciones incompletas o corruptas, conflictos con ediciones locales, rutas y enlaces simbólicos, repetición de la importación y rechazo de una toolchain sin LTO funcional antes de limpiar un ejecutable anterior. Los resultados de esta revisión se resumen en [`publication-checks.json`](evidence/publication-checks.json).

## Reconstrucción en Linux

La reconstrucción se completó con GCC 13.2.0 `sh2eb-elf`, un enlazador dinámico Binutils 2.40 y el plugin compartido de GCC 13.2.0 construido desde fuentes oficiales. La preparación está descrita en [Compilar](COMPILAR.md).

El paquete Linux inicial aceptaba `-flto`, pero no completaba la eliminación del código del motor que el juego no utilizaba. El enlace excedía el límite de memoria. Se corrigió el entorno de construcción conservando el compilador original y añadiendo el soporte de plugin. No se desplazó la pila ni se amplió el límite para hacer pasar la compilación.

La prueba previa verifica optimización entre dos archivos y eliminación de símbolos prescindibles. El enlace completo conserva la aserción de memoria: `__jo_work_end` queda en `0x060E89D0`, por debajo del comienzo de pila en `0x060FBC00`. La distancia de 78.384 bytes pertenece al reparto de memoria del programa; no es una medición de uso máximo de la pila.

| Comprobación de la imagen reconstruida | Resultado |
| --- | --- |
| Productos de compilación | ELF, mapa, ejecutable plano, ISO, BIN y CUE generados. |
| Archivos del sistema ISO | 66, coincidentes con los contenidos previstos; sin archivos ausentes, añadidos o con datos distintos. |
| Sectores de datos | 96.401; todos los bloques de 2048 bytes coinciden entre ISO y BIN. |
| Estructura MODE1/2352 | Cabeceras, EDC y paridad P/Q comprobados; 0 sectores defectuosos. |
| CUE | Una pista de datos MODE1/2352, con el nombre de archivo esperado. |

Esta compilación emitió **un aviso** de posible uso de `m` sin inicializar en `jo_engine/vdp2.c`, dentro de una función de matriz de rotación del motor. La publicación conserva el fuente original y registra el aviso; no se presenta la reconstrucción Linux como una compilación sin avisos. Esa función no aparece como símbolo en el ejecutable enlazado.

### Diferencia respecto a la entrega original

| Ejecutable plano | Tamaño | SHA-256 |
| --- | ---: | --- |
| Referencia de la entrega 1.4 | 554.960 bytes | `cce6b48eae25c755c238ded62dcbf02d17550737b718f30e6c1f67c1dba433b1` |
| Reconstrucción Linux | 554.576 bytes | `73e50c32b2d074f2e5434636631e69e429725efb3befd756638a224c45b9af26` |

El ejecutable Linux es 384 bytes menor. **No es idéntico byte por byte a la referencia.** Se han conservado las fuentes, pero no se ha aislado qué diferencia de configuración o generación de código explica esos bytes. Tampoco se atribuye la diferencia únicamente a las fechas del ISO, porque aquí se comparan los ejecutables planos.

La reconstrucción Linux no se ha ejecutado en una Saturn física ni se presenta como una nueva entrega validada en consola. La confirmación del primer apartado corresponde al BIN original. El objetivo de esta comprobación es demostrar que el árbol publicado, con sus dependencias locales y la toolchain documentada, construye una imagen íntegra. La aceptación funcional de una compilación nueva requiere sus propias pruebas.

## Qué queda por cerrar

Se mantiene pendiente la regeneración completa desde la ROM arcade y la documentación de recorridos y sesiones físicas con alcance específico. Cualquier ampliación de esa evidencia debe indicar la imagen probada, su huella, el equipo y el procedimiento. Los pendientes del juego están en [Estado](ESTADO.md); los de preparación de recursos, en [Recursos y procedencia](RECURSOS.md).
