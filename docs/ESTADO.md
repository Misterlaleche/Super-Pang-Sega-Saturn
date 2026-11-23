# Estado de Super Pang Saturn 1.4

[English](STATUS.md) | **Español**

**Super Pang Saturn sigue en desarrollo. La versión 1.4 funciona en una Saturn física, según la confirmación actual recibida para esta entrega.** Esa confirmación se registra con alcance general. Las pruebas específicas que acompañaban al paquete se describen por separado, porque proceden de análisis del código, pruebas en el equipo de desarrollo y sesiones de Mednafen.

El objetivo de este documento es distinguir lo que el código implementa, lo que cambia en 1.4 y lo que permiten afirmar las comprobaciones disponibles. La explicación del funcionamiento interno está en [ARQUITECTURA.md](ARQUITECTURA.md).

## Alcance implementado

| Área | Lo que incluye el código de 1.4 | Referencia |
| --- | --- | --- |
| Tour | Las 40 etapas, sus cambios de escenario, mapas, bonificaciones y secuencia final. La progresión entra en el final después de la etapa de índice 39. | [`stage.c`](../stage/stage.c), [`spang_round_flow.c`](../stage/spang_round_flow.c) |
| Panic | Programación de globos, progresión hasta el nivel 100, cambios de fondo, cierre de la partida y entrada en la secuencia final. | [`spang_panic.c`](../stage/spang_panic.c), [`stage.c`](../stage/stage.c) |
| Jugadores | Inicio con uno o dos jugadores, estado y controles independientes, puntuaciones y gestión de finalización por jugador. La entrada durante una partida se rige por el modo y el estado de juego. | [`stage.h`](../stage/stage.h), [`main.c`](../main.c) |
| Presentación | Título, selección de modo, selección de etapa, pantallas de récords y entrada de nombre. | [`frontend.h`](../frontend/frontend.h), [`frontend.c`](../frontend/frontend.c) |
| Demostraciones | Dos demostraciones de Tour, dos secuencias de tutorial y una demostración de Panic, con entradas preparadas a partir de las secuencias de referencia. | [`stage.h`](../stage/stage.h), [`spang_attract_input.c`](../stage/spang_attract_input.c) |
| Opciones | Dificultad, vidas, sonido de demostraciones, salida mono, niveles de música y efectos, y centrado de pantalla. | [`settings.c`](../frontend/settings.c) |
| Guardado | Récords y ajustes en la memoria interna, con dos archivos alternos y verificación de la escritura. | [`savegame.c`](../savegame.c), [`save_codec.c`](../frontend/save_codec.c) |
| Control de sesión | Pausa, regreso al título y salida a la BIOS mediante el gesto de botones definido por la aplicación. | [`main.c`](../main.c) |

La presencia de estas rutas es comprobable en el código. La aceptación completa de una partida, su fidelidad en cada situación y su comportamiento sostenido en distintos equipos requieren evidencias de ejecución con ese alcance. Por eso la tabla describe implementación y no sustituye a un registro de pruebas de todas las etapas y combinaciones.

## Qué cambia en 1.4

La entrega conserva la base de 1.3 y concentra su modificación funcional en la coordinación entre el guardado de la BIOS y el SMPC. La comparación archivada entre ambas versiones identificó cambios de producción en `jo_engine/backup.c`, `jo_engine/internal/smpc.h`, `jo_engine/smpc.c` y el `makefile`. Los 60 archivos de los directorios `ART` y `AUDIO` conservaban su contenido y sus posiciones en el disco respecto a la entrega comparada de 1.3.

El problema investigado era un bloqueo durante la carga en una Saturn con MODE. Las observaciones previas situaban una espera en la zona de inicialización del controlador de guardado y reactivación de RESET. La hipótesis de trabajo era un posible solapamiento con las peticiones periódicas `INTBACK` utilizadas para leer los mandos.

La corrección de [`backup.c`](../jo_engine/backup.c) pausa el inicio de nuevas peticiones `INTBACK` antes de esa inicialización y lo reanuda después de restaurar RESET. Una petición ya iniciada conserva su camino normal de finalización por interrupción. El cambio se aplica también cuando el controlador de guardado vuelve a inicializarse para escribir.

La confirmación actual de funcionamiento de 1.4 en Saturn física actualiza el estado de la entrega. Por sí sola no identifica de forma definitiva la causa de los bloqueos anteriores ni convierte cada operación de guardado o cada combinación de periféricos en una prueba documentada. La explicación del cambio se mantiene en el nivel que respaldan el código y las observaciones disponibles.

## Cómo leer las comprobaciones

El [informe de validación de 1.4](VALIDACION_1.4.md) separa la evidencia que acompañaba al paquete de las comprobaciones de reproducción realizadas para este repositorio. Cada tipo de prueba responde a preguntas distintas:

| Evidencia | Alcance |
| --- | --- |
| Confirmación actual en Saturn física | Funcionamiento general de la versión 1.4 en una consola real. |
| Compilación y examen del disco | Construcción del ejecutable, estructura de la imagen y correspondencia de los archivos entregados. |
| Análisis estático y modelo del asignador | Límites de secciones, reservas y secuencias de asignación representadas en el modelo. |
| Pruebas de lógica en el equipo de desarrollo | Comportamiento de las funciones y rutas que ejecuta cada prueba. |
| Registros simulados del SMPC y almacenamiento simulado | Orden de operaciones, exclusión temporal de nuevas peticiones y recuperación de datos en los escenarios modelados. |
| Mednafen | Comportamiento observado al ejecutar la imagen en ese emulador y en los estados inspeccionados. |

Las pruebas archivadas de la candidata 1.4 incluían compilación sin avisos, examen de **96.395 sectores sin sectores defectuosos**, secuencias de asignación sin fallos en el modelo, **360.000 actualizaciones modeladas de Panic**, pruebas de guardado con corrupción y memoria no disponible, y **21 retornos de Options desde centrado** con componentes de vídeo simulados. Son resultados de las pruebas descritas en el paquete de referencia; no se presentan como una sesión equivalente en hardware.

La sesión de Mednafen documentada utilizó la región americana y observó el título, entrada y avance en Panic, regreso al título y salida a la BIOS mediante `A+B+C+START`. Las capturas que acompañaban a esa evidencia son de Mednafen. La ausencia de fallos en los indicadores de los estados inspeccionados delimita esos estados, sin demostrar la totalidad de una partida prolongada.

Las notas antiguas que decían que 1.4 todavía no se había probado en Saturn reflejaban el momento de preparación de la candidata. La confirmación física recibida después es el estado actual. Del mismo modo, los informes de revisiones R10/R13/R16 conservados en el archivo de trabajo pertenecen a sus revisiones; no deben usarse como descripción automática de 1.4.

## Límites y trabajo pendiente

El port tiene una presentación adaptada a 320 × 240. Combina compresión horizontal de la geometría del mundo, mapas a escala 1:1 y composición específica del HUD y de los sprites. La fidelidad visual debe evaluarse teniendo en cuenta esas decisiones, explicadas en [Arquitectura](ARQUITECTURA.md).

La configuración actual usa el SH-2 maestro y la RAM interna. Las capacidades opcionales de Jo para el segundo SH-2, el cartucho de RAM o un controlador de sonido en el 68000 no describen funciones activadas por esta versión. La música se reproduce desde archivos PCM preparados, con lectura progresiva desde la pista de datos.

Los presupuestos de memoria son ajustados y dependen del catálogo. El informe de referencia calcula **2.276 bytes** de margen en el área temporal de sonido después del mayor efecto no residente. Ese margen es un cálculo del conjunto auditado. Cualquier ampliación de recursos debe volver a contrastarse con el catálogo y con la distribución del ejecutable. La reserva de pila y el modelo del asignador tampoco constituyen una medida del peor caso de toda una sesión física.

Las siguientes ampliaciones de evidencia permitirían concretar mejor el estado del proyecto:

- Registrar la consola, región de vídeo, medio de carga, mandos y duración de las sesiones físicas de 1.4.
- Documentar recorridos completos de Tour y Panic, incluidos dos jugadores, continuaciones, finales y entrada de récords.
- Comprobar la persistencia de ajustes y récords tras reiniciar y volver a cargar la versión.
- Mantener la validación de memoria, audio y sincronización de escena junto a los cambios que puedan afectar a sus presupuestos.

Estos puntos delimitan el seguimiento del trabajo en curso. La referencia para afirmar que una comprobación concreta está realizada será su registro en [VALIDACION_1.4.md](VALIDACION_1.4.md).
