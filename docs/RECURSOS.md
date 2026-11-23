# Recursos y procedencia

[English](RESOURCES.md) | **Español**

## Qué contiene una construcción de la 1.4

El ejecutable se construye con código C del port y de Jo Engine. Parte de los datos del juego se incorpora al ejecutable y otra parte se carga desde el CD. Por tanto, separar los archivos `.bin` y `.wav` no basta para preparar el árbol público: también hay tablas y píxeles en fuentes C y cabeceras generadas.

El inventario preciso está en [`resources/manifest.json`](../resources/manifest.json). Cada dependencia tiene ruta, tamaño y SHA-256. El importador solo copia las entradas de ese inventario desde un paquete local; no descarga archivos ni extrae todo el ZIP.

| Grupo | Dependencias externas |
| --- | --- |
| Inicio de Saturn | `Compiler/COMMON/IP.BIN`, la plantilla que utiliza `ip.S`. No es una BIOS. |
| Datos del juego | Tablas de fases, estados, presentación y colisiones; datos del frontend; catálogo de audio; cabeceras de gráficos. Se conservan localmente con los nombres que espera el código. |
| Gráficos de CD | `AUTH4.BIN`, `CELLH4.BIN`, `CELLT4.BIN`, `CHARDF.BIN`, `CHARMDF.BIN`, `VIEWX.BIN` y `VIEWY.BIN`, dentro de `cd/ART`. |
| Audio de CD | Los 53 archivos de `cd/AUDIO` referenciados por el catálogo de la 1.4. |
| Cabeceras de compilación | Las 127 cabeceras del árbol `Compiler/WINDOWS/sh-elf/include`. Aunque la ruta diga WINDOWS, este makefile también las usa al compilar en Linux. Conservan sus propios avisos. |

Los siete archivos gráficos de ejecución ocupan aproximadamente 602 KB. El audio preparado ocupa unos 196 MB decimales: es la parte principal del tamaño del disco. La imagen utiliza una única pista de datos; esos archivos de música no son pistas CDDA.

El importador deja las dependencias fuera de Git mediante `.gitignore`. Las fuentes públicas conservadas del paquete se enumeran por separado en [`public-source-reference.json`](../resources/public-source-reference.json).

## Ruta que se puede reproducir

La ruta de construcción publicada es:

1. Disponer del código de este repositorio y de la toolchain indicada.
2. Aportar localmente los recursos preparados de `SuperPang_1.4_FUENTES.zip`, con los permisos necesarios para utilizarlos.
3. Comprobar las huellas, compilar y verificar la imagen resultante.

El importador también admite el directorio `native320_r09` extraído de ese paquete. El nombre es heredado; su makefile identifica la versión 1.4.

El repositorio no ofrece una descarga de ese paquete ni de sus recursos. Si falta una dependencia, la compilación debe detenerse y decir cuál falta. No se sustituyen gráficos, tablas o sonidos por archivos vacíos para producir una imagen aparentemente válida.

## Regeneración desde el arcade: trabajo pendiente

La cadena histórica de preparación todavía necesita reunirse antes de ofrecer una construcción completa que parta de una ROM arcade aportada por el usuario. Estos son los límites encontrados en el paquete 1.4:

| Herramienta conservada | Dependencia histórica |
| --- | --- |
| [`stage/prepare_data.py`](../stage/prepare_data.py) | Referencias a `c13_reference/fuentes`, módulos de extracción, datos decodificados y capturas de atributos, caracteres y paletas. |
| [`art/build_art.py`](../art/build_art.py) y las revisiones posteriores | Material preparado en `art320/target_v001` y otros datos de autoría y revisión de etapas anteriores. |
| [`audio/build_assets.py`](../audio/build_assets.py) y los exportadores de audio | Recursos de una entrega C13 anterior, catálogos y manifiestos de aquella etapa. |
| [`art/frontend/megadrive_03/build_title.py`](../art/frontend/megadrive_03/build_title.py) | Una captura de las letras PANG de Mega Pang v0.3 beta y metadatos del título. Esa captura no procede de `spang.zip`. |

Se conserva el código de estos conversores porque explica decisiones del port: composición de celdas, empaquetado de gráficos, preparación de muestras y adaptación del título. **No forman una receta autónoma que pueda ejecutarse de principio a fin con los archivos públicos actuales.** No deben utilizarse para regenerar a ciegas el árbol de la 1.4: algunos scripts también escriben código y recursos.

Para cerrar esta parte faltan recuperar o reemplazar las dependencias de preparación, fijar sus versiones y permisos, y verificar una extracción completa desde entradas documentadas. El resultado deberá compararse con las huellas de la 1.4. Hasta entonces, la guía se limita a la reconstrucción con recursos preparados.

## Propiedad de los materiales

El inventario describe necesidades técnicas y procedencia. No es una licencia de los recursos. La conversión de una imagen, una muestra de sonido o una tabla a otro formato no elimina los derechos que puedan existir sobre el material de partida. Consulta [Derechos y créditos](DERECHOS_Y_CREDITOS.md).
