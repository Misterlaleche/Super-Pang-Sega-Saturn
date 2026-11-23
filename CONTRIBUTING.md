# Colaborar en Super Pang Saturn

La referencia de este repositorio es la versión 1.4. Las mejoras deben poder compararse con ella, especialmente cuando afecten a la lógica, al orden de las colisiones o a las operaciones de CD y audio.

## Informar de un problema

Abre una incidencia con el modo de juego, la fase y los pasos necesarios para llegar al fallo. Si utilizas una compilación propia, incluye el commit y la huella de `game.bin`. En Linux puede obtenerse con `sha256sum game.bin`; en macOS, con `shasum -a 256 game.bin`.

Para pruebas físicas, indica modelo y región de Saturn, método de carga, cartuchos y periféricos conectados. Para pruebas en emulador, indica nombre, versión y configuración relevante. Una grabación breve puede ayudar a describir una anomalía gráfica, un sonido o una parada, pero no hace falta adjuntar una imagen de disco.

No adjuntes ROMs, BIOS, música, volcados completos de recursos ni imágenes BIN/CUE. Tampoco incluyas datos privados de otras partidas o cuentas. Un registro de diagnóstico, una descripción reproducible y la huella del archivo suelen ser suficientes.

## Proponer cambios

Describe el problema, el comportamiento esperado y qué modifica el parche. Mantén los arreglos acotados: un cambio de física, uno de gráficos y otro de carga son más fáciles de comprobar por separado.

No sustituyas rutinas por aproximaciones sin explicar la diferencia respecto al arcade. Si una optimización cambia el orden de las operaciones, documenta por qué mantiene el resultado. Para cambios de memoria, anota los límites y el peor caso que se haya probado.

Conserva los avisos de terceros e identifica cualquier material nuevo y su procedencia. La situación de las licencias se explica en [Derechos y créditos](docs/DERECHOS_Y_CREDITOS.md); una contribución no puede conceder derechos sobre recursos del juego original.

## Comprobaciones

Las pruebas de las herramientas de publicación no necesitan los recursos del juego:

```sh
python3 -m unittest discover -s tools/tests -v
```

Con los recursos locales y la toolchain disponibles:

```sh
python3 tools/import_resources.py --check
python3 tools/build.py
python3 tools/verify_disc.py
```

Si el cambio afecta al juego, añade los pasos y los resultados de la prueba funcional. Distingue siempre las comprobaciones del ordenador, el emulador y la consola. Compilar correctamente no demuestra que una situación de juego esté resuelta.
