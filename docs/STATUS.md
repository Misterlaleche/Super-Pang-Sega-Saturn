# Super Pang Saturn 1.4 status

**English** | [Español](ESTADO.md)

**Super Pang Saturn is still in development. Version 1.4 works on a physical Saturn, according to the confirmation now received for this release.** This is recorded as a general confirmation. The specific tests that accompanied the package are described separately, because they come from code analysis, tests on the development computer, and Mednafen sessions.

This document distinguishes what the code implements, what changes in 1.4, and what the available checks allow us to claim. The internal workings are explained in [ARCHITECTURE.md](ARCHITECTURE.md).

## Implemented scope

| Area | What the 1.4 code includes | Reference |
| --- | --- | --- |
| Tour | All 40 stages, their scene changes, maps, bonuses, and ending sequence. Progression enters the ending after stage index 39. | [`stage.c`](../stage/stage.c), [`spang_round_flow.c`](../stage/spang_round_flow.c) |
| Panic | Ball scheduling, progression up to level 100, background changes, game-end handling, and entry into the ending sequence. | [`spang_panic.c`](../stage/spang_panic.c), [`stage.c`](../stage/stage.c) |
| Players | Starting with one or two players, independent state and controls, scores, and per-player game-end handling. Joining during a game depends on the mode and game state. | [`stage.h`](../stage/stage.h), [`main.c`](../main.c) |
| Presentation | Title, mode selection, stage selection, high-score screens, and name entry. | [`frontend.h`](../frontend/frontend.h), [`frontend.c`](../frontend/frontend.c) |
| Demonstrations | Two Tour demonstrations, two tutorial sequences, and one Panic demonstration, with inputs prepared from the reference sequences. | [`stage.h`](../stage/stage.h), [`spang_attract_input.c`](../stage/spang_attract_input.c) |
| Options | Difficulty, lives, demonstration sound, mono output, music and sound-effect levels, and screen centering. | [`settings.c`](../frontend/settings.c) |
| Saving | High scores and settings in internal memory, using two alternating files and write verification. | [`savegame.c`](../savegame.c), [`save_codec.c`](../frontend/save_codec.c) |
| Session control | Pause, return to the title, and exit to the BIOS using the button combination defined by the application. | [`main.c`](../main.c) |

The presence of these paths can be checked in the code. Full acceptance of a playthrough, its accuracy in every situation, and its sustained behavior on different systems require runtime evidence covering that scope. The table therefore describes implementation and does not replace a test record for every stage and combination.

## What changes in 1.4

The release retains the 1.3 foundation and focuses its functional change on coordination between BIOS saving and the SMPC. The archived comparison between the two versions identified production changes in `jo_engine/backup.c`, `jo_engine/internal/smpc.h`, `jo_engine/smpc.c`, and the `makefile`. The 60 files in the `ART` and `AUDIO` directories retained their contents and disc positions relative to the compared 1.3 release.

The issue under investigation was a hang during loading on a Saturn with MODE. Earlier observations located a wait around save-driver initialization and RESET re-enabling. The working hypothesis was a possible overlap with the periodic `INTBACK` requests used to read the controllers.

The fix in [`backup.c`](../jo_engine/backup.c) pauses the start of new `INTBACK` requests before that initialization and resumes them after restoring RESET. A request that has already started retains its normal interrupt-driven completion path. The change also applies when the save driver is reinitialized for writing.

The current confirmation that 1.4 works on a physical Saturn updates the release status. On its own, it does not definitively identify the cause of the earlier hangs or make every save operation or peripheral combination a documented test. The explanation of the change stays within what the code and available observations support.

## How to interpret the checks

The [1.4 validation report](VALIDATION_1.4.md) separates the evidence that accompanied the package from the reproduction checks performed for this repository. Each type of test answers different questions:

| Evidence | Scope |
| --- | --- |
| Current confirmation on a physical Saturn | General operation of version 1.4 on a real console. |
| Build and disc inspection | Executable construction, image structure, and correspondence of the delivered files. |
| Static analysis and allocator model | Section limits, reservations, and allocation sequences represented in the model. |
| Logic tests on the development computer | Behavior of the functions and paths exercised by each test. |
| Simulated SMPC registers and simulated storage | Order of operations, temporary exclusion of new requests, and data recovery in the modeled scenarios. |
| Mednafen | Behavior observed while running the image in that emulator and in the inspected states. |

The archived tests of the 1.4 candidate included a build without warnings, inspection of **96,395 sectors with no defective sectors**, allocation sequences with no failures in the model, **360,000 modeled Panic updates**, save tests with corruption and unavailable memory, and **21 returns to Options from screen centering** with simulated video components. These are results from the tests described in the reference package; they are not presented as an equivalent hardware session.

The documented Mednafen session used the American region and observed the title, entry into and progression in Panic, return to the title, and exit to the BIOS using `A+B+C+START`. The screenshots accompanying that evidence are from Mednafen. The absence of failures in the indicators for the inspected states applies to those states, without demonstrating a complete extended playthrough.

Older notes stating that 1.4 had not yet been tested on Saturn reflected the time when the candidate was prepared. The physical-hardware confirmation received later is the current status. Likewise, the R10/R13/R16 revision reports retained in the working archive belong to their respective revisions; they must not be used as an automatic description of 1.4.

## Limits and remaining work

The port has a presentation adapted to 320 × 240. It combines horizontal compression of world geometry, maps at 1:1 scale, and specific HUD and sprite composition. Visual accuracy should be assessed with those decisions in mind, as explained in [Architecture](ARCHITECTURE.md).

The current configuration uses the master SH-2 and internal RAM. Jo's optional capabilities for the second SH-2, RAM cartridge, or a sound driver on the 68000 do not describe features enabled in this version. Music plays from prepared PCM files, read progressively from the data track.

Memory budgets are tight and depend on the asset catalog. The reference report calculates **2,276 bytes** of headroom in the temporary sound area after the largest nonresident effect. This headroom is a calculation for the audited set. Any asset expansion must be checked again against the catalog and executable layout. Neither the stack reservation nor the allocator model measures the worst case of an entire session on physical hardware.

The following additions to the evidence would help define the project's status more precisely:

- Record the console, video region, loading medium, controllers, and duration of physical-hardware sessions with 1.4.
- Document complete Tour and Panic playthroughs, including two players, continues, endings, and high-score entry.
- Check that settings and high scores persist after restarting and reloading the version.
- Keep memory, audio, and scene-synchronization validation up to date alongside changes that may affect their budgets.

These points define the follow-up work in progress. Any claim that a specific check has been completed will be supported by its record in [VALIDATION_1.4.md](VALIDATION_1.4.md).
