# Building Super Pang Saturn 1.4

**English** | [Español](COMPILAR.md)

This repository contains the port's code, the version of Jo Engine it uses,
and the project's build tools. Graphics, audio, game data banks, and the binary
boot template are imported from a local copy of `SuperPang_1.4_FUENTES.zip`
that you already have and are entitled to use. The repository does not provide
that archive or download game assets.

**The currently available build path requires the private 1.4 source archive.**
A copy of the arcade ROM `spang.zip` alone is not enough to rebuild all the
assets for this version. The importer retrieves prepared assets; it does not
regenerate them from the ROM.

## Requirements

- Python 3.11 or later.
- A host C compiler available as `cc`, plus `make` and `sh`.
- `xorriso`/`xorrisofs` or `mkisofs`, available in `PATH`.
- GCC **13.2.0** targeting **`sh2eb-elf`**, with LTO and a working linker plugin,
  plus `sh2eb-elf-objcopy`, `sh2eb-elf-nm`, and `sh2eb-elf-objdump`. The project
  builds for big-endian SH-2 with `-m2 -mb`.
- Your local `SuperPang_1.4_FUENTES.zip` archive or its extracted
  `native320_r09` directory.
- Space for roughly 199 MB of local assets, a temporary copy of the files being
  imported, and the build outputs, including ISO and BIN images.

The reference workflow uses Linux. Building on macOS or Windows has not been
validated here. The compiler executable must match the system on which you run
it. A Saturn BIOS and emulators are not build requirements; they are used for
running and testing the result afterward.

The inherited Makefile does not support spaces or certain special characters
in the repository or toolchain path. The launcher checks these paths before
cleaning an earlier build. Use ASCII letters, digits, `/`, `_`, `-`, `.`, and
`+`. The ZIP path may contain spaces if you quote it.

### GCC accepting `-flto` is not enough

The engine depends on the linker removing code that the game does not use.
A GCC package can report version 13.2.0, accept `-flto`, and perform some
cross-file optimizations, yet lack the plugin that tells the compiler which
symbols the executable actually needs. Such a package does not meet this
project's requirements: it may retain enough engine code to exceed the
linker's memory limit.

`tools/build.py` compiles and links a temporary test consisting of two C files
before running `make clean`. It checks for a big-endian ELF32 SuperH executable,
preservation of the entry point, removal of two unnecessary functions, and
cross-file resolution of `scale(7)` to 42. It accepts LTO objects that also
contain conventional code, provided linking actually uses their LTO
representation and passes these checks. The test does not execute SH-2
instructions.

To prepare your own toolchain, you need GCC 13.2.0 with `--enable-lto` and a
linker with plugin support. The GCC plugin must be built for the architecture
of the **host running the compiler** and must be loadable by that linker.
Indiscriminately disabling host shared libraries may leave only
`liblto_plugin.a`, which does not provide this capability. GCC documents the
plugin configuration options and the selection of a compatible linker. See
the [GCC 13.2 LTO options](https://gcc.gnu.org/onlinedocs/gcc-13.2.0/gcc/Optimize-Options.html)
and the [official GCC configuration guide](https://gcc.gnu.org/install/configure.html).
A Linux 13.2.0 package lacking this working support was found during repository
preparation. The preflight check rejects it. The build was completed by keeping
the compiler and adding a compatible dynamically linked linker and plugin,
without changing the game code or its memory limits.

### Preparing the inherited Linux package

If you have the **Yaul GCC 13.2.0 package for Linux x86_64** with the static
linker described above, the repository includes a tool to create another copy
with working LTO support:

```sh
bash tools/prepare_linux_toolchain.sh \
  /path/to/sh2eb-elf-original \
  "$PWD/toolchain/sh2eb-elf-lto"

export SATURN_TOOLCHAIN_ROOT="$PWD/toolchain/sh2eb-elf-lto"
```

It requires host GCC and G++, Make, the host binutils tools, Python 3, Bash,
`tar`, `xz`, and the C library development headers. The output directory must
be new. It does not install system packages or require administrator
privileges.

The script downloads the official **Binutils 2.40** and **GCC 13.2.0** sources,
checks their SHA-256 hashes, and builds the linker, `libiberty`, and the LTO
plugin. It keeps the compiler, assembler, and target libraries from the
original package. The new copy configures GCC's link instructions to use the
plugin; the original package remains intact.

Allow at least 2 GB for the sources and temporary build, in addition to space
for the output copy. Work takes place under `/tmp` by default. You can choose
another parent directory with `SPANG_TOOLCHAIN_BUILD_DIR` and limit the number
of build processes with `SPANG_TOOLCHAIN_JOBS`, for example
`SPANG_TOOLCHAIN_JOBS=4`. Logs are kept in the temporary directory reported
when the script finishes.

This tool is intended for that inherited package. If your GCC already passes
the LTO test, use its directory directly with `SATURN_TOOLCHAIN_ROOT`.
The origins and hashes of the GNU sources are fixed in the script.

## 1. Import the local assets

From the repository root:

```sh
python3 tools/import_resources.py /path/to/SuperPang_1.4_FUENTES.zip
python3 tools/import_resources.py --check
```

You can also specify the extracted directory, without changing its contents:

```sh
python3 tools/import_resources.py /path/to/native320_r09
```

To check the source and possible conflicts without installing assets:

```sh
python3 tools/import_resources.py --dry-run /path/to/SuperPang_1.4_FUENTES.zip
```

The reference ZIP has these properties:

| Property | Value |
|---|---|
| Original name | `SuperPang_1.4_FUENTES.zip` |
| Size | 185,292,771 bytes |
| SHA-256 | `aec07b4b6b3ffb6fc95365e1ebd96c3707bc5d30945a3b283fd4f881a8da8f9e` |
| Asset directory inside the ZIP | `native320_r09` |

The ZIP's name may change; its identity is checked by size and SHA-256 hash.
Recompressing it changes its identity even if it contains the same assets.
For an extracted copy, each required file is checked separately.

### What is imported

The complete list, with the size, SHA-256 hash, and purpose of each entry, is
in [`resources/manifest.json`](../resources/manifest.json). It does not contain
the asset data.

| Group | Files | Bytes |
|---|---:|---:|
| Audio used by the game | 53 | 195,829,852 |
| Graphics read from disc | 7 | 601,847 |
| Generated data compiled with the code | 11 | 1,575,778 |
| C/newlib headers from the inherited environment | 127 | 663,659 |
| `Compiler/COMMON/IP.BIN` template | 1 | 6,144 |
| **Total** | **199** | **198,677,280** |

The seven runtime graphics files are `AUTH4.BIN`, `CELLH4.BIN`, `CELLT4.BIN`,
`CHARDF.BIN`, `CHARMDF.BIN`, `VIEWX.BIN`, and `VIEWY.BIN`, all in `cd/ART`.
The `audio/catalog.h` catalog references the 53 files in `cd/AUDIO`.

Generated data includes level tables, interface banks, audio and sprite
catalogs, collision profiles, and compiled pixels for the title screen, items,
and start and pause notices. Being written as `.c`, `.h`, or `.inc` files does
not change their origin; they are therefore supplied locally as well.

The headers in `Compiler/WINDOWS/sh-elf/include` are also used when building
on Linux: the directory name comes from the inherited SDK. The Makefile
explicitly adds them to the header search path. The reference toolchain does
not include all the C headers this code needs on its own.

The importer checks **all** assets and existing files before installing the
missing ones. It copies only the listed entries; it does not extract the ROM,
a BIOS, screenshots, old evidence, or other archive contents. Running it again
with the same assets preserves files already installed. If a destination file
contains a local edit, it stops without overwriting it. It also rejects
ambiguous paths, duplicate ZIP entries, and symbolic links in the paths used.

Imported assets are excluded from Git tracking. Their local presence makes
building possible; it does not mean they are part of the published repository.

## 2. Build

Specify the directory containing `bin/sh2eb-elf-gcc`:

```sh
export SATURN_TOOLCHAIN_ROOT=/path/to/sh2eb-elf
python3 tools/build.py
```

The launcher first validates the 199 assets, the compiler version and target,
LTO linking, and the presence of the host tools. It then creates the Jo Engine
options file if missing and runs a clean build: `make clean`, followed by
`sh build.sh`. It clears Make options inherited through `MAKEFLAGS`, `MFLAGS`,
and `MAKEOVERRIDES` so that they cannot turn the build into a dry run or
silently override its variables.

The build chain is:

1. `cc` compiles `JoEngineCueMaker.c` and `JoEngineIsoToBin.c` for the host.
2. GCC compiles and links the C code and SH-2 startup code, with LTO.
3. `objcopy` generates the binary executable and `cd/0.bin`.
4. `ip.S` builds the disc header using the local `IP.BIN` template.
5. `mkisofs` or `xorrisofs` packages the executable and assets from the `cd`
   directory into an ISO.
6. `JoEngineIsoToBin` converts its sectors to MODE1/2352, and the CUE is written.

The outputs are in the repository root:

| File | Purpose |
|---|---|
| `game.elf` | Linked executable for inspection and debugging |
| `game.map` | Memory layout of sections and symbols |
| `game.raw` | Flat executable |
| `game.iso` | Data track with 2048-byte sectors |
| `game.bin` | Image with MODE1/2352 sectors |
| `game.cue` | Track description for tools and emulators |

Music and sound effects are files on the data track. This disc does not use
CD-DA tracks. Dates, ISO packaging, and the executable may vary with the build
environment. The [validation report](VALIDATION_1.4.md) records the differences
observed between the Linux rebuild and the original release. A byte-for-byte
identical reproduction is not promised.

## 3. Check the result

The importer checks use small artificial samples and do not require the game
assets:

```sh
python3 -m unittest discover -s tools/tests -v
```

After building, verify the disc contents and sectors:

```sh
python3 tools/verify_disc.py
cc -O2 tools/verify_cd_raw.c -o /tmp/spang-verify-cd
/tmp/spang-verify-cd game.bin
```

These checks verify structure and integrity. Runtime tests in an emulator or
on a physical Saturn are a separate phase; a successful build does not replace
them. For the scope of the release tests, see the relevant documentation and
reports in the repository.

## 4. Load or burn the image

Keep `game.bin` and `game.cue` in the same directory and open the **CUE**. That
file specifies the image name and sector type. If you rename the BIN, also
update the CUE's `FILE` line.

- **Mednafen:** configure a BIOS you are entitled to use and load `game.cue`.
  Emulator instructions are in its [Saturn documentation](https://mednafen.github.io/documentation/ss.html).
  The project's reference test used Mednafen 1.32.1; other emulators or
  configurations require their own checks.
- **Terraonion MODE:** copy the BIN and CUE together to the device's storage,
  inside a directory for the game, and select that image using your Saturn's
  usual configuration. The repository does not update firmware or modify the
  device.
- **CD-R:** use your application's burn-image function and select the CUE.
  Copying both files as documents onto a data CD produces a different disc.
  Booting homebrew software depends on the console's configuration; this image
  does not include a procedure for modifying it.

These instructions describe how to use the files. Physical-hardware
confirmation of 1.4 does not amount to verified compatibility with every
Saturn, burner, medium, or ODE.

## Why the arcade ROM is still not enough

The historical asset-creation scripts do not currently form a self-contained
pipeline that starts with `spang.zip` and produces every file for this version:

- Level preparation and some graphics depend on `c13_reference/fuentes`,
  containing tools, decoded data, and reference states. There are also
  references to earlier artwork in `art320/target_v001`.
- The audio exporters use a C13-version disc or ZIP, with its catalogs and
  manifests. They do not synthesize all the music directly from the arcade ROM.
- This version's title screen comes from a Mega Pang v0.3 screenshot and has
  a specific adaptation; it is not obtained from `spang.zip`.
- Some pixel and effect adaptations were established using assets and
  screenshots reviewed in earlier stages.

A future path based solely on original assets supplied by the user requires
completing, documenting, and checking those converters. The path described
here preserves the prepared 1.4 assets through their hashes and allows code
changes to be built without claiming that this complete reconstruction
already exists.

## Common problems

| Message or situation | What to check |
|---|---|
| ZIP does not match | Use the specified 1.4 archive; if you only have an extracted copy, pass `native320_r09` and let its assets be checked. |
| Missing asset | The extraction is incomplete, or you have not run the importer yet. |
| Modified local file | Save or move that edit before importing again. The importer will not replace it. |
| Missing `stdint.h` | Check that the inherited headers were imported, and do not remove their path from the Makefile. |
| GCC version other than 13.2.0 or target other than `sh2eb-elf` | Check `SATURN_TOOLCHAIN_ROOT`; it must point to the required toolchain, not the host compiler. |
| LTO test fails, or GCC only accepts LTO objects containing conventional code | You need a toolchain with a working linker plugin. Version 13.2.0 alone does not guarantee that capability. |
| Unsupported path | Move the checkout or toolchain to a path without spaces or special characters. |
| Missing ISO tool | Install `xorriso`/`xorrisofs` or `mkisofs` and add it to `PATH`. |

The manifest specifies the exact assets for the published version. If you also
want to modify those assets, you will need your own authoring workflow and must
explicitly review the expected identities; the importer does not update the
manifest automatically.
