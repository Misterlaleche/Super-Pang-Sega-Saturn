# Super Pang para Sega Saturn

[English](README.md) | **Español**

Port no oficial de **Super Pang**, el arcade de Mitchell de 1990, para Sega Saturn. La versión de referencia es **SuperPang 1.4** y ya funciona en una Saturn real. El proyecto sigue en desarrollo: quedan diferencias que revisar, situaciones de juego que probar y trabajo de rendimiento y compatibilidad.

El objetivo es conservar el comportamiento del arcade y adaptarlo a una presentación de **320 × 240**, pensada para jugar en CRT. Este repositorio reúne el código del port, la copia de Jo Engine utilizada, las herramientas de construcción y las notas técnicas. También explica los límites de la versión actual y qué hace falta para compilar una imagen propia.

## Estado de la versión 1.4

| Apartado | Situación actual |
| --- | --- |
| Consola | Funcionamiento confirmado en Saturn real; el alcance de la prueba está recogido en [Validación](docs/VALIDACION_1.4.md). |
| Vídeo | NTSC, 320 × 240, progresivo. |
| Juego | Tour con las 40 fases y su final implementados; modo Panic, opciones, pausa, demostraciones y partidas de uno o dos jugadores. Su presencia en el código no equivale a una validación exhaustiva de cada recorrido. |
| Sonido | Música preparada en PCM y efectos reproducidos por el SCSP. Todo se lee de la pista de datos. |
| Guardado | Récords y ajustes en la memoria interna de Saturn. |
| Expansión de RAM | Desactivada en esta construcción. No necesita el cartucho de 4 MB. |
| SDK | Jo Engine 2026.3, con modificaciones locales. |
| Compilador | GCC 13.2.0 para SH-2, big endian, con optimización durante el enlace. |

La [lista de pendientes](docs/ESTADO.md) distingue las funciones implementadas de las comprobadas. No se presenta esta versión como una reproducción perfecta ni como un port terminado.

## Cómo se ha llevado a Saturn

El punto de partida es **Super Pang (World 900914)**. Su placa pertenece a la familia Mitchell: utiliza un procesador compatible con Z80, con protección Kabuki, gráficos por caracteres y objetos, y sonido YM2413/OKIM6295. La identificación y el mapa del hardware se pueden consultar en el [driver de MAME](https://github.com/mamedev/mame/blob/master/src/mame/capcom/mitchell.cpp).

El trabajo del port consiste en estudiar esas rutinas y sus datos, trasladar su comportamiento a C y preparar una presentación que encaje en Saturn. En ejecución se emplean rutinas nativas compiladas para SH-2. Se conservan tablas, estados y disposiciones de datos del original donde ayudan a mantener su comportamiento; no hay un intérprete Z80 ejecutando la ROM completa.

La versión 1.4 reparte el trabajo así:

- **SH-2 principal:** lógica, controles, colisiones, preparación de gráficos, acceso al CD y servicio del audio. El segundo SH-2 no está habilitado en esta construcción.
- **VDP1:** personajes, bolas, arpones, objetos y otros elementos móviles, mediante texturas y listas de comandos.
- **VDP2:** composición final y fondos/HUD sobre un bitmap NBG1 con dos páginas. La CPU prepara los cambios de ese bitmap.
- **SCSP:** reproducción de las muestras y de los búferes de música. Las músicas están preparadas de antemano; el juego no sintetiza el YM2413 en tiempo real.

La adaptación gráfica combina composición de celdas, conversión de recursos y ajustes de presentación. El campo horizontal del arcade se lleva de 384 a 320 píxeles; el HUD, los menús y ciertos elementos reciben un tratamiento específico para conservar su legibilidad. Los datos de juego y las coordenadas de pantalla se tratan por separado.

La explicación completa, con enlaces a las funciones y módulos, está en [Arquitectura](docs/ARQUITECTURA.md).

## Dónde está la dificultad

Saturn tiene varios procesadores y memorias independientes. Ese reparto obliga a decidir qué datos están residentes, qué debe descomprimirse, cuándo se puede cambiar de página y cuánto tiempo queda para atender el CD. La capacidad libre de una memoria no resuelve una falta de espacio en otra.

También hay que conservar el orden de las operaciones del juego: un cambio pequeño en el movimiento de una bola, una colisión, un contador o la secuencia de aparición de objetos puede alterar una partida. El reloj lógico utiliza un acumulador de 2871/3000 por VBlank nominal de 60 Hz; la cadencia de la lógica y la presentación de vídeo se gestionan por separado. Tener una salida de 60 Hz no demuestra que todas las escenas mantengan esa fluidez.

La música añade otra exigencia: los búferes deben recibir datos a tiempo mientras se dibuja y se actualiza la partida. Por eso se han preparado efectos residentes y prefijos de música, y se han acotado las operaciones que pueden bloquear durante el juego.

Por último, la consola real sigue siendo la referencia. La revisión 1.4 incorpora una coordinación entre las lecturas periódicas de mandos y la inicialización de la memoria de guardado. Fue una respuesta a un bloqueo de arranque que las comprobaciones anteriores no habían descartado. La confirmación en Saturn es un avance; todavía hace falta ampliar las pruebas de recorridos, mandos, guardado y sesiones largas.

## Compilar una copia propia

**El repositorio no incluye los recursos del juego, una ROM arcade, una BIOS de Saturn ni una imagen BIN/CUE.** Algunos datos también estaban incrustados en archivos C; se han tratado como recursos externos y no se publican aquí.

La ruta disponible permite reconstruir la 1.4 con los recursos preparados de un paquete local `SuperPang_1.4_FUENTES.zip`, obtenido y utilizado con los permisos correspondientes. Se verifican sus archivos antes de incorporarlos al árbol de trabajo. **La regeneración completa a partir de `spang.zip` sigue pendiente**: varios conversores conservan dependencias de etapas anteriores que no están en esta entrega. Si solo tienes la ROM arcade, consulta primero [Recursos y procedencia](docs/RECURSOS.md).

En Linux, con Python 3.11 o posterior, Make, un compilador C del sistema, xorriso y la toolchain SH-2 indicada:

```sh
git clone https://github.com/Misterlaleche/Super-Pang-Sega-Saturn.git
cd Super-Pang-Sega-Saturn

python3 tools/import_resources.py /ruta/SuperPang_1.4_FUENTES.zip
python3 tools/import_resources.py --check

export SATURN_TOOLCHAIN_ROOT=/ruta/sh2eb-elf
python3 tools/build.py
python3 tools/verify_disc.py
```

El resultado es `game.bin` junto a `game.cue`. La [guía de compilación](docs/COMPILAR.md) detalla las dependencias, las comprobaciones y la preparación para emulador, MODE o CD-R. La presencia de un archivo entre las dependencias no concede permiso para redistribuirlo.

## Contenido del repositorio

| Ruta | Contenido |
| --- | --- |
| [`main.c`](main.c) | Inicio, bucle principal y coordinación entre juego, vídeo y audio. |
| [`stage/`](stage/) | Lógica del juego, fases, bolas, armas, objetos, fauna y modos. |
| [`frontend/`](frontend/) | Título, menús, opciones, récords y ajustes. |
| [`renderer.c`](renderer.c), [`render_common.c`](render_common.c) | Presentación de sprites, escenarios, texto y conversiones de coordenadas. |
| [`audio/`](audio/) | Reproducción, cachés y herramientas históricas de preparación de audio. |
| [`jo_engine/`](jo_engine/) | Copia del motor usada por la 1.4, con sus modificaciones y avisos. |
| [`Compiler/COMMON/`](Compiler/COMMON/) | Enlace, cabecera del disco y utilidades de creación de imágenes. |
| [`art/`](art/) | Conversores y código de descompresión; los píxeles del juego se aportan localmente. |
| [`resources/`](resources/) | Inventario de dependencias y huellas de la versión de referencia. |
| [`tools/`](tools/) | Importación local, compilación y verificación. |
| [`docs/`](docs/) | Arquitectura, estado, construcción, procedencia y validación. |

Los conversores históricos se conservan para documentar el proceso. Sus dependencias están descritas en [Recursos](docs/RECURSOS.md); el comando de construcción soportado es `python3 tools/build.py`.

## Documentación

| Guía | Español | English |
| --- | --- | --- |
| Requisitos e instrucciones de compilación | [Compilar](docs/COMPILAR.md) | [Building](docs/BUILDING.md) |
| Hardware e implementación | [Arquitectura](docs/ARQUITECTURA.md) | [Architecture](docs/ARCHITECTURE.md) |
| Funciones, limitaciones y trabajo pendiente | [Estado](docs/ESTADO.md) | [Status](docs/STATUS.md) |
| Recursos locales y procedencia | [Recursos](docs/RECURSOS.md) | [Resources](docs/RESOURCES.md) |
| Pruebas y evidencias de la versión 1.4 | [Validación](docs/VALIDACION_1.4.md) | [Validation](docs/VALIDATION_1.4.md) |
| Derechos, licencias y créditos | [Derechos y créditos](docs/DERECHOS_Y_CREDITOS.md) | [Rights and credits](docs/RIGHTS_AND_CREDITS.md) |
| Colaboraciones e informes en consola | [Colaborar](CONTRIBUTING.es.md) | [Contributing](CONTRIBUTING.md) |

## Colaborar y comunicar fallos

Se aceptan contribuciones e informes de fallos en español o en inglés.

Para un fallo en consola, ayuda indicar la versión, el modelo de Saturn, el método de carga, los mandos conectados y los pasos para reproducirlo. Una fase concreta y una secuencia breve de acciones suelen aportar más que una descripción general de que el juego va lento. Los detalles están en [CONTRIBUTING.es.md](CONTRIBUTING.es.md).

## Derechos y créditos

Super Pang Saturn es un proyecto no oficial y sin afiliación con Mitchell, Capcom o SEGA. Los nombres, gráficos, música y demás materiales originales pertenecen a sus respectivos titulares. Jo Engine y el resto de componentes de terceros conservan sus propios avisos y condiciones; este repositorio no aplica la licencia del motor al juego original.

Consulta [Derechos y créditos](docs/DERECHOS_Y_CREDITOS.md) antes de reutilizar código o distribuir archivos. La licencia general de las aportaciones propias del port todavía no se ha establecido.
