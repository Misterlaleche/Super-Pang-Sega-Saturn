# Super Pang for Sega Saturn

**English** | [Español](README.es.md)

An unofficial Sega Saturn port of **Super Pang**, Mitchell's 1990 arcade game. The reference release is **SuperPang 1.4**, which has been confirmed working on real Saturn hardware. This is a work in progress: differences from the arcade still need review, gameplay scenarios need testing, and performance and compatibility need further work.

The aim is to preserve the arcade game's behavior while adapting its presentation to **320 × 240** for CRT displays. This repository contains the port's source code, the version of Jo Engine it uses, build tools and technical notes. It also documents the current limitations and what you need to build your own disc image.

## Version 1.4 status

| Area | Current status |
| --- | --- |
| Hardware | Confirmed working on a real Saturn; the scope of that test is recorded in [Validation](docs/VALIDATION_1.4.md). |
| Video | NTSC, 320 × 240, progressive. |
| Gameplay | All 40 Tour stages and the ending are implemented, along with Panic mode, options, pause, attract sequences and one- or two-player games. Their presence in the code does not mean every playthrough has been thoroughly tested. |
| Audio | Music prepared as PCM and sound effects played through the SCSP. All audio is read from the data track. |
| Saving | High scores and settings saved to the Saturn's internal backup memory. |
| RAM expansion | Disabled in this build. A 4 MB cartridge is not required. |
| SDK | Jo Engine 2026.3, with local modifications. |
| Compiler | GCC 13.2.0 targeting SH-2, big endian, with link-time optimization. |

The [status and outstanding work](docs/STATUS.md) document distinguishes implemented features from verified behavior. Version 1.4 is neither a finished port nor a claim of perfect arcade accuracy.

## Bringing the arcade game to Saturn

The starting point is **Super Pang (World 900914)**. Its board belongs to the Mitchell hardware family: it uses a Z80-compatible processor with Kabuki protection, character and object graphics, and YM2413/OKIM6295 sound. Hardware identification and the memory map are documented in the [MAME driver](https://github.com/mamedev/mame/blob/master/src/mame/capcom/mitchell.cpp).

The port involves studying those routines and data, translating their behavior into C, and building a presentation that fits the Saturn. At runtime, the game uses native routines compiled for SH-2. Original tables, states and data layouts are retained where they help preserve the game's behavior; there is no Z80 interpreter running the complete ROM.

Version 1.4 divides the work as follows:

- **Main SH-2:** game logic, controls, collisions, graphics preparation, CD access and audio servicing. The second SH-2 is not enabled in this build.
- **VDP1:** characters, balls, harpoons, items and other moving elements, using textures and command lists.
- **VDP2:** final composition and backgrounds/HUD on an NBG1 bitmap with two pages. The CPU prepares changes to that bitmap.
- **SCSP:** playback of samples and music buffers. Music is prepared ahead of time; the game does not synthesize the YM2413 in real time.

The graphics adaptation combines cell composition, asset conversion and presentation adjustments. The arcade's horizontal playfield is mapped from 384 to 320 pixels; the HUD, menus and certain elements receive specific treatment to keep them readable. Game data and screen coordinates are handled separately.

[Architecture](docs/ARCHITECTURE.md) explains the implementation in detail, with links to the relevant functions and modules.

## What makes the port difficult

The Saturn has several processors and separate memory areas. That arrangement requires decisions about which data stays resident, what must be decompressed, when a page can be switched and how much time remains to service the CD drive. Free space in one memory area does not solve a shortage in another.

The order of game operations also matters. A small change to a ball's movement, a collision, a counter or the sequence in which items appear can change a playthrough. The logic clock uses a 2871/3000 accumulator per nominal 60 Hz VBlank; logic updates and video presentation are managed separately. A 60 Hz video output does not establish that every scene runs smoothly at that rate.

Music adds another requirement: buffers must receive data on time while rendering and game updates continue. The port therefore uses resident sound effects, preloads the beginnings of music tracks, and limits operations that can block during gameplay.

Real hardware remains the reference. Version 1.4 coordinates periodic controller reads with backup-memory initialization. This addressed a startup lockup that earlier checks had not ruled out. Confirmation on Saturn is progress; broader testing of playthroughs, controllers, saving and long sessions is still needed.

## Build your own copy

**This repository does not include game assets, an arcade ROM, a Saturn BIOS or a BIN/CUE disc image.** Some game data was also embedded in C files; it is treated as an external resource and is not published here.

The available build process reconstructs version 1.4 using prepared resources from a local `SuperPang_1.4_FUENTES.zip` archive, obtained and used with the appropriate permissions. Its files are verified before being imported into the working tree. **Complete resource regeneration from `spang.zip` is still pending:** several converters depend on earlier development stages that are not included in this release. If you only have the arcade ROM, read [Resources and provenance](docs/RESOURCES.md) first.

On Linux, with Python 3.11 or later, Make, a host C compiler, xorriso and the specified SH-2 toolchain:

```sh
git clone https://github.com/Misterlaleche/Super-Pang-Sega-Saturn.git
cd Super-Pang-Sega-Saturn

python3 tools/import_resources.py /path/to/SuperPang_1.4_FUENTES.zip
python3 tools/import_resources.py --check

export SATURN_TOOLCHAIN_ROOT=/path/to/sh2eb-elf
python3 tools/build.py
python3 tools/verify_disc.py
```

The output is `game.bin` together with `game.cue`. The [build guide](docs/BUILDING.md) covers dependencies, checks and preparation for an emulator, MODE or CD-R. A file's inclusion among the dependencies does not grant permission to redistribute it.

## Repository contents

| Path | Contents |
| --- | --- |
| [`main.c`](main.c) | Startup, main loop and coordination between gameplay, video and audio. |
| [`stage/`](stage/) | Game logic, stages, balls, weapons, items, animals and modes. |
| [`frontend/`](frontend/) | Title screen, menus, options, high scores and settings. |
| [`renderer.c`](renderer.c), [`render_common.c`](render_common.c) | Rendering of sprites, stages and text, and coordinate conversion. |
| [`audio/`](audio/) | Playback, caches and historical audio preparation tools. |
| [`jo_engine/`](jo_engine/) | The engine copy used by version 1.4, including local changes and notices. |
| [`Compiler/COMMON/`](Compiler/COMMON/) | Linking, disc header and image creation utilities. |
| [`art/`](art/) | Converters and decompression code; game pixel data must be supplied locally. |
| [`resources/`](resources/) | Dependency inventory and hashes for the reference release. |
| [`tools/`](tools/) | Local resource import, building and verification. |
| [`docs/`](docs/) | Architecture, status, building, provenance and validation. |

Historical converters are retained to document the process. Their dependencies are explained in [Resources](docs/RESOURCES.md); the supported build command is `python3 tools/build.py`.

## Documentation

| Guide | English | Español |
| --- | --- | --- |
| Build requirements and instructions | [Building](docs/BUILDING.md) | [Compilar](docs/COMPILAR.md) |
| Hardware and implementation | [Architecture](docs/ARCHITECTURE.md) | [Arquitectura](docs/ARQUITECTURA.md) |
| Features, limitations and outstanding work | [Status](docs/STATUS.md) | [Estado](docs/ESTADO.md) |
| Local resources and provenance | [Resources](docs/RESOURCES.md) | [Recursos](docs/RECURSOS.md) |
| Version 1.4 validation and evidence | [Validation](docs/VALIDATION_1.4.md) | [Validación](docs/VALIDACION_1.4.md) |
| Rights, licenses and credits | [Rights and credits](docs/RIGHTS_AND_CREDITS.md) | [Derechos y créditos](docs/DERECHOS_Y_CREDITOS.md) |
| Contributions and hardware reports | [Contributing](CONTRIBUTING.md) | [Colaborar](CONTRIBUTING.es.md) |

## Contributing and reporting bugs

Contributions and reports are welcome in English or Spanish.

For a hardware issue, include the version, Saturn model, loading method, connected controllers and steps to reproduce it. A specific stage and a short sequence of actions are usually more useful than a general description that the game runs slowly. See [CONTRIBUTING.md](CONTRIBUTING.md) for details.

## Rights and credits

Super Pang Saturn is an unofficial project with no affiliation to Mitchell, Capcom or SEGA. The original names, graphics, music and other materials belong to their respective rights holders. Jo Engine and other third-party components retain their own notices and terms; this repository does not apply the engine's license to the original game.

Read [Rights and credits](docs/RIGHTS_AND_CREDITS.md) before reusing code or distributing files. An overall license for the port's own contributions has not yet been established.
