# Derechos y créditos

Super Pang Saturn es un proyecto personal de afición y sigue en desarrollo.
Se comparte para enseñar el trabajo de adaptación a Saturn y documentar cómo
reconstruirlo localmente. No está afiliado a Mitchell, Capcom ni SEGA.

El juego de referencia es **Super Pang (World 900914)**, de Mitchell, publicado
en 1990. El nombre del juego, sus gráficos, música, sonidos y demás elementos
originales pertenecen a sus respectivos titulares. Este proyecto no reclama
derechos sobre ellos.

## El código de este proyecto

Para reproducir el comportamiento del arcade se ha estudiado el original y se
han traducido o reimplementado partes de su lógica en C. La adaptación a Saturn,
el renderizado, la integración de sonido y las herramientas de desarrollo se
explican en la documentación técnica.

Por ahora no se ha asignado una licencia general al código propio del port.
Que el código pueda consultarse públicamente no significa que todo el
repositorio tenga una licencia de código abierto. Las licencias de terceros
se aplican únicamente a los componentes que identifican; este documento no
cambia sus condiciones ni concede derechos sobre materiales ajenos.

## Archivos necesarios para compilar

Los archivos originales del juego y los recursos derivados de ellos se
preparan localmente. Lo mismo ocurre con la BIOS que pueda necesitar un
emulador y con la plantilla de arranque `IP.BIN`: no son materiales que este
proyecto pueda incluir bajo la licencia de Jo Engine.

Debes obtener los archivos externos necesarios de una fuente lícita y contar
con los permisos que correspondan para utilizarlos. Tener un ejemplar del
juego no equivale, por sí solo, a tener permiso para descargar cualquier copia
de Internet. Generar una imagen jugable tampoco concede permiso para
redistribuir los archivos originales o una imagen que los incorpore.

Las herramientas que estudian o convierten el arte original no incluyen esos
recursos. No añadas ROM, BIOS, `IP.BIN`, gráficos, música extraída ni imágenes
jugables a los commits o a los adjuntos de las incidencias.

## Jo Engine

El port utiliza **Jo Engine**, de **Johannes Fetz**, con cambios locales para
este proyecto. Su repositorio oficial declara la licencia MIT, y los archivos
fuente de la versión incorporada conservan también avisos con las condiciones
BSD de tres cláusulas.

Se mantienen ambos avisos, además de las cabeceras de los archivos:

- [JO_ENGINE_LICENSE.txt](../JO_ENGINE_LICENSE.txt): licencia MIT.
- [JO_ENGINE_SOURCE_NOTICE.txt](../JO_ENGINE_SOURCE_NOTICE.txt): aviso de los
  fuentes y notas sobre los cambios locales.

Estos avisos deben acompañar a las redistribuciones a las que se apliquen.
La atribución a Jo Engine no implica que su autor respalde este port.

## MAME y las herramientas de audio

**MAME** ha servido de referencia para estudiar el hardware del arcade,
observar su comportamiento y contrastar resultados. El driver de Mitchell
identifica el juego de referencia y sus variantes.

Las herramientas históricas de conversión incluyen el decodificador
[spang_oki_adpcm.c](../audio/vendor/spang_oki_adpcm.c) y
[su interfaz](../audio/vendor/spang_oki_adpcm.h). El algoritmo está adaptado de
`okiadpcm.cpp`, de **Andrew Gardner y Aaron Giles**, bajo licencia **BSD de
tres cláusulas**. Se conserva el aviso completo en
[MAME-OKI-BSD-3-Clause.txt](../audio/vendor/MAME-OKI-BSD-3-Clause.txt).

Ese aviso histórico también acredita a **Mirko Buffoni y Aaron Giles** por
`okim6295.cpp`, usado en el material de referencia. No atribuye a estos autores
los demás ayudantes C13 y Rice de `audio/vendor`, ni extiende automáticamente
la licencia BSD a todo lo que se encuentra en esa carpeta.

[PROVENANCE.json](../audio/vendor/PROVENANCE.json) conserva las rutas de origen
y los hashes de las copias incorporadas durante el desarrollo. Sus rutas
describen la organización de aquella versión del proyecto, no archivos que
deban descargarse ni rutas necesarias para la compilación actual.

La licencia de MAME no concede derechos sobre las ROM de los juegos. Los
avisos de su código y los derechos de los recursos del arcade son distintos.

## tinf: descompresión de gráficos

[glyph_inflate.h](../art/r10_r4/glyph_inflate.h) contiene una adaptación de
**tinf / tiny inflate**, de **Joergen Ibsen**, con copyright de 2003–2019 y
licencia **zlib**. El aviso completo está al comienzo de ese mismo archivo.

La cabecera identifica la revisión original y explica las modificaciones
para el port: descompresión DEFLATE sin envoltura, comprobaciones de límites
y ajustes en las tablas de búsqueda. Se conservan tanto el aviso original
como la identificación de esta versión modificada. La licencia exige
mantener el aviso y no atribuirse la autoría del original ni presentar una
versión modificada como si fuera la original.

## Sobre el carácter del proyecto

Que este trabajo sea gratuito y se haga por afición no sustituye los permisos
de los titulares. Los avisos anteriores describen la procedencia y los límites
de lo que se comparte; no convierten los materiales del arcade en contenido
libre.

Como referencia para lectores en España, la Ley de Propiedad Intelectual
excluye los programas de ordenador del límite general de copia privada
(artículo 31.3.c). Los artículos 99 y 100 regulan sus derechos y las excepciones
específicas, incluidas ciertas operaciones del usuario legítimo, la copia de
seguridad y la interoperabilidad, cada una con sus condiciones.

## Referencias

- [Jo Engine: repositorio oficial](https://github.com/johannes-fetz/joengine).
- [Jo Engine: documentación oficial](https://jo-engine.org/doxygen/).
- [Ejemplo de los avisos conservados en los fuentes de Jo Engine](https://jo-engine.org/doxygen/entity_8h_source.html).
- [MAME: driver de Mitchell](https://github.com/mamedev/mame/blob/master/src/mame/capcom/mitchell.cpp).
- [MAME: información de licencias](https://www.mamedev.org/legal.html).
- [tinf: repositorio de Joergen Ibsen](https://github.com/jibsen/tinf).
- [tinf: licencia zlib](https://github.com/jibsen/tinf/blob/master/LICENSE).
- [tinf: revisión de origen indicada en la adaptación](https://github.com/jibsen/tinf/blob/c9561a6c6950a9fa4cd59cce481661c1e4e2156e/src/tinflate.c).
- [Ley de Propiedad Intelectual: texto consolidado del BOE](https://www.boe.es/buscar/act.php?id=BOE-A-1996-8930).
