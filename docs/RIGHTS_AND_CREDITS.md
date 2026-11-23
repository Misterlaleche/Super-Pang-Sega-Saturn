# Rights and credits

**English** | [Español](DERECHOS_Y_CREDITOS.md)

Super Pang Saturn is a personal hobby project and remains in development.
It is shared to demonstrate the work of adapting the game to Saturn and to
document how to rebuild it locally. It is not affiliated with Mitchell, Capcom, or SEGA.

The reference game is **Super Pang (World 900914)**, by Mitchell, released
in 1990. The game's name, graphics, music, sounds, and other original elements
belong to their respective rights holders. This project claims no rights
over them.

## The code in this project

To reproduce the arcade's behavior, the original has been studied and parts
of its logic have been translated or reimplemented in C. The Saturn adaptation,
rendering, audio integration, and development tools are described in the
technical documentation.

No general license has been assigned to the port's own code at this time.
The fact that the code can be viewed publicly does not mean that the entire
repository has an open-source license. Third-party licenses apply only to
the components they identify; this document does not change their terms or
grant rights over other parties' materials.

## Files required to build

The original game files and the resources derived from them are prepared
locally. The same applies to any BIOS an emulator may need and to the
`IP.BIN` boot template: these are not materials that this project can include
under the Jo Engine license.

You must obtain the required external files from a lawful source and have
the relevant permissions to use them. Owning a copy of the game does not,
by itself, grant permission to download any copy from the Internet.
Creating a playable image also does not grant permission to redistribute
the original files or an image that incorporates them.

The tools that study or convert the original artwork do not include those
resources. Do not add ROMs, BIOS files, `IP.BIN`, graphics, extracted music,
or playable images to commits or issue attachments.

## Jo Engine

The port uses **Jo Engine**, by **Johannes Fetz**, with local changes for
this project. Its official repository declares the MIT license, and the
source files in the included version also retain notices with the terms
of the three-clause BSD license.

Both notices are retained, along with the file headers:

- [JO_ENGINE_LICENSE.txt](../JO_ENGINE_LICENSE.txt): MIT license.
- [JO_ENGINE_SOURCE_NOTICE.txt](../JO_ENGINE_SOURCE_NOTICE.txt): source
  notice and notes on local changes.

These notices must accompany redistributions to which they apply.
Crediting Jo Engine does not imply that its author endorses this port.

## MAME and the audio tools

**MAME** has served as a reference for studying the arcade hardware,
observing its behavior, and cross-checking results. The Mitchell driver
identifies the reference game and its variants.

The historical conversion tools include the decoder
[spang_oki_adpcm.c](../audio/vendor/spang_oki_adpcm.c) and
[its interface](../audio/vendor/spang_oki_adpcm.h). The algorithm is adapted
from `okiadpcm.cpp`, by **Andrew Gardner and Aaron Giles**, under the
**three-clause BSD** license. The complete notice is retained in
[MAME-OKI-BSD-3-Clause.txt](../audio/vendor/MAME-OKI-BSD-3-Clause.txt).

That historical notice also credits **Mirko Buffoni and Aaron Giles** for
`okim6295.cpp`, which was used in the reference material. It does not
attribute the other C13 and Rice helpers in `audio/vendor` to these authors,
nor does it automatically extend the BSD license to everything in that folder.

[PROVENANCE.json](../audio/vendor/PROVENANCE.json) retains the source paths
and hashes of the copies included during development. Its paths describe
the organization of that version of the project, not files to download or
paths required for the current build.

The MAME license does not grant rights over game ROMs. The notices for its
code and the rights over the arcade resources are separate matters.

## tinf: graphics decompression

[glyph_inflate.h](../art/r10_r4/glyph_inflate.h) contains an adaptation of
**tinf / tiny inflate**, by **Joergen Ibsen**, with copyright 2003–2019 and
the **zlib** license. The complete notice is at the beginning of that file.

The header identifies the original revision and explains the changes made
for the port: raw DEFLATE decompression, bounds checks, and adjustments to
the lookup tables. Both the original notice and the identification of this
modified version are retained. The license requires retaining the notice
and prohibits claiming authorship of the original or presenting a modified
version as though it were the original.

## The nature of the project

Offering this work free of charge as a hobby does not replace permission
from the rights holders. The notices above describe the provenance and
limits of what is shared; they do not make the arcade materials freely
licensed content.

As a reference for readers in Spain, the Intellectual Property Law excludes
computer programs from the general private-copying exception
(Article 31.3.c). Articles 99 and 100 govern rights in those programs and
the specific exceptions, including certain operations by a lawful user,
backup copies, and interoperability, each subject to its own conditions.

## References

- [Jo Engine: official repository](https://github.com/johannes-fetz/joengine).
- [Jo Engine: official documentation](https://jo-engine.org/doxygen/).
- [Example of the notices retained in Jo Engine source files](https://jo-engine.org/doxygen/entity_8h_source.html).
- [MAME: Mitchell driver](https://github.com/mamedev/mame/blob/master/src/mame/capcom/mitchell.cpp).
- [MAME: licensing information](https://www.mamedev.org/legal.html).
- [tinf: Joergen Ibsen's repository](https://github.com/jibsen/tinf).
- [tinf: zlib license](https://github.com/jibsen/tinf/blob/master/LICENSE).
- [tinf: source revision identified in the adaptation](https://github.com/jibsen/tinf/blob/c9561a6c6950a9fa4cd59cce481661c1e4e2156e/src/tinflate.c).
- [Intellectual Property Law: consolidated text from the BOE](https://www.boe.es/buscar/act.php?id=BOE-A-1996-8930).
