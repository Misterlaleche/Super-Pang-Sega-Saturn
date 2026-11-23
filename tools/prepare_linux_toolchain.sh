#!/usr/bin/env bash
# Native Linux prerequisites: bash, Python 3, GCC/G++, make, binutils
# (ar, ranlib, strip), tar, xz, and the usual development libc headers.
# No root access, package installation, or modification of the base toolchain.
# Usage: prepare_linux_toolchain.sh ORIGINAL_TOOLCHAIN OUTPUT_TOOLCHAIN
# Optional build parent: SPANG_TOOLCHAIN_BUILD_DIR=/tmp/my-build SPANG_TOOLCHAIN_JOBS=8
set -euo pipefail

if [[ $# -ne 2 ]]; then
    printf 'Usage: %s ORIGINAL_TOOLCHAIN OUTPUT_TOOLCHAIN\n' "$0" >&2
    exit 2
fi
if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
    printf 'This overlay is for the supplied x86_64 Linux Yaul toolchain.\n' >&2
    exit 2
fi
for required in python3 gcc g++ make ar ranlib strip tar xz realpath; do
    command -v "$required" >/dev/null || {
        printf 'Missing native build prerequisite: %s\n' "$required" >&2
        exit 2
    }
done
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if [[ ! "$script_dir" =~ ^[A-Za-z0-9_./+\-]+$ ]]; then
    printf 'Unsupported characters in script path: %s\n' "$script_dir" >&2
    exit 2
fi
jobs=${SPANG_TOOLCHAIN_JOBS:-8}
if [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
    printf 'SPANG_TOOLCHAIN_JOBS must be a positive integer.\n' >&2
    exit 2
fi
# Validation happens before any mkdir, download, or extraction. The configured
# build directory is a parent only: the helper allocates a new private child.
work_dir=$(python3 "$script_dir/install_overlay.py" --prepare-workdir \
    "$1" "$2" "${SPANG_TOOLCHAIN_BUILD_DIR:-/tmp}")
base_dir=$(realpath -- "$1")
output_dir=$(realpath -m -- "$2")
stage=downloads
trap 'printf "Toolchain preparation failed during %s. Logs and sources: %s\n" "$stage" "$work_dir" >&2' ERR
printf 'Building Linux LTO support in %s\n' "$work_dir"
python3 - "$work_dir" <<'PY'
import hashlib
from pathlib import Path
import sys
import urllib.request

work = Path(sys.argv[1])
archives = [
    ("https://ftp.gnu.org/gnu/binutils/binutils-2.40.tar.xz", "0f8a4c272d7f17f369ded10a4aca28b8e304828e95526da482b0ccc4dfc9d8e1"),
    ("https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz", "e275e76442a6067341a27f04c5c6b83d8613144004c0413528863dc6b5c743da"),
]
for url, expected in archives:
    target = work / url.rsplit("/", 1)[-1]
    if not target.exists():
        temporary = target.with_suffix(target.suffix + ".part")
        with urllib.request.urlopen(url, timeout=60) as source, temporary.open("wb") as output:
            while block := source.read(1024 * 1024):
                output.write(block)
        temporary.replace(target)
    digest = hashlib.sha256()
    with target.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    if digest.hexdigest() != expected:
        raise SystemExit("SHA-256 mismatch; refusing to extract " + str(target))
    print("Verified", target.name, digest.hexdigest(), flush=True)
PY

stage=extraction
# The private work directory must still have no preexisting extraction trees.
for extraction_name in binutils-2.40 gcc-13.2.0 build-binutils build-libiberty build-lto-plugin prefix; do
    if [[ -e "$work_dir/$extraction_name" || -L "$work_dir/$extraction_name" ]]; then
        printf 'Refusing to reuse an extraction path: %s\n' "$work_dir/$extraction_name" >&2
        exit 2
    fi
done
tar -xf "$work_dir/binutils-2.40.tar.xz" -C "$work_dir"
# Only the host LTO plugin, its matching PIC libiberty, and build support
# are needed from GCC. The supplied compiler backend is preserved.
tar -xf "$work_dir/gcc-13.2.0.tar.xz" -C "$work_dir" \
    gcc-13.2.0/libiberty gcc-13.2.0/lto-plugin gcc-13.2.0/include \
    gcc-13.2.0/config gcc-13.2.0/configure gcc-13.2.0/config.guess \
    gcc-13.2.0/config.sub gcc-13.2.0/install-sh gcc-13.2.0/ltmain.sh \
    gcc-13.2.0/ltversion.m4 gcc-13.2.0/libtool.m4 gcc-13.2.0/ltoptions.m4 \
    gcc-13.2.0/ltsugar.m4 gcc-13.2.0/lt~obsolete.m4 gcc-13.2.0/missing \
    gcc-13.2.0/mkinstalldirs gcc-13.2.0/depcomp gcc-13.2.0/move-if-change \
    gcc-13.2.0/COPYING gcc-13.2.0/COPYING.LIB gcc-13.2.0/COPYING3 \
    gcc-13.2.0/COPYING3.LIB gcc-13.2.0/gcc/BASE-VER gcc-13.2.0/gcc/lto/common.h
mkdir -p "$work_dir/build-binutils" "$work_dir/build-libiberty" "$work_dir/build-lto-plugin" "$work_dir/prefix"

stage=binutils
printf 'Building Binutils 2.40 with plugin support...\n'
(
    cd "$work_dir/build-binutils"
    CC=gcc CXX=g++ ../binutils-2.40/configure --target=sh2eb-elf \
        --prefix="$work_dir/prefix" --enable-plugins --disable-nls \
        --disable-werror --disable-gdb --disable-gdbserver --disable-gprofng \
        --disable-sim --disable-gas >configure.log 2>&1
    make -j"$jobs" MAKEINFO=true all-ld all-binutils >build.log 2>&1
    make MAKEINFO=true install-ld install-binutils >install.log 2>&1
)

stage=libiberty
printf 'Building GCC 13.2.0 PIC libiberty...\n'
(
    cd "$work_dir/build-libiberty"
    CC=gcc ../gcc-13.2.0/libiberty/configure --enable-shared \
        --enable-host-shared --disable-nls >configure.log 2>&1
    make -j"$jobs" >build.log 2>&1
)

stage=lto-plugin
printf 'Building the GCC 13.2.0 shared LTO plugin...\n'
(
    cd "$work_dir/build-lto-plugin"
    CC=gcc ../gcc-13.2.0/lto-plugin/configure --prefix="$work_dir/prefix" \
        --target=sh2eb-elf --enable-shared --disable-static \
        --with-libiberty="$work_dir/build-libiberty" >configure.log 2>&1
    make -j"$jobs" >build.log 2>&1
    make install >install.log 2>&1
)

stage=overlay-installation
python3 "$script_dir/install_overlay.py" "$base_dir" "$work_dir/prefix" "$output_dir"
printf 'Prepared toolchain: %s\nSet SATURN_TOOLCHAIN_ROOT to that directory.\n' "$output_dir"
printf 'Build sources and logs retained at: %s\n' "$work_dir"
