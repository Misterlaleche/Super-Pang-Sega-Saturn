# Version 1.4 validation and reproduction

**English** | [Español](VALIDACION_1.4.md)

Review date: **November 23, 2026**.

## Reference tested on Saturn

The project lead's confirmation is: **“SuperPang 1.4 now works on Saturn.”** This is recorded as confirmation that the original release works on a real console. No console model, session duration, or list of stages or peripherals has been added to that statement where none was specified.

The original image has this hash:

```text
SuperPang 1.4.bin
SHA-256 60b99dffaaa95b1304bbb1c2c6b910ebc8b407c12eb7dbeb3b4778a112712d15
```

The repository retains the identities in [`resources/reference-release.json`](../resources/reference-release.json), without including the image or its executable.

## Evidence accompanying the release

The evidence package was prepared before the physical-hardware confirmation was received. Its reports therefore retain `physical_saturn_tested: false`. That field describes the scope of those tests; it has not been rewritten to present a computer-based test as a console test.

The reference checks include:

- Building the candidate without warnings in the original environment.
- Inspecting 96,395 sectors of the original image, with no defective sectors.
- An allocator model with no failures in the represented sequences, using the release's memory limits.
- Modeled tests of Panic, saving, and SMPC coordination.
- Mednafen sessions covering the title, Panic, return to the title, and exit to the BIOS.

Two original summaries containing no game assets are published:

- [`reference-memory.json`](evidence/reference-memory.json): sections, reservations, and memory-model results.
- [`reference-production-comparison.json`](evidence/reference-production-comparison.json): production files that changed between 1.3 and 1.4.

The full logs, emulator states, images, and executables from the working archive are not part of the publication. The scope of the historical tests is explained in [Status](STATUS.md).

## Publication checks

The selected sources were checked against the 1.4 package using SHA-256. The game code, engine code, and linker limits retain the contents of that reference. The repository adds documentation and preparation and verification tools.

The importer validated the **199 external files**, totaling **198,677,280 bytes**. The published set does not include any of those files. Headers and source files with embedded data were also reviewed, as were file extensions.

The tool tests cover incomplete or corrupt imports, conflicts with local edits, paths and symbolic links, repeated imports, and rejection of a toolchain without working LTO before cleaning an earlier executable. The results of this review are summarized in [`publication-checks.json`](evidence/publication-checks.json).

## Rebuilding on Linux

The rebuild was completed with GCC 13.2.0 `sh2eb-elf`, a dynamic Binutils 2.40 linker, and the GCC 13.2.0 shared plugin built from official sources. The setup is described in [Building](BUILDING.md).

The initial Linux package accepted `-flto`, but did not finish removing engine code that the game did not use. Linking exceeded the memory limit. The build environment was corrected by retaining the original compiler and adding plugin support. The stack was not moved and the limit was not increased to make the build pass.

The preflight test checks optimization across two files and removal of unneeded symbols. The full link retains the memory assertion: `__jo_work_end` is at `0x060E89D0`, below the stack start at `0x060FBC00`. The 78,384-byte distance is part of the program's memory layout; it is not a measurement of peak stack usage.

| Rebuilt-image check | Result |
| --- | --- |
| Build products | ELF, map, flat executable, ISO, BIN, and CUE generated. |
| ISO filesystem files | 66, matching the expected contents; no missing or added files, and no files with different data. |
| Data sectors | 96,401; all 2048-byte blocks match between ISO and BIN. |
| MODE1/2352 structure | Headers, EDC, and P/Q parity checked; 0 defective sectors. |
| CUE | One MODE1/2352 data track, with the expected filename. |

This build emitted **one warning** about possible use of uninitialized `m` in `jo_engine/vdp2.c`, inside an engine rotation-matrix function. The publication retains the original source and records the warning; the Linux rebuild is not presented as a build without warnings. That function does not appear as a symbol in the linked executable.

### Difference from the original release

| Flat executable | Size | SHA-256 |
| --- | ---: | --- |
| 1.4 release reference | 554,960 bytes | `cce6b48eae25c755c238ded62dcbf02d17550737b718f30e6c1f67c1dba433b1` |
| Linux rebuild | 554,576 bytes | `73e50c32b2d074f2e5434636631e69e429725efb3befd756638a224c45b9af26` |

The Linux executable is 384 bytes smaller. **It is not byte-for-byte identical to the reference.** The sources have been preserved, but the configuration or code-generation difference that accounts for those bytes has not been isolated. Nor is the difference attributed solely to ISO timestamps, because the comparison here is between flat executables.

The Linux rebuild has not been run on a physical Saturn and is not presented as a new release validated on a console. The confirmation in the first section applies to the original BIN. This check aims to demonstrate that the published tree, with its local dependencies and documented toolchain, builds an intact image. Functional acceptance of a new build requires its own tests.

## Remaining work

Complete regeneration from the arcade ROM and documentation of playthroughs and physical-hardware sessions with a specific scope remain pending. Any additional evidence must identify the image tested, its hash, the equipment, and the procedure. Outstanding game work is in [Status](STATUS.md); outstanding asset-preparation work is in [Assets and provenance](RESOURCES.md).
