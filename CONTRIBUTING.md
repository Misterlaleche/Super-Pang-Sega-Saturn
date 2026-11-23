# Contributing to Super Pang Saturn

**English** | [Español](CONTRIBUTING.es.md)

Bug reports and contributions are welcome in English or Spanish.

The reference for this repository is version 1.4. Improvements must be comparable with it, especially when they affect logic, collision order, or CD and audio operations.

## Reporting a problem

Open an issue with the game mode, stage, and steps needed to reach the failure. If you use your own build, include the commit and the hash of `game.bin`. On Linux, you can obtain it with `sha256sum game.bin`; on macOS, with `shasum -a 256 game.bin`.

For physical-hardware tests, specify the Saturn model and region, loading method, cartridges, and connected peripherals. For emulator tests, specify the name, version, and relevant configuration. A short recording can help describe a graphical anomaly, a sound, or a hang, but there is no need to attach a disc image.

Do not attach ROMs, BIOS files, music, complete asset dumps, or BIN/CUE images. Do not include private data from other play sessions or accounts either. A diagnostic log, a reproducible description, and the file hash are usually sufficient.

## Proposing changes

Describe the problem, the expected behavior, and what the patch changes. Keep fixes focused: a physics change, a graphics change, and a loading change are easier to check separately.

Do not replace routines with approximations without explaining the difference from the arcade game. If an optimization changes the order of operations, document why it preserves the result. For memory changes, record the limits and the worst case that has been tested.

Keep third-party notices and identify any new material and its provenance. The licensing situation is explained in [Rights and credits](docs/RIGHTS_AND_CREDITS.md); a contribution cannot grant rights to assets from the original game.

## Checks

The publication-tool tests do not need the game assets:

```sh
python3 -m unittest discover -s tools/tests -v
```

With local assets and the toolchain available:

```sh
python3 tools/import_resources.py --check
python3 tools/build.py
python3 tools/verify_disc.py
```

If the change affects the game, add the functional-test steps and results. Always distinguish checks on the computer, in the emulator, and on the console. A successful build does not demonstrate that a gameplay situation has been resolved.
