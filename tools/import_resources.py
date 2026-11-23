#!/usr/bin/env python3
"""Import only the pinned private build inputs from a local source archive.

The manifest contains identities, not resource content. No network access is used.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "resources/manifest.json"
CHUNK_BYTES = 1024 * 1024


class ResourceError(Exception):
    """An input failed validation; existing resources must be preserved."""


def _relative_path(value):
    if not isinstance(value, str) or not value or "\\" in value or ":" in value:
        raise ResourceError(f"Ruta no válida: {value!r}")
    if any(ord(char) < 32 or ord(char) == 127 for char in value):
        raise ResourceError(f"Ruta con caracteres de control: {value!r}")
    path = PurePosixPath(value)
    if path.is_absolute() or path.as_posix() != value or any(p in (".", "..") for p in path.parts):
        raise ResourceError(f"Ruta fuera del formato permitido: {value!r}")
    if any(part.endswith((" ", ".")) for part in path.parts):
        raise ResourceError(f"Ruta ambigua: {value!r}")
    return path


def load_manifest(path=MANIFEST):
    try:
        manifest = json.loads(Path(path).read_text(encoding="utf-8"))
        if manifest["schema_version"] != 1:
            raise ResourceError("Versión de manifiesto no compatible")
        archive = manifest["source_archive"]
        _relative_path(archive["root"])
        entries = manifest["files"]
        if not entries:
            raise ResourceError("El manifiesto no contiene recursos")
        seen = set()
        for entry in [archive, *entries]:
            if not isinstance(entry["size"], int) or isinstance(entry["size"], bool) or entry["size"] < 0:
                raise ResourceError("Tamaño no válido en el manifiesto")
            if not re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]):
                raise ResourceError("SHA-256 no válido en el manifiesto")
        for entry in entries:
            _relative_path(entry["path"])
            key = entry["path"].casefold()
            if key in seen:
                raise ResourceError(f"Ruta duplicada en el manifiesto: {entry['path']}")
            seen.add(key)
        return manifest
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise ResourceError(f"No se pudo leer el manifiesto {path}: {error}") from error


def _root_directory(path):
    path = Path(path).expanduser()
    if path.is_symlink() or not path.is_dir():
        raise ResourceError(f"Se necesita un directorio real, sin enlace simbólico: {path}")
    return path.resolve()


def _resource_path(root, relative):
    """Check every existing component before reading or creating a resource."""
    parts = _relative_path(relative).parts
    current = root
    for index, part in enumerate(parts):
        current = current / part
        try:
            mode = current.lstat().st_mode
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(mode):
            raise ResourceError(f"No se permiten enlaces simbólicos: {current}")
        if index < len(parts) - 1 and not stat.S_ISDIR(mode):
            raise ResourceError(f"Un componente de la ruta no es un directorio: {current}")
        if index == len(parts) - 1 and not stat.S_ISREG(mode):
            raise ResourceError(f"El recurso no es un archivo regular: {current}")
    if not current.resolve().is_relative_to(root):
        raise ResourceError(f"La ruta sale del directorio permitido: {relative}")
    return current


def _verify_stream(stream, entry, label, output=None):
    digest = hashlib.sha256()
    length = 0
    while block := stream.read(CHUNK_BYTES):
        length += len(block)
        if length > entry["size"]:
            raise ResourceError(f"Tamaño distinto del esperado: {label}")
        digest.update(block)
        if output is not None:
            output.write(block)
    if length != entry["size"] or digest.hexdigest() != entry["sha256"]:
        raise ResourceError(f"El recurso no coincide con Super Pang Saturn 1.4: {label}")


@contextmanager
def _source_reader(source, manifest):
    source = Path(source).expanduser()
    if source.is_symlink():
        raise ResourceError(f"El origen no puede ser un enlace simbólico: {source}")
    if source.is_dir():
        directory = _root_directory(source)

        def read_directory(entry):
            path = _resource_path(directory, entry["path"])
            if not path.is_file():
                raise ResourceError(f"Falta el recurso de origen: {entry['path']}")
            return path.open("rb")

        yield read_directory
        return
    if not source.is_file():
        raise ResourceError(f"No existe el ZIP o directorio indicado: {source}")
    with source.open("rb") as raw:
        _verify_stream(raw, manifest["source_archive"], str(source))
        raw.seek(0)
        with zipfile.ZipFile(raw) as archive:
            members = {}
            seen = set()
            for entry in archive.infolist():
                name = entry.filename
                canonical = name[:-1] if entry.is_dir() else name
                _relative_path(canonical)
                folded = canonical.casefold()
                if folded in seen:
                    raise ResourceError(f"Ruta duplicada o ambigua en el ZIP: {name}")
                seen.add(folded)
                mode = stat.S_IFMT(entry.external_attr >> 16)
                if mode not in (0, stat.S_IFREG, stat.S_IFDIR):
                    raise ResourceError(f"Enlace o archivo especial en el ZIP: {name}")
                if mode == stat.S_IFDIR and not entry.is_dir():
                    raise ResourceError(f"Directorio ZIP con nombre ambiguo: {name}")
                members[name] = entry
            prefix = manifest["source_archive"]["root"] + "/"
            for resource in manifest["files"]:
                member = members.get(prefix + resource["path"])
                if member is None or member.is_dir():
                    raise ResourceError(f"Falta el recurso en el ZIP: {resource['path']}")
                if member.file_size != resource["size"]:
                    raise ResourceError(f"Tamaño distinto en el ZIP: {resource['path']}")

            def read_archive(entry):
                return archive.open(members[prefix + entry["path"]], "r")

            yield read_archive


def _destinations(root, manifest, *, require_all):
    missing = []
    for entry in manifest["files"]:
        path = _resource_path(root, entry["path"])
        if not path.exists():
            missing.append(entry)
            continue
        try:
            with path.open("rb") as stream:
                _verify_stream(stream, entry, entry["path"])
        except ResourceError as error:
            raise ResourceError(
                f"Archivo local modificado; se conserva sin sobrescribir: {entry['path']}. "
                "Guarda o mueve tu edición antes de volver a importar."
            ) from error
    if require_all and missing:
        names = ", ".join(entry["path"] for entry in missing[:5])
        raise ResourceError(
            f"Faltan {len(missing)} recursos: {names}. "
            "Ejecuta python3 tools/import_resources.py /ruta/SuperPang_1.4_FUENTES.zip"
        )
    return missing


def check_resources(root=ROOT, manifest=None):
    manifest = load_manifest() if manifest is None else manifest
    try:
        root = _root_directory(root)
        _destinations(root, manifest, require_all=True)
        return {"files": len(manifest["files"]),
                "bytes": sum(entry["size"] for entry in manifest["files"])}
    except OSError as error:
        raise ResourceError(f"No se pudieron comprobar los recursos: {error}") from error


def import_resources(source, root=ROOT, manifest=None, *, dry_run=False):
    manifest = load_manifest() if manifest is None else manifest
    try:
        root = _root_directory(root)
        with _source_reader(source, manifest) as read:
            # Validate the entire allowlist and all conflicts before installation.
            for entry in manifest["files"]:
                with read(entry) as stream:
                    _verify_stream(stream, entry, entry["path"])
            missing = _destinations(root, manifest, require_all=False)
            result = {"imported": 0 if dry_run else len(missing),
                      "pending": len(missing) if dry_run else 0,
                      "unchanged": len(manifest["files"]) - len(missing),
                      "files": len(manifest["files"])}
            if dry_run or not missing:
                return result
            # A second pass stages only missing, allowlisted files. Recheck bytes
            # in case a directory input changed after validation. Staging on the
            # same filesystem allows atomic, non-overwriting hard-link installs.
            with tempfile.TemporaryDirectory(prefix=".spang-import-", dir=root) as temp:
                stage = Path(temp)
                for index, entry in enumerate(missing):
                    with read(entry) as stream, (stage / str(index)).open("xb") as output:
                        _verify_stream(stream, entry, entry["path"], output)
                created_files = []
                created_dirs = []
                try:
                    for index, entry in enumerate(missing):
                        target = _resource_path(root, entry["path"])
                        parents = []
                        parent = target.parent
                        while parent != root and not parent.exists():
                            parents.append(parent)
                            parent = parent.parent
                        for parent in reversed(parents):
                            parent.mkdir()
                            created_dirs.append(parent)
                        _resource_path(root, entry["path"])
                        os.link(stage / str(index), target)
                        created_files.append(target)
                except BaseException:
                    for path in reversed(created_files):
                        path.unlink()
                    for path in reversed(created_dirs):
                        path.rmdir()
                    raise
            return result
    except ResourceError:
        raise
    except (OSError, zipfile.BadZipFile, NotImplementedError, RuntimeError) as error:
        raise ResourceError(f"No se pudieron importar los recursos: {error}") from error


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", type=Path,
                        help="Tu ZIP SuperPang_1.4_FUENTES.zip o su directorio native320_r09")
    parser.add_argument("--check", action="store_true", help="Verifica los recursos ya instalados")
    parser.add_argument("--dry-run", action="store_true", help="Valida el origen sin instalar archivos")
    args = parser.parse_args(argv)
    if args.check and (args.source is not None or args.dry_run):
        parser.error("--check se usa sin origen y sin --dry-run")
    if not args.check and args.source is None:
        parser.error("indica tu ZIP/directorio de origen o usa --check")
    try:
        if args.check:
            result = check_resources()
            print(f"Recursos verificados: {result['files']} archivos, {result['bytes']:,} bytes.")
        else:
            result = import_resources(args.source, dry_run=args.dry_run)
            if args.dry_run:
                print(f"Validación correcta: {result['pending']} por importar; "
                      f"{result['unchanged']} ya coinciden. No se ha instalado ningún recurso.")
            else:
                print(f"Importación completa: {result['imported']} archivos nuevos; "
                      f"{result['unchanged']} ya coincidían.")
        return 0
    except ResourceError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
