# Resources and provenance

**English** | [Español](RECURSOS.md)

## What a version 1.4 build contains

The executable is built from the port's C code and Jo Engine. Some game data is embedded in the executable, while other data is loaded from the CD. Separating out the `.bin` and `.wav` files is therefore not enough to prepare the public source tree: C source files and generated headers also contain tables and pixel data.

The precise inventory is in [`resources/manifest.json`](../resources/manifest.json). Each dependency has a path, size, and SHA-256 hash. The importer only copies the entries in that inventory from a local package; it does not download files or extract the entire ZIP.

| Group | External dependencies |
| --- | --- |
| Saturn boot | `Compiler/COMMON/IP.BIN`, the template used by `ip.S`. It is not a BIOS. |
| Game data | Stage, state, presentation, and collision tables; frontend data; audio catalog; graphics headers. These are kept locally under the names expected by the code. |
| CD graphics | `AUTH4.BIN`, `CELLH4.BIN`, `CELLT4.BIN`, `CHARDF.BIN`, `CHARMDF.BIN`, `VIEWX.BIN`, and `VIEWY.BIN`, in `cd/ART`. |
| CD audio | The 53 files in `cd/AUDIO` referenced by the version 1.4 catalog. |
| Build headers | The 127 headers in the `Compiler/WINDOWS/sh-elf/include` tree. Although the path says WINDOWS, this makefile also uses them when building on Linux. They retain their own notices. |

The seven runtime graphics files take up approximately 602 KB. The prepared audio takes up about 196 MB in decimal units and accounts for most of the disc's size. The image uses a single data track; those music files are not CDDA tracks.

The importer keeps the dependencies out of Git through `.gitignore`. The public source files retained from the package are listed separately in [`public-source-reference.json`](../resources/public-source-reference.json).

## Reproducible build process

The published build process is:

1. Have the source code from this repository and the specified toolchain available.
2. Provide the prepared resources from `SuperPang_1.4_FUENTES.zip` locally, with the permissions needed to use them.
3. Check the hashes, build, and verify the resulting image.

The importer also accepts the `native320_r09` directory extracted from that package. The name is a legacy one; its makefile identifies version 1.4.

The repository does not provide a download of that package or its resources. If a dependency is missing, the build must stop and identify it. Graphics, tables, or sounds are not replaced with empty files to produce an apparently valid image.

## Regenerating from the arcade: work still needed

The historical preparation pipeline still needs to be assembled before a complete build starting from a user-provided arcade ROM can be offered. These are the limitations found in the version 1.4 package:

| Retained tool | Historical dependency |
| --- | --- |
| [`stage/prepare_data.py`](../stage/prepare_data.py) | References to `c13_reference/fuentes`, extraction modules, decoded data, and attribute, character, and palette captures. |
| [`art/build_art.py`](../art/build_art.py) and later revisions | Material prepared in `art320/target_v001` and other authoring and revision data from earlier stages. |
| [`audio/build_assets.py`](../audio/build_assets.py) and the audio exporters | Resources from an earlier C13 release, along with catalogs and manifests from that stage. |
| [`art/frontend/megadrive_03/build_title.py`](../art/frontend/megadrive_03/build_title.py) | A screenshot of the PANG lettering from Mega Pang v0.3 beta and title metadata. That screenshot does not come from `spang.zip`. |

The source code for these converters is retained because it explains decisions made for the port: cell composition, graphics packing, sample preparation, and title adaptation. **They do not form a self-contained recipe that can run from start to finish using the current public files.** They must not be used to blindly regenerate the version 1.4 tree: some scripts also write code and resources.

The remaining work is to recover or replace the preparation dependencies, establish their versions and permissions, and verify a complete extraction from documented inputs. The result must be compared against the version 1.4 hashes. Until then, the guide is limited to rebuilding with prepared resources.

## Ownership of the materials

The inventory describes technical requirements and provenance. It is not a license for the resources. Converting an image, an audio sample, or a table to another format does not remove any rights that may apply to the source material. See [Rights and credits](RIGHTS_AND_CREDITS.md).
