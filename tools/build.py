#!/usr/bin/env python3
"""Preflight the pinned resources and SH-2 compiler, then run a clean build."""
from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

from import_resources import ResourceError, check_resources, load_manifest


ROOT = Path(__file__).resolve().parents[1]


class BuildError(Exception):
    """Build prerequisites were not satisfied, or the build failed."""


def _make_path(path, label):
    # The inherited Makefile expands these paths into unquoted shell commands.
    # Refuse unsupported names instead of letting Make split or interpret them.
    if not re.fullmatch(r"[A-Za-z0-9_./+\-]+", str(path)):
        raise BuildError(
            f"{label} contiene espacios o caracteres que el Makefile heredado no admite: {path}. "
            "Usa una ruta con letras ASCII, cifras, /, _, -, . o +."
        )


def _compiler_output(executable, option):
    try:
        result = subprocess.run([str(executable), option], check=True, text=True,
                                capture_output=True, timeout=15)
    except (OSError, subprocess.SubprocessError) as error:
        raise BuildError(f"No se pudo ejecutar {executable}: {error}") from error
    return result.stdout.strip()


def verify_lto(compiler_root):
    """Require usable SH-2 LTO and linker-driven symbol internalization.

    Some GCC 13.2 packages accept -flto but have no usable linker plugin. They
    retain unused exported engine code and exceed the game's fixed RAM budget.
    Compile and link a two-translation-unit probe; never execute target code.
    """
    binaries = Path(compiler_root) / "bin"
    compiler = binaries / "sh2eb-elf-gcc"
    nm = binaries / "sh2eb-elf-nm"
    objdump = binaries / "sh2eb-elf-objdump"
    for executable in (nm, objdump):
        if not executable.is_file() or not os.access(executable, os.X_OK):
            raise BuildError(f"Falta una herramienta necesaria para comprobar LTO: {executable}")

    def run(arguments, directory):
        try:
            result = subprocess.run([str(argument) for argument in arguments], cwd=directory,
                                    check=True, text=True, capture_output=True, timeout=30)
            return result.stdout
        except (OSError, subprocess.SubprocessError) as error:
            diagnostic = getattr(error, "stderr", None) or str(error)
            raise BuildError(
                "El toolchain no supera la prueba de LTO con plugin de enlace. "
                "GCC 13.2.0 y -flto por sí solos no bastan; usa un toolchain completo "
                "con un plugin compatible. No se ha ejecutado make clean. "
                f"Detalle: {diagnostic.strip()}"
            ) from error

    with tempfile.TemporaryDirectory(prefix="spang-lto-") as temporary:
        directory = Path(temporary)
        (directory / "entry.c").write_text(
            "extern unsigned spang_lto_scale(unsigned);\n"
            "volatile unsigned spang_lto_observed;\n"
            "void spang_lto_entry(void) { spang_lto_observed = spang_lto_scale(7); for (;;) {} }\n",
            encoding="ascii",
        )
        (directory / "scale.c").write_text(
            "unsigned spang_lto_scale(unsigned x) { return x * 6; }\n"
            "unsigned spang_lto_unused(unsigned x) { return x + 999; }\n",
            encoding="ascii",
        )
        common = [compiler, "-O2", "-m2", "-mb", "-ffreestanding", "-fno-pic", "-flto=4"]
        for stem in ("entry", "scale"):
            # Production uses -flto with the compiler's default object format.
            # Fat objects are valid if the linker actually consumes their LTO
            # representation; the symbol and folding checks below prove that.
            run([*common, "-c", stem + ".c", "-o", stem + ".o"], directory)
        run([*common, "-nostdlib", "-Wl,--entry=_spang_lto_entry,-Ttext=0x06004000",
             "entry.o", "scale.o", "-o", "probe.elf"], directory)
        elf = (directory / "probe.elf").read_bytes()
        if len(elf) < 52 or elf[:6] != b"\x7fELF\x01\x02" or int.from_bytes(elf[18:20], "big") != 42:
            raise BuildError("La prueba LTO no produjo un ELF32 SuperH big endian válido")
        symbols = {line.split()[-1] for line in run([nm, "probe.elf"], directory).splitlines() if line.split()}
        if not {"_spang_lto_entry", "_spang_lto_observed"} <= symbols or {
            "_spang_lto_scale", "_spang_lto_unused"
        } & symbols:
            raise BuildError(
                "LTO no ha conservado los símbolos necesarios o no ha eliminado los prescindibles. "
                "Comprueba el plugin de enlace del toolchain. No se ha ejecutado make clean."
            )
        assembly = run([objdump, "-dr", "probe.elf"], directory)
        if not re.search(r"\bmov\s+#42,", assembly):
            raise BuildError("LTO no ha resuelto scale(7) entre unidades de compilación; revisa el toolchain")


def build(root=ROOT, *, toolchain_root=None):
    if sys.version_info < (3, 11):
        raise BuildError("Se necesita Python 3.11 o posterior")
    root = Path(root).resolve()
    _make_path(root, "La ruta del repositorio")
    try:
        resources = check_resources(root, load_manifest(root / "resources/manifest.json"))
    except ResourceError as error:
        raise BuildError(str(error)) from error
    if not toolchain_root:
        raise BuildError("Define SATURN_TOOLCHAIN_ROOT con la ruta al GCC 13.2.0 sh2eb-elf")
    compiler_root = Path(toolchain_root).expanduser().resolve()
    _make_path(compiler_root, "La ruta del compilador")
    compiler = compiler_root / "bin/sh2eb-elf-gcc"
    objcopy = compiler_root / "bin/sh2eb-elf-objcopy"
    for executable in (compiler, objcopy):
        if not executable.is_file() or not os.access(executable, os.X_OK):
            raise BuildError(f"Falta un ejecutable del toolchain o no tiene permiso de ejecución: {executable}")
    version = _compiler_output(compiler, "-dumpfullversion")
    target = _compiler_output(compiler, "-dumpmachine")
    if version != "13.2.0" or target != "sh2eb-elf":
        raise BuildError(f"Se necesita GCC 13.2.0 sh2eb-elf; se detectó GCC {version} para {target}")
    verify_lto(compiler_root)
    for command in ("cc", "make", "sh"):
        if shutil.which(command) is None:
            raise BuildError(f"Falta el programa del equipo: {command}")
    bundled_iso = root / "iso-runtime/usr/bin/xorrisofs"
    if not (bundled_iso.is_file() and os.access(bundled_iso, os.X_OK)) and not any(
        shutil.which(command) for command in ("xorrisofs", "xorriso", "mkisofs")
    ):
        raise BuildError("Instala xorriso/xorrisofs o mkisofs y asegúrate de que esté en PATH")
    if not (root / "build.sh").is_file() or not (root / "makefile").is_file():
        raise BuildError("El repositorio está incompleto: faltan build.sh o makefile")
    engine = root / "jo_engine"
    if not engine.is_dir():
        raise BuildError("El repositorio está incompleto: falta jo_engine")
    environment = dict(os.environ, SATURN_TOOLCHAIN_ROOT=str(compiler_root))
    # Ambient -n, -e, or command-line variable assignments must not turn a clean
    # release build into a dry run or silently override the checked-in settings.
    for key in ("MAKEFLAGS", "MFLAGS", "MAKEOVERRIDES"):
        environment.pop(key, None)
    print(f"Recursos verificados: {resources['files']} archivos. GCC {version}, {target}; prueba LTO superada.", flush=True)
    print("Compilación limpia: make clean y sh build.sh", flush=True)
    try:
        (engine / "jo_options.txt").touch(exist_ok=True)
        subprocess.run(["make", "clean"], cwd=root, env=environment, check=True)
        subprocess.run(["sh", "build.sh"], cwd=root, env=environment, check=True)
    except (OSError, subprocess.CalledProcessError) as error:
        raise BuildError(f"La compilación ha fallado: {error}") from error
    expected = ("game.elf", "game.raw", "game.map", "game.iso", "game.bin", "game.cue")
    for name in expected:
        path = root / name
        if not path.is_file() or path.stat().st_size == 0:
            raise BuildError(f"La compilación no produjo el archivo esperado: {name}")
    print("Generados: " + ", ".join(expected) + ".", flush=True)
    print("Comprueba el disco con python3 tools/verify_disc.py.", flush=True)


def main():
    try:
        build(toolchain_root=os.environ.get("SATURN_TOOLCHAIN_ROOT"))
        return 0
    except BuildError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
