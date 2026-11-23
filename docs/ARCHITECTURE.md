# Super Pang Saturn 1.4 Architecture

**English** | [Español](ARQUITECTURA.md)

Super Pang Saturn runs the game logic as native C code for the SH-2, using a modified copy of **Jo Engine 2026.3** as its foundation. The 1.4 configuration uses the master SH-2, displays the image at **320 × 240**, and plays PCM music and sound effects through the SCSP. The actual division of work between processors and chips is described here based on the code compiled for this version.

The arcade reference uses Mitchell hardware: a Z80-family CPU with Kabuki protection and YM2413/OKI M6295 sound. The external reference for this identification is the [MAME Mitchell driver](https://github.com/mamedev/mame/blob/master/src/mame/capcom/mitchell.cpp). The bootleg variants described in that same file have hardware differences; their configurations should not be attributed to this port.

## Native logic and reference data

The modules in [`stage/`](../stage) translate game behavior into C functions: movement, collisions, balls, weapons, items, animals, scoring, progression, and endings. They retain many byte structures, tables, and states from the reference. This is why addresses such as `E080`, `8000`, or `D000` appear in comments, checks, and state values.

Those addresses identify the origin or meaning of a piece of data. The executable does not contain a loop that reads and executes Z80 instructions. For example, [`spang_player_control.c`](../stage/spang_player_control.c) calculates movement and animations through C operations, and [`spang_panic.c`](../stage/spang_panic.c) dispatches known states of the ball scheduler. Character and attribute writes go through functions that convert reference addresses into array indices.

[`stage.c`](../stage/stage.c) brings these modules together and maintains the game state. It exposes a read-only view for rendering and an ordered queue of audio requests. [`main.c`](../main.c) coordinates controller input, screen ownership—gameplay or menus—resource preparation, sound, and presentation. [`frontend/`](../frontend) contains the title screen, mode selection, demos, high scores, and options.

Generated tables and scenes, such as `stage/stage_data.c`, and the catalogs and graphics and audio assets are imported locally from the dependency package specified in the build instructions. Linking those tables into the program does not make their contents code executed by an emulated CPU. Nor does it mean that every game path has been shown to match the original board: the scope of the checks is documented separately in [1.4 Validation](VALIDATION_1.4.md).

## Division of work on Saturn

| Component | Use in the 1.4 configuration |
| --- | --- |
| Master SH-2 | Runs the application, simulation, graphics command preparation, file management, and audio control. |
| Slave SH-2 | Jo's dual-CPU module is disabled in this build. |
| VDP1 | Draws sprites and graphical elements using 4-bit textures and color tables. |
| VDP2 | Displays the background and text in an RGB555 NBG1 bitmap and composites the result with VDP1 output. |
| SCSP | Plays the PCM music and sound-effect samples stored in sound RAM. |
| Sound 68000 | Runs a small wait loop; the SH-2 programs the SCSP slots directly. |

This division depends on specific options. The [`makefile`](../makefile) sets `JO_MINIMAL=1` and enables the file, audio, and backup-memory modules. In [Jo's common makefile](../Compiler/COMMON/jo_engine_makefile), minimal mode leaves `JO_COMPILE_WITH_DUAL_CPU_MODULE` disabled unless the game explicitly enables it. This project does not enable it. The presence of `jo_engine/slave.c` in the tree reflects an engine capability, not work that this version sends to the second SH-2.

The same applies to the optional 68000 sound driver. `JO_COMPILE_WITH_AUDIO_DRIVER_MODULE` and the audio DSP effects module are not enabled. The compiled path in [`jo_engine/audio.c`](../jo_engine/audio.c) installs the 68000 wait loop and controls the SCSP from the SH-2.

## Image rendering and adaptation to 320 × 240

The video target in the [`makefile`](../makefile) is NTSC, 320 × 240, with a presentation interval of one VBlank. The application disables interlacing in [`main.c`](../main.c). This describes the program's configuration; it is not a measurement of a constant frame rate across all scenes.

### Background and text on VDP2

[`renderer.c`](../renderer.c) reconstructs the background and HUD from characters, attributes, and palettes. It writes RGB555 pixels into the NBG1 bitmap and tracks changes to update the areas that need it. Although the input data is organized as tiles, this rendering path uses a VDP2 bitmap for presentation.

Two pages of **512 × 256 × 2 bytes** are reserved: one occupies banks A0/A1 and the other B0/B1. Together they occupy the **512 KiB of VDP2 VRAM**. The visible window is 320 × 240; the remaining area is kept ready for the centering adjustment. Allocation and swapping are implemented in [`jo_engine/vdp2.c`](../jo_engine/vdp2.c).

Full scene changes are drawn on the hidden page. Once drawing is complete, the renderer associates its presentation with the identifier of the corresponding VDP1 command table. The swap is confirmed during VBlank when that frame is presented. It also keeps a second set of color tables so that the previous image retains its colors while the next one is being prepared.

The simulation accounts for that handoff: while a new scene is pending, it does not consume the time that scene should remain visible. The audio service continues to handle elapsed VBlanks. This coordinates background changes, sprites, and screen duration without reserving another framebuffer in work RAM.

### Sprites and geometry

VDP1 receives 4-bit textures with 16-entry CLUTs. Assets prepared for a 320-pixel display are combined with original cells that are composited and cached. There are specific paths for elements such as players, balls, items, ladders, maps, and the title screen. The graphics therefore receive more than one kind of treatment.

The horizontal conversion is explicit in [`render_common.c`](../render_common.c): world positions use a **5/6** ratio, adapting the reference's 384 pixels to a width of 320. Maps use a centered 320-pixel window at a 1:1 ratio. The HUD rearranges its blocks and preserves its character pixels; the two-player panels have their own adjustments. These choices mean that the entire image cannot be described as a pixel-for-pixel copy or as uniformly scaled.

The configured budget allows **2,400 sprite definitions** and **512 VDP1 commands**. The renderer also maintains a cache of **256 composite groups**. These are different internal limits: 2,400 definitions does not mean 2,400 objects drawn simultaneously.

## Game clock and input

[`spang_frame_clock.h`](../stage/spang_frame_clock.h) accumulates **2,871/3,000 logical steps per VBlank**, relative to a nominal 60 Hz presentation rate. The reference cadence adopted by the code is therefore **57.42 logical updates per nominal second**. Each call performs at most eight pending updates and retains the fraction and remaining backlog.

Input handling distinguishes held buttons from new presses and preserves new presses when a frame produces no logical update. Pausing stops gameplay progression and keeps exit presses separate from actions to be performed on resuming. This coordination, and the return to the title screen or BIOS, are controlled in [`main.c`](../main.c).

## Assets and memory

The main graphics assets are read during initialization. They then remain available in work RAM or VRAM; some derived textures are created when first needed and cached. Some source patterns—**160 KiB**—are stored in VDP1 using ten storage textures that are not drawn. This frees work RAM for audio.

Glyphs are kept compressed and decoded in bounded blocks, using eight reusable working slots. Scenes linked into the executable are decompressed into a **10,240-byte** buffer, defined in [`spang_scene_data.h`](../stage/spang_scene_data.h). The [scene loader](../stage/spang_scene_data.c) applies LZSS decompression and differences relative to a base scene. Normal background updates do not reread their graphics from disc.

The application adds the **MiB of Low Work RAM** to Jo's allocator. In High Work RAM, it uses the configured heap reservation and the usable gaps left by linking. The [linker script](../Compiler/COMMON/jo.linker) protects a **16 KiB** stack for the master SH-2, between `0x060FBC00` and `0x060FFC00`. The RAM cartridge module is disabled; this configuration uses Saturn's internal memory.

The [memory audit for the 1.4 release](evidence/reference-memory.json) records these budgets, among others:

| Area | Value in the reference report |
| --- | ---: |
| Main heap reservation in High RAM | 108,544 bytes |
| Usable area before `JO_WORK` | 11,312 bytes |
| Usable area after `JO_WORK` | 78,384 bytes |
| Calculated VDP1 texture capacity | 451,832 bytes |
| Two VDP2 bitmaps | 524,288 bytes |
| PCM resident in sound RAM | 392,768 bytes |
| Music ring buffers, including alignment | 65,568 bytes |
| Area available for temporary PCM | 61,856 bytes |

These values come from the executable layout, the catalog, and an allocator model. Texture capacity is not equivalent to free memory during gameplay, and heap gaps do not measure stack headroom. [`stack_usage.c`](../stack_usage.c) includes a conservative measurement starting at application entry; its comment explicitly identifies the earlier startup and worst-case behavior that this measurement does not cover.

## Audio: prepared files and PCM playback

The music comes from the C13 project's preparation pipeline and is delivered to the runtime as mono PCM WAV files at **16 bits and 44,100 Hz**, with defined loop points. The port uses these prepared samples; playback does not run a YM2413 synthesizer or a Z80 sequencer in real time.

The 1.4 catalog contains **26 music files**, **19 effects derived from the FM path**, and **8 samples derived from OKI**. The implementation in [`audio/audio.c`](../audio/audio.c) preserves request identifiers, priorities, volume, fades, pause, and resume. It provides one logical voice for FM effects and four for OKI samples.

Effects are loaded at startup: some remain resident in sound RAM, while others stay in work RAM until transferred to the SCSP. FM effects normally use 16-bit mono PCM at 22,050 Hz; `F00` has its own 8-bit, 44,100 Hz format. Prepared OKI samples play as 16-bit PCM at 7,576 Hz. Decoding the source formats is part of asset preparation.

Music is read progressively from disc. Jo maintains a **64 KiB** input buffer in work RAM and reserves two ring buffers of **16,384 samples** in sound RAM. A bounded cache of up to four music beginnings is prepared at explicit boundaries where playback is stopped, such as a mode or scene change. A normal sound request uses the prepared resources; a missing catalog entry or missing preparation is reported as a failure.

The generated disc contains one data track. Music and effects are files on that track; there are no CD-DA tracks. Progressive reading, buffer replenishment, and audio diagnostics belong to [Jo's audio module](../jo_engine/audio.c), with project adaptations for its loops and prepared music beginnings.

## Saving and system coordination

[`savegame.c`](../savegame.c) uses Saturn's internal backup memory through the BIOS. It maintains two alternating files, `SPANG_REC_A` and `SPANG_REC_B`, with generations and verification after writing. Persistent data consists of high scores and settings; the current game and still-active name editors remain in RAM.

The defining change in 1.4 affects this initialization: [`jo_engine/backup.c`](../jo_engine/backup.c) suspends the start of new periodic `INTBACK` requests while initializing the backup-memory driver and restoring RESET. [`jo_engine/smpc.c`](../jo_engine/smpc.c) preserves normal completion of an earlier request, then resumes periodic polling. Details of this change and the scope of its validation are in [Version Status](STATUS.md).

## Building and limits of this description

The Jo base and its modifications are identified in [`JO_ENGINE_SOURCE_NOTICE.txt`](../JO_ENGINE_SOURCE_NOTICE.txt). The configuration uses C for big-endian SH-2, GCC 13.2, and LTO. The public build entry point is [`tools/build.py`](../tools/build.py), once its local dependencies have been imported.

Building the tree with prepared assets and regenerating all those assets from their sources are separate tasks. This description documents the 1.4 runtime and its budgets; implementation status, tests, and remaining work are maintained in [STATUS.md](STATUS.md) and [VALIDATION_1.4.md](VALIDATION_1.4.md).
