# Compilar Super Pang Saturn 1.4

[English](BUILDING.md) | **Español**

Este repositorio contiene el código del port, la versión de Jo Engine que utiliza
y las herramientas de compilación del proyecto. Los gráficos, el audio, los
bancos de datos del juego y la plantilla binaria de arranque se importan desde
una copia local de `SuperPang_1.4_FUENTES.zip` que ya tengas y puedas utilizar.
El repositorio no proporciona ese archivo ni descarga recursos del juego.

**La ruta disponible actualmente requiere el archivo privado de fuentes 1.4.**
Una copia de la ROM arcade `spang.zip` por sí sola no basta para reconstruir todos
los recursos de esta versión. El importador recupera los recursos ya preparados;
no los regenera desde la ROM.

## Requisitos

- Python 3.11 o posterior.
- Un compilador C del equipo, disponible como `cc`, además de `make` y `sh`.
- `xorriso`/`xorrisofs` o `mkisofs`, disponible en `PATH`.
- GCC **13.2.0** para el destino **`sh2eb-elf`**, con LTO y un plugin de enlace
  funcional, además de `sh2eb-elf-objcopy`, `sh2eb-elf-nm` y
  `sh2eb-elf-objdump`. El proyecto compila para SH-2 en big endian con `-m2 -mb`.
- Tu archivo local `SuperPang_1.4_FUENTES.zip`, o su directorio extraído
  `native320_r09`.
- Espacio para unos 199 MB de recursos locales, una copia temporal de los
  archivos que se importen y los productos de compilación, incluidos ISO y BIN.

El flujo de referencia usa Linux. No se ha validado aquí la compilación en macOS
ni en Windows. El ejecutable del compilador debe corresponder al sistema donde
lo ejecutes. La BIOS de Saturn y los emuladores no son requisitos para compilar;
pertenecen a la ejecución y las pruebas posteriores.

El Makefile heredado no admite espacios ni determinados caracteres especiales
en la ruta del repositorio o del toolchain. El lanzador comprueba estas rutas
antes de limpiar una compilación anterior. Usa letras ASCII, cifras, `/`, `_`,
`-`, `.` y `+`. La ruta del ZIP puede contener espacios si la pasas entre comillas.

### No basta con que GCC acepte `-flto`

El motor depende de que el enlace elimine código que el juego no utiliza. Un
paquete de GCC puede anunciar versión 13.2.0, aceptar `-flto` y realizar algunas
optimizaciones entre archivos, pero carecer del plugin que informa al compilador
sobre los símbolos que realmente necesita el ejecutable. Ese paquete no cumple
los requisitos de este proyecto: puede conservar código del motor hasta superar
el límite de memoria del enlazador.

`tools/build.py` compila y enlaza una prueba temporal de dos archivos C antes de
ejecutar `make clean`. Comprueba un ELF32 SuperH big endian, la conservación de
la entrada, la eliminación de dos funciones prescindibles y la resolución de
`scale(7)` a 42 entre ambos archivos. Acepta objetos LTO con código convencional
adicional, siempre que el enlace utilice realmente su representación LTO y
supere esas comprobaciones. La prueba no ejecuta instrucciones SH-2.

Para preparar tu propio toolchain, necesitas GCC 13.2.0 con `--enable-lto` y un
enlazador con soporte de plugins. El plugin de GCC debe estar construido para la
arquitectura del **equipo que ejecuta el compilador** y ser cargable por ese
enlazador. Desactivar indiscriminadamente las bibliotecas compartidas del equipo
puede dejar únicamente `liblto_plugin.a`, que no proporciona esta capacidad.
GCC documenta las opciones de configuración del plugin y la selección de un
enlazador compatible. Consulta las
[opciones LTO de GCC 13.2](https://gcc.gnu.org/onlinedocs/gcc-13.2.0/gcc/Optimize-Options.html)
y la [configuración oficial de GCC](https://gcc.gnu.org/install/configure.html).
Durante la preparación del repositorio se detectó precisamente un paquete Linux
13.2.0 sin ese soporte funcional. La comprobación previa lo rechaza. Se pudo
completar la construcción conservando el compilador y añadiendo un enlazador
dinámico y un plugin compatibles, sin modificar el código del juego ni sus
límites de memoria.

### Preparar el paquete Linux heredado

Si tienes el paquete **Yaul GCC 13.2.0 para Linux x86_64** con el enlazador
estático descrito, el repositorio incluye una herramienta para crear otra copia
con soporte LTO funcional:

```sh
bash tools/prepare_linux_toolchain.sh \
  /ruta/al/sh2eb-elf-original \
  "$PWD/toolchain/sh2eb-elf-lto"

export SATURN_TOOLCHAIN_ROOT="$PWD/toolchain/sh2eb-elf-lto"
```

Necesita GCC y G++ del equipo, Make, las utilidades binutils del equipo,
Python 3, Bash, `tar`, `xz` y las cabeceras de desarrollo de la biblioteca C.
La carpeta de salida debe ser nueva. No instala paquetes del sistema ni necesita
permisos de administrador.

El script descarga las fuentes oficiales de **Binutils 2.40** y **GCC 13.2.0**,
comprueba sus SHA-256 y construye el enlazador, `libiberty` y el plugin LTO.
Conserva el compilador, el ensamblador y las bibliotecas de destino del paquete
de partida. La copia nueva configura las instrucciones de enlace de GCC para
usar el plugin; el paquete original permanece intacto.

Reserva al menos 2 GB para las fuentes y la construcción temporal, además del
espacio de la copia de salida. Por defecto se trabaja bajo `/tmp`. Puedes elegir
otro directorio padre con `SPANG_TOOLCHAIN_BUILD_DIR` y limitar los procesos de
compilación con `SPANG_TOOLCHAIN_JOBS`, por ejemplo `SPANG_TOOLCHAIN_JOBS=4`.
Los registros se conservan en el directorio temporal indicado al terminar.

Esta herramienta corresponde a ese paquete heredado. Si tu GCC ya supera la
prueba LTO, utiliza directamente su directorio con `SATURN_TOOLCHAIN_ROOT`.
El origen y las huellas de las fuentes de GNU están fijados en el script.

## 1. Importar los recursos locales

Desde la raíz del repositorio:

```sh
python3 tools/import_resources.py /ruta/SuperPang_1.4_FUENTES.zip
python3 tools/import_resources.py --check
```

También puedes indicar el directorio extraído, sin cambiar sus contenidos:

```sh
python3 tools/import_resources.py /ruta/native320_r09
```

Para comprobar el origen y los posibles conflictos sin instalar recursos:

```sh
python3 tools/import_resources.py --dry-run /ruta/SuperPang_1.4_FUENTES.zip
```

El ZIP de referencia tiene estas características:

| Propiedad | Valor |
|---|---|
| Nombre original | `SuperPang_1.4_FUENTES.zip` |
| Tamaño | 185.292.771 bytes |
| SHA-256 | `aec07b4b6b3ffb6fc95365e1ebd96c3707bc5d30945a3b283fd4f881a8da8f9e` |
| Directorio de recursos dentro del ZIP | `native320_r09` |

El nombre del ZIP puede cambiar; su identidad se comprueba por tamaño y SHA-256.
Volver a comprimirlo cambia su identidad aunque conserve los mismos recursos.
Para una copia extraída se comprueba cada archivo requerido por separado.

### Qué se importa

La lista completa, con tamaño, SHA-256 y finalidad de cada entrada, está en
[`resources/manifest.json`](../resources/manifest.json). No contiene los datos de
los recursos.

| Grupo | Archivos | Bytes |
|---|---:|---:|
| Audio utilizado por el juego | 53 | 195.829.852 |
| Gráficos que se leen desde el disco | 7 | 601.847 |
| Datos generados compilados con el código | 11 | 1.575.778 |
| Cabeceras de C/newlib del entorno heredado | 127 | 663.659 |
| Plantilla `Compiler/COMMON/IP.BIN` | 1 | 6.144 |
| **Total** | **199** | **198.677.280** |

Los siete archivos gráficos de ejecución son `AUTH4.BIN`, `CELLH4.BIN`,
`CELLT4.BIN`, `CHARDF.BIN`, `CHARMDF.BIN`, `VIEWX.BIN` y `VIEWY.BIN`, todos en
`cd/ART`. El catálogo `audio/catalog.h` referencia los 53 archivos de `cd/AUDIO`.

Los datos generados incluyen las tablas de niveles, bancos de la interfaz,
catálogos de audio y sprites, perfiles de colisión y píxeles compilados del título,
los ítems y los avisos de inicio y pausa. Que estén escritos como `.c`, `.h` o
`.inc` no cambia su procedencia; por eso también se suministran localmente.

Las cabeceras de `Compiler/WINDOWS/sh-elf/include` se usan también al compilar en
Linux: el nombre del directorio procede del SDK heredado. El Makefile las añade
expresamente a la búsqueda de cabeceras. El toolchain de referencia no incluye
por sí solo todas las cabeceras C que este código necesita.

El importador comprueba **todos** los recursos y los archivos existentes antes
de instalar los que faltan. Solo copia las entradas de la lista; no extrae la
ROM, una BIOS, capturas, evidencias antiguas ni otros contenidos del archivo.
Repetirlo con los mismos recursos conserva los archivos ya instalados. Si un
archivo de destino contiene una edición local, se detiene sin sobrescribirla.
También rechaza rutas ambiguas, entradas ZIP duplicadas y enlaces simbólicos en
las rutas utilizadas.

Los recursos importados están excluidos del seguimiento de Git. Su presencia
local permite compilar; no implica que formen parte de la publicación del
repositorio.

## 2. Compilar

Indica el directorio que contiene `bin/sh2eb-elf-gcc`:

```sh
export SATURN_TOOLCHAIN_ROOT=/ruta/al/sh2eb-elf
python3 tools/build.py
```

El lanzador valida primero los 199 recursos, la versión y el destino del
compilador, el enlace LTO y la presencia de las herramientas del equipo. Después crea el
archivo de opciones de Jo Engine si falta y ejecuta una compilación limpia:
`make clean`, seguido de `sh build.sh`. Elimina las opciones de Make heredadas
mediante `MAKEFLAGS`, `MFLAGS` y `MAKEOVERRIDES` para que no conviertan la
compilación en una simulación ni sustituyan sus variables sin indicarlo.

La cadena de construcción es:

1. `cc` compila `JoEngineCueMaker.c` y `JoEngineIsoToBin.c` para el equipo.
2. GCC compila y enlaza el código C y el arranque SH-2, con LTO.
3. `objcopy` genera el ejecutable binario y `cd/0.bin`.
4. `ip.S` construye la cabecera del disco usando la plantilla local `IP.BIN`.
5. `mkisofs` o `xorrisofs` empaqueta el ejecutable y los recursos del directorio
   `cd` en un ISO.
6. `JoEngineIsoToBin` convierte sus sectores a MODE1/2352 y se escribe el CUE.

Los resultados están en la raíz del repositorio:

| Archivo | Uso |
|---|---|
| `game.elf` | Ejecutable enlazado para inspección y depuración |
| `game.map` | Distribución de secciones y símbolos en memoria |
| `game.raw` | Ejecutable plano |
| `game.iso` | Pista de datos con sectores de 2048 bytes |
| `game.bin` | Imagen con sectores MODE1/2352 |
| `game.cue` | Descripción de la pista para herramientas y emuladores |

La música y los efectos son archivos de la pista de datos. Este disco no utiliza
pistas CD-DA. Las fechas, el empaquetado del ISO y el ejecutable pueden variar
con el entorno de construcción. El [informe de validación](VALIDACION_1.4.md)
recoge las diferencias observadas entre la reconstrucción Linux y la entrega
original. No se promete una reproducción idéntica byte por byte.

## 3. Comprobar el resultado

Las comprobaciones del importador usan pequeñas muestras artificiales y no
necesitan los recursos del juego:

```sh
python3 -m unittest discover -s tools/tests -v
```

Después de compilar, verifica el contenido del disco y sus sectores:

```sh
python3 tools/verify_disc.py
cc -O2 tools/verify_cd_raw.c -o /tmp/spang-verify-cd
/tmp/spang-verify-cd game.bin
```

Estas comprobaciones verifican estructura e integridad. Las pruebas de ejecución
en emulador o Saturn física son una fase distinta; una compilación correcta no
las sustituye. Para el alcance de las pruebas de la publicación, consulta la
documentación y los informes correspondientes del repositorio.

## 4. Cargar o grabar la imagen

Conserva `game.bin` y `game.cue` en el mismo directorio y abre el **CUE**. Ese
archivo indica el nombre de la imagen y el tipo de sectores. Si cambias el nombre
del BIN, actualiza también la línea `FILE` del CUE.

- **Mednafen:** configura una BIOS que puedas utilizar y carga `game.cue`. Las
  instrucciones del emulador están en su [documentación de Saturn](https://mednafen.github.io/documentation/ss.html).
  La prueba de referencia del proyecto utilizó Mednafen 1.32.1; otros emuladores
  o configuraciones necesitan su propia comprobación.
- **Terraonion MODE:** copia juntos el BIN y el CUE al almacenamiento del equipo,
  dentro de un directorio para el juego, y selecciona esa imagen con la
  configuración habitual de tu Saturn. El repositorio no actualiza el firmware
  ni modifica el equipo.
- **CD-R:** utiliza la función de grabar imagen de tu programa y selecciona el
  CUE. Copiar ambos archivos como documentos dentro de un CD de datos produce un
  disco distinto. El arranque de software casero depende de la configuración de
  la consola; esta imagen no incorpora un procedimiento para modificarla.

Estas indicaciones describen el uso de los archivos. La confirmación física de
1.4 no equivale a compatibilidad comprobada con todas las Saturn, grabadoras,
soportes u ODE.

## Por qué aún no basta con la ROM arcade

Los scripts históricos de creación de recursos no forman actualmente una cadena
autónoma que parta de `spang.zip` y termine en todos los archivos de esta versión:

- La preparación de niveles y parte de los gráficos dependen de
  `c13_reference/fuentes`, con utilidades, datos decodificados y estados de
  referencia. También hay referencias a los dibujos anteriores de
  `art320/target_v001`.
- Los exportadores de audio toman un disco o ZIP de la versión C13, con sus
  catálogos y manifiestos. No sintetizan toda la música directamente desde la ROM
  arcade.
- El título de esta versión procede de una captura de Mega Pang v0.3 y tiene una
  adaptación específica; no se obtiene de `spang.zip`.
- Algunas adaptaciones de píxeles y efectos se fijaron mediante recursos y
  capturas revisados en etapas anteriores.

Una futura ruta basada solamente en recursos originales aportados por el usuario
necesita completar, documentar y comprobar esos conversores. La ruta descrita
aquí conserva los recursos preparados de 1.4 mediante sus hashes y permite
compilar cambios del código sin afirmar que esa reconstrucción completa ya
existe.

## Problemas habituales

| Mensaje o situación | Qué comprobar |
|---|---|
| El ZIP no coincide | Usa el archivo 1.4 indicado; si solo dispones de una copia extraída, pasa `native320_r09` y deja que se comprueben sus recursos. |
| Falta un recurso | La extracción está incompleta o aún no has ejecutado el importador. |
| Archivo local modificado | Guarda o mueve esa edición antes de volver a importar. El importador no la reemplaza. |
| Falta `stdint.h` | Comprueba la importación de las cabeceras heredadas y no elimines su ruta del Makefile. |
| GCC distinto de 13.2.0 o destino distinto de `sh2eb-elf` | Revisa `SATURN_TOOLCHAIN_ROOT`; debe señalar al toolchain requerido, no al compilador del equipo. |
| La prueba LTO falla, o GCC solo admite objetos LTO con código convencional | Necesitas un toolchain con plugin de enlace funcional. La versión 13.2.0 por sí sola no garantiza esa capacidad. |
| Ruta no admitida | Mueve el checkout o el toolchain a una ruta sin espacios ni caracteres especiales. |
| Falta una herramienta ISO | Instala `xorriso`/`xorrisofs` o `mkisofs` y añádelo a `PATH`. |

El manifiesto fija los recursos de la versión publicada. Si deseas modificar
también esos recursos, necesitarás tu propio flujo de autoría y revisar de forma
explícita las identidades esperadas; el importador no actualiza el manifiesto
automáticamente.
