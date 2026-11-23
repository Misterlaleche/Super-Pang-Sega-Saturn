# Arquitectura de Super Pang Saturn 1.4

[English](ARCHITECTURE.md) | **Español**

Super Pang Saturn ejecuta la lógica del juego como código C nativo para SH-2, con una copia modificada de **Jo Engine 2026.3** como base. La configuración de 1.4 utiliza el SH-2 maestro, presenta la imagen a **320 × 240** y reproduce música y efectos PCM mediante el SCSP. El reparto efectivo entre procesadores y chips se describe aquí a partir del código que compila esta versión.

El punto de partida arcade pertenece al hardware Mitchell: CPU de la familia Z80 con protección Kabuki y sonido YM2413/OKI M6295. La referencia externa para esta identificación es el [controlador Mitchell de MAME](https://github.com/mamedev/mame/blob/master/src/mame/capcom/mitchell.cpp). Las variantes bootleg descritas en ese mismo archivo tienen diferencias de hardware; sus configuraciones no deben trasladarse a este port.

## Lógica nativa y datos de referencia

Los módulos de [`stage/`](../stage) traducen comportamientos del juego a funciones C: movimiento, colisiones, bolas, armas, objetos, fauna, puntuación, progresión y finales. Mantienen muchas estructuras de bytes, tablas y estados de la referencia. Por eso aparecen direcciones como `E080`, `8000` o `D000` en comentarios, comprobaciones y valores de estado.

Esas direcciones identifican el origen o el significado de un dato. El ejecutable no contiene un bucle que lea y ejecute instrucciones Z80. Por ejemplo, [`spang_player_control.c`](../stage/spang_player_control.c) calcula movimiento y animaciones mediante operaciones C, y [`spang_panic.c`](../stage/spang_panic.c) despacha estados conocidos del programador de globos. Las escrituras de caracteres y atributos pasan por funciones que convierten las direcciones de referencia en índices de arrays.

[`stage.c`](../stage/stage.c) reúne estos módulos y mantiene el estado de la partida. Expone una vista de solo lectura para dibujar y una cola ordenada de peticiones de audio. [`main.c`](../main.c) coordina la entrada de los mandos, el propietario de la pantalla —partida o menús—, la preparación de recursos, el sonido y la presentación. [`frontend/`](../frontend) contiene el título, la selección de modo, las demostraciones, los récords y las opciones.

Las tablas y escenas generadas, como `stage/stage_data.c`, y los catálogos y recursos gráficos y sonoros se importan localmente desde el paquete de dependencias indicado en las instrucciones de compilación. Que esas tablas se enlacen con el programa no convierte su contenido en código ejecutado por una CPU emulada. Tampoco supone que cada ruta del juego haya sido demostrada equivalente a la placa original: el alcance de las comprobaciones está separado en [Validación de 1.4](VALIDACION_1.4.md).

## Reparto del trabajo en Saturn

| Componente | Uso en la configuración de 1.4 |
| --- | --- |
| SH-2 maestro | Ejecuta la aplicación, la simulación, la preparación de comandos gráficos, la gestión de archivos y el control del audio. |
| SH-2 esclavo | El módulo de doble CPU de Jo está desactivado en esta compilación. |
| VDP1 | Dibuja sprites y elementos gráficos mediante texturas de 4 bits y tablas de color. |
| VDP2 | Presenta el fondo y los textos en un bitmap NBG1 RGB555 y compone el resultado con la salida de VDP1. |
| SCSP | Reproduce las muestras PCM de música y efectos almacenadas en la RAM de sonido. |
| 68000 de sonido | Ejecuta un pequeño bucle de espera; el SH-2 programa directamente los slots del SCSP. |

Este reparto depende de opciones concretas. El [`makefile`](../makefile) activa `JO_MINIMAL=1` y habilita los módulos de archivos, audio y memoria de guardado. En el [makefile común de Jo](../Compiler/COMMON/jo_engine_makefile), el modo mínimo deja desactivado `JO_COMPILE_WITH_DUAL_CPU_MODULE` si el juego no lo habilita expresamente. El proyecto no lo habilita. La presencia de `jo_engine/slave.c` en el árbol corresponde a una capacidad del motor, no a trabajo que esta versión envíe al segundo SH-2.

Sucede lo mismo con el controlador opcional de sonido para 68000. `JO_COMPILE_WITH_AUDIO_DRIVER_MODULE` y el módulo de efectos del DSP de audio no están activados. El camino compilado de [`jo_engine/audio.c`](../jo_engine/audio.c) instala el bucle de espera del 68000 y controla el SCSP desde el SH-2.

## Imagen y adaptación a 320 × 240

El objetivo de vídeo del [`makefile`](../makefile) es NTSC, 320 × 240, con un intervalo de presentación de un VBlank. La aplicación desactiva el entrelazado en [`main.c`](../main.c). Esto describe la configuración del programa; no es una medición de una tasa constante de imágenes por segundo en todas las escenas.

### Fondo y textos en VDP2

[`renderer.c`](../renderer.c) reconstruye el fondo y el HUD a partir de caracteres, atributos y paletas. Escribe píxeles RGB555 en el bitmap de NBG1 y conserva información sobre los cambios para actualizar las zonas que lo necesitan. Aunque los datos de entrada tienen organización de tiles, la presentación de este camino utiliza un bitmap de VDP2.

Se reservan dos páginas de **512 × 256 × 2 bytes**: una ocupa los bancos A0/A1 y la otra B0/B1. Juntas ocupan los **512 KiB de VRAM de VDP2**. La ventana visible es de 320 × 240; el área restante se mantiene preparada para el ajuste de centrado. La reserva y el intercambio están implementados en [`jo_engine/vdp2.c`](../jo_engine/vdp2.c).

Los cambios completos de escena se dibujan en la página oculta. Al terminar, el renderer asocia su presentación con el identificador de la tabla de comandos VDP1 correspondiente. El intercambio se confirma durante VBlank cuando se presenta ese cuadro. También conserva un segundo juego de tablas de color para que la imagen anterior mantenga sus colores mientras se prepara la siguiente.

La simulación tiene en cuenta esa entrega: mientras una nueva escena está pendiente, no consume el tiempo que debe permanecer visible. El servicio de audio sigue atendiendo los VBlanks transcurridos. Así se coordinan cambios de fondo, sprites y duración de las pantallas sin reservar otro framebuffer en la RAM de trabajo.

### Sprites y geometría

VDP1 recibe texturas de 4 bits con CLUT de 16 entradas. Se combinan recursos preparados para la presentación a 320 píxeles con celdas originales compuestas y almacenadas en caché. Hay rutas específicas para elementos como jugadores, bolas, objetos, escaleras, mapas y título. Por tanto, el conjunto gráfico tiene más de un tratamiento.

La conversión horizontal está explícita en [`render_common.c`](../render_common.c): la posición del mundo usa una relación de **5/6**, que adapta los 384 píxeles de la referencia al ancho de 320. Los mapas emplean una ventana centrada de 320 píxeles con relación 1:1. El HUD redistribuye sus bloques y conserva los píxeles de sus caracteres; los paneles de dos jugadores tienen ajustes propios. Estas decisiones impiden describir toda la imagen como una copia píxel a píxel o como un escalado uniforme.

El presupuesto configurado permite **2.400 definiciones de sprite** y **512 comandos VDP1**. El renderer mantiene además una caché de **256 grupos compuestos**. Son límites internos distintos: 2.400 definiciones no significa 2.400 objetos dibujados simultáneamente.

## Reloj de juego y entrada

[`spang_frame_clock.h`](../stage/spang_frame_clock.h) acumula **2.871/3.000 avances lógicos por VBlank**, referidos a una presentación nominal de 60 Hz. La cadencia de referencia adoptada por el código es, por tanto, **57,42 actualizaciones lógicas por segundo nominal**. Cada llamada ejecuta como máximo ocho actualizaciones pendientes y conserva la fracción y la deuda restante.

La entrada distingue botones mantenidos y pulsaciones nuevas, y conserva estas últimas cuando un cuadro no produce actualización lógica. La pausa detiene el avance de la partida y mantiene las pulsaciones de salida separadas de las acciones que deben ejecutarse al reanudar. El control de esta coordinación y del regreso al título o a la BIOS está en [`main.c`](../main.c).

## Recursos y memoria

Los recursos gráficos principales se leen durante la inicialización. Después permanecen disponibles en RAM de trabajo o VRAM; algunas texturas derivadas se crean la primera vez que se necesitan y se conservan en caché. Parte de los patrones de origen —**160 KiB**— se guarda en VDP1 mediante diez texturas de almacenamiento que no se dibujan. Esto libera RAM de trabajo para el audio.

Los glifos se conservan comprimidos y se decodifican en bloques acotados, con ocho posiciones de trabajo reutilizadas. Las escenas enlazadas en el ejecutable se descomprimen en un buffer de **10.240 bytes**, definido en [`spang_scene_data.h`](../stage/spang_scene_data.h). El [cargador de escenas](../stage/spang_scene_data.c) aplica descompresión LZSS y diferencias respecto a una escena base. La actualización ordinaria del fondo no vuelve a leer sus gráficos desde el disco.

La aplicación añade el **MiB de Low Work RAM** al asignador de Jo. En High Work RAM utiliza la reserva de heap configurada y los huecos utilizables que deja el enlazado. El [script del enlazador](../Compiler/COMMON/jo.linker) protege una pila de **16 KiB** para el SH-2 maestro, entre `0x060FBC00` y `0x060FFC00`. El módulo de cartucho de RAM está desactivado; esta configuración utiliza la memoria interna de Saturn.

La [auditoría de memoria de la entrega 1.4](evidence/reference-memory.json) recoge, entre otros, estos presupuestos:

| Área | Valor del informe de referencia |
| --- | ---: |
| Reserva principal de heap en High RAM | 108.544 bytes |
| Zona utilizable anterior a `JO_WORK` | 11.312 bytes |
| Zona utilizable posterior a `JO_WORK` | 78.384 bytes |
| Capacidad de texturas VDP1 calculada | 451.832 bytes |
| Dos bitmaps de VDP2 | 524.288 bytes |
| PCM residente en RAM de sonido | 392.768 bytes |
| Anillos de música, con alineación | 65.568 bytes |
| Área disponible para PCM temporal | 61.856 bytes |

Estos valores proceden de la distribución del ejecutable, del catálogo y de un modelo del asignador. La capacidad de texturas no equivale a memoria libre durante una partida, y los huecos de heap no miden el margen de la pila. [`stack_usage.c`](../stack_usage.c) incorpora una medición conservadora desde la entrada de la aplicación; su comentario delimita expresamente el arranque previo y el peor caso que esa medición no cubre.

## Audio: archivos preparados y reproducción PCM

La música procede de la cadena de preparación del proyecto C13 y se entrega al runtime como WAV PCM mono de **16 bits a 44.100 Hz**, con puntos de bucle definidos. El port utiliza esas muestras ya preparadas; la reproducción no ejecuta un sintetizador YM2413 ni un secuenciador Z80 en tiempo real.

El catálogo de 1.4 contiene **26 archivos de música**, **19 efectos derivados de la ruta FM** y **8 muestras derivadas de OKI**. La implementación de [`audio/audio.c`](../audio/audio.c) conserva identificadores de petición, prioridades, volumen, fundidos, pausa y reanudación. Dispone de una voz lógica para los efectos FM y cuatro para las muestras OKI.

Los efectos se cargan al iniciar: algunos quedan residentes en la RAM de sonido y otros permanecen en RAM de trabajo hasta su transferencia al SCSP. Los efectos FM usan normalmente PCM mono de 16 bits a 22.050 Hz; `F00` tiene su formato específico de 8 bits a 44.100 Hz. Las muestras OKI preparadas se reproducen como PCM de 16 bits a 7.576 Hz. La decodificación de los formatos de origen pertenece a la preparación de recursos.

La música sí se lee progresivamente desde el disco. Jo mantiene un buffer de entrada de **64 KiB** en RAM de trabajo y reserva dos anillos de **16.384 muestras** en RAM de sonido. Una caché acotada de hasta cuatro comienzos de música se prepara en límites explícitos donde la reproducción está detenida, como un cambio de modo o de escena. Una petición ordinaria de sonido utiliza los recursos preparados; una ausencia de catálogo o de preparación se informa como fallo.

El disco generado contiene una pista de datos. La música y los efectos son archivos de esa pista; no hay pistas CD-DA. La lectura progresiva, la reposición de buffers y los diagnósticos de audio pertenecen al [módulo de audio de Jo](../jo_engine/audio.c), con adaptaciones del proyecto para sus bucles y comienzos preparados.

## Guardado y coordinación con el sistema

[`savegame.c`](../savegame.c) utiliza la memoria de guardado interna de Saturn mediante la BIOS. Mantiene dos archivos alternos, `SPANG_REC_A` y `SPANG_REC_B`, con generaciones y comprobación después de escribir. El contenido persistente reúne récords y ajustes; la partida en curso y los editores de nombre todavía activos permanecen en RAM.

La modificación característica de 1.4 afecta a esa inicialización: [`jo_engine/backup.c`](../jo_engine/backup.c) suspende el inicio de nuevas peticiones periódicas `INTBACK` mientras inicializa el controlador de guardado y restaura RESET. [`jo_engine/smpc.c`](../jo_engine/smpc.c) conserva la terminación normal de una petición anterior y reanuda después la lectura periódica. Los detalles de este cambio y el alcance de su comprobación están en [Estado de la versión](ESTADO.md).

## Compilación y límites de esta descripción

La base de Jo y sus modificaciones se identifican en [`JO_ENGINE_SOURCE_NOTICE.txt`](../JO_ENGINE_SOURCE_NOTICE.txt). La configuración utiliza C para SH-2 big-endian, GCC 13.2 y LTO. La entrada pública de construcción es [`tools/build.py`](../tools/build.py), una vez importadas sus dependencias locales.

Construir el árbol con recursos preparados y regenerar todos esos recursos desde sus fuentes son tareas distintas. Esta descripción documenta el runtime de 1.4 y sus presupuestos; el estado de implementación, las pruebas y el trabajo pendiente se mantienen en [ESTADO.md](ESTADO.md) y [VALIDACION_1.4.md](VALIDACION_1.4.md).
