#!/usr/bin/env python3
"""Append exact original frontend glyphs and palette banks to the R09 assets.

The existing glyph ordinals/pattern prefix, every sprite cell, all authored
textures, and every Tour scene stay unchanged. The native layout relocates
original glyph cells; it never draws a replacement font or rewrites wording.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import sys

sys.dont_write_bytecode = True
P = Path(__file__).resolve().parents[1]
ART = P / 'art'
CD = P / 'cd/ART'
OUT = ART / 'r10'
FRONT = P / 'frontend'
BASE = ART / 'baseline'
SOURCE_DATA_SHA = '9d460c57aac71d4ec9e7e8e6d361e259ff789fd7aeee7f545a60c688c63a09f2'
SOURCE_GLYPHS_SHA = 'd7488c5b86a111ae4a0bdc906214d71e9b93a6d4eb1e997a6ec53b42a264187e'
GLYPH_FILES = ('CHARIDX.BIN', 'CHAR4.BIN', 'CHARMAP.BIN', 'CHARD4.BIN', 'CHARZ4.BIN')
CELL_SPLIT_FILES = ('cd/ART/CELLH4.BIN', 'cd/ART/CELLT4.BIN')
CELL_SPLIT_PATTERNS = 1280
sha = lambda b: hashlib.sha256(bytes(b)).hexdigest()


def original_text(data, address):
    """FIXED116D source grammar, independently of the frontend controller."""
    pos = 0x38000 + address - 0x8000
    cells = {}
    while True:
        dest = int.from_bytes(data[pos:pos + 2], 'little'); pos += 3
        while True:
            code = data[pos]; pos += 1
            if code == 0:
                return cells
            if code == 0x2f:
                break
            assert 0xd000 <= dest <= 0xdffe and not dest & 1
            cells[(dest - 0xd000) // 2] = code
            dest = (dest & 0xff00) | ((dest + 2) & 255)


def wrap(items, width):
    """Wrap original (source-index, glyph-code) pairs, keeping every glyph.

    Spaces at line breaks are layout separators. A long original token may
    break after its existing comma/hyphen; no punctuation is inserted.
    """
    items = list(items)
    while items and items[0][1] == 32:
        items.pop(0)
    while items and items[-1][1] == 32:
        items.pop()
    lines = []
    while len(items) > width:
        spaces = [i for i in range(width + 1) if items[i][1] == 32]
        if spaces:
            count = spaces[-1]
            lines.append(items[:count]); items = items[count + 1:]
        else:
            punctuation = [i + 1 for i in range(width) if items[i][1] in (44, 45)]
            count = punctuation[-1] if punctuation else width
            lines.append(items[:count]); items = items[count:]
        while items and items[0][1] == 32:
            items.pop(0)
    if items:
        lines.append(items)
    return lines


def text_layout(data):
    warning = original_text(data, 0xa22c)
    rows = sorted(set(i // 64 for i in warning))
    heading = [(i, warning[i]) for i in sorted(warning) if i // 64 == rows[0]]
    warning_map = []
    for col, (index, code) in enumerate(heading):
        if code != 32:
            warning_map.append((index, (320 - len(heading) * 8) // 2 + col * 8, 24))
    body = []
    for row in rows[1:]:
        line = [(i, warning[i]) for i in sorted(warning) if i // 64 == row]
        if body:
            body.append((-1, 32))
        body.extend(line)
    lines = wrap(body, 38)
    assert len(lines) <= 15
    for row, line in enumerate(lines):
        for col, (index, code) in enumerate(line):
            if code != 32:
                warning_map.append((index, 8 + col * 8, 48 + row * 12))
    pointer = int.from_bytes(data[0x38064:0x38066], 'little')
    menu = original_text(data, pointer)
    menu_map = []
    for first, last, left in ((10, 34, 8), (37, 54, 176)):
        for row in (17, 19, 21, 23, 25):
            line = [(row * 64 + col, menu.get(row * 64 + col, 32)) for col in range(first, last)]
            lines = wrap(line, 17)
            assert len(lines) <= 2
            for extra, part in enumerate(lines):
                for col, (index, code) in enumerate(part):
                    if code != 32:
                        menu_map.append((index, left + col * 8, row * 8 - 8 + extra * 8))
    header = '/* Generated original-glyph placements; no font pixels or replacement wording. */\n'
    header += 'typedef struct {uint16_t source;int16_t x,y;} frontend_text_placement;\n'
    for name, items in (('warning', warning_map), ('mode', menu_map)):
        items.sort()
        assert len({i[0] for i in items}) == len(items)
        header += 'static const frontend_text_placement frontend_' + name + '_text[]={\n'
        header += ''.join(' {%d,%d,%d},\n' % item for item in items) + '};\n'
    (FRONT / 'layout_data.h').write_text(header)
    return dict(warning_glyphs=len(warning_map), mode_description_glyphs=len(menu_map),
                warning_body_lines=len(wrap(body, 38)),
                warning_text_source='BANK10:A22C', mode_text_source=f'BANK10:{pointer:04X}',
                method='Relocate unchanged original non-space glyphs, retaining order and punctuation')


def baseline():
    OUT.mkdir(parents=True, exist_ok=True); FRONT.mkdir(exist_ok=True); BASE.mkdir(exist_ok=True)
    path = OUT / 'r09_baseline.json'
    if not path.exists():
        mapping = (CD / 'CHARMAP.BIN').read_bytes()
        assert sha(mapping) == '490033533e5217d28f037f89ee57d2995db425f00476a80fcfdae85cedc7ae69'
        (BASE / 'r09_CHARMAP.BIN').write_bytes(mapping)
        before = json.loads((ART / 'manifest.json').read_text())
        (BASE / 'r09_manifest.json').write_text(json.dumps(before, indent=2) + '\n')
        unchanged = {}
        for directory in (CD, P / 'editable_art', P / 'stage/data'):
            for file in sorted(directory.rglob('*')):
                if file.is_file() and not (directory == CD and file.name in GLYPH_FILES):
                    unchanged[str(file.relative_to(P))] = sha(file.read_bytes())
        for name in ('stage/stage_data.c', 'stage/stage_data.h'):
            unchanged[name] = sha((P / name).read_bytes())
        record = dict(files={name: dict(bytes=(CD / name).stat().st_size, sha256=sha((CD / name).read_bytes()))
                             for name in GLYPH_FILES}, unchanged_files=unchanged)
        path.write_text(json.dumps(record, indent=2) + '\n')
    result = json.loads(path.read_text())
    for name, expected in result['unchanged_files'].items():
        if name in CELL_SPLIT_FILES:
            continue  # Repartitioned below against the unchanged complete catalog.
        assert sha((P / name).read_bytes()) == expected, ('Existing resource changed', name)
    return result


def repack_cells(prior, manifest):
    """Only change the storage boundary; every original byte and ordinal stays.

    The renderer can move the first1280 immutable patterns to ten valid256x128
    VDP1 allocations, retaining only the4096-byte dictionary in working RAM.
    """
    catalog = (CD / 'CELLD4.BIN').read_bytes()
    assert sha(catalog) == prior['unchanged_files']['cd/ART/CELLD4.BIN']
    assert len(catalog) == 235520
    # Existing split may be R09 or the new R10 split; either must reconstruct it.
    assert (CD / 'CELLH4.BIN').read_bytes() + (CD / 'CELLT4.BIN').read_bytes() == catalog
    boundary = 4096 + CELL_SPLIT_PATTERNS * 128
    files = {}
    for name, blob in (('CELLH4.BIN', catalog[:boundary]), ('CELLT4.BIN', catalog[boundary:])):
        (CD / name).write_bytes(blob)
        files[name] = dict(bytes=len(blob), sha256=sha(blob))
        manifest['files'][name].update(files[name])
    manifest['files']['CELLH4.BIN']['meaning'] = 'Exact4096-byte BE16 ordinal map plus first1280 original128-byte patterns; payload can reside in VDP1'
    manifest['files']['CELLT4.BIN']['meaning'] = 'Exact remaining528 original128-byte patterns; resident in working RAM; no gameplay disc reads'
    return dict(previous_head_patterns=1536, head_patterns=CELL_SPLIT_PATTERNS,
                head_bytes=boundary, tail_bytes=len(catalog) - boundary,
                pattern_payload_vdp1_bytes=CELL_SPLIT_PATTERNS * 128,
                dictionary_working_ram_bytes=4096,
                combined_sha256=sha(catalog), original_concatenation_preserved=True,
                original_ordinals_preserved=True, files=files,
                method='Storage split only; complete CELLD4.BIN and expanded CELLS.N4 unchanged')


def export_banks(data):
    header = '''#ifndef SPANG_FRONTEND_DATA_H
#define SPANG_FRONTEND_DATA_H
#include <stdint.h>
/* Exact original decoded DATA. Only newly owned palette banks12/13 are here;
 * the controller reuses existing stage tables for all its other banks. */
const uint8_t *frontend_data_bank(unsigned bank);
#endif
'''
    source = '#include "data.h"\n/* Generated from verified World900914 DATA; immutable source tables. */\n'
    identities = {}
    for bank in (12, 13):
        payload = data[0x10000 + bank * 0x4000:0x14000 + bank * 0x4000]
        assert len(payload) == 16384
        identities[str(bank)] = dict(bytes=len(payload), sha256=sha(payload))
        source += f'static const uint8_t frontend_bank{bank}[16384]={{\n'
        source += ''.join(' ' + ','.join(f'0x{v:02x}' for v in payload[i:i + 24]) + ',\n' for i in range(0, len(payload), 24))
        source += '};\n'
    source += 'const uint8_t *frontend_data_bank(unsigned bank)\n{\n switch(bank){case 12:return frontend_bank12;case 13:return frontend_bank13;default:return 0;}\n}\n'
    (FRONT / 'data.h').write_text(header); (FRONT / 'data.c').write_text(source)
    return identities


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, default=P.parent / 'c13_reference/fuentes')
    args = parser.parse_args()
    data_path = args.reference / 'build/wp00/decoded/data.bin'
    glyph_path = args.reference / 'build/wp02/native-renderer-assets/chars.n4'
    data, original = data_path.read_bytes(), glyph_path.read_bytes()
    assert len(data) == 0x50000 and sha(data) == SOURCE_DATA_SHA
    assert len(original) == 0x100000 and sha(original) == SOURCE_GLYPHS_SHA
    prior = baseline()
    old = (BASE / 'r09_CHARMAP.BIN').read_bytes()
    count, old_unique = struct.unpack_from('>2H', old)
    old_pairs = dict(struct.unpack_from('>2H', old, 4 + i * 4) for i in range(count))
    patterns = [None] * old_unique
    for code, ordinal in old_pairs.items():
        pattern = original[code * 32:(code + 1) * 32]
        assert patterns[ordinal] is None or patterns[ordinal] == pattern
        patterns[ordinal] = pattern
    assert all(pattern is not None for pattern in patterns)
    assert sha(b''.join(patterns)) == prior['files']['CHARD4.BIN']['sha256']
    # The original F01 source inventory owns the first4096 glyphs. Retaining
    # the complete small-font/menu/title range also covers original how-to and
    # selector commands without captured screenshots becoming runtime assets.
    frontend_codes = set(range(4096)) | {0x3fee, 0x3fef, 0x3ffe, 0x3fff}
    ordered = sorted(set(old_pairs) | frontend_codes)
    ordinals = {pattern: i for i, pattern in enumerate(patterns)}
    pairs = dict(old_pairs)
    for code in ordered:
        if code in pairs:
            continue
        pattern = original[code * 32:(code + 1) * 32]
        if pattern not in ordinals:
            ordinals[pattern] = len(patterns); patterns.append(pattern)
        pairs[code] = ordinals[pattern]
    pixels = b''.join(patterns)
    mapping = struct.pack('>2H', len(ordered), len(patterns)) + b''.join(struct.pack('>2H', code, pairs[code]) for code in ordered)
    sys.path.insert(0, str(P / 'Compiler/COMMON'))
    import jo_lz
    blocks = [jo_lz.compress(pixels[i:i + 4096]) for i in range(0, len(pixels), 4096)]
    offsets = [8 + 4 * (len(blocks) + 1)]
    for block in blocks:
        offsets.append(offsets[-1] + len(block))
    packed = struct.pack('>2I', len(pixels), 4096) + b''.join(struct.pack('>I', v) for v in offsets) + b''.join(blocks)
    assert b''.join(jo_lz.decompress(block) for block in blocks) == pixels
    blobs = {'CHARIDX.BIN': struct.pack('>H', len(ordered)) + b''.join(struct.pack('>H', c) for c in ordered),
             'CHAR4.BIN': b''.join(original[c * 32:(c + 1) * 32] for c in ordered),
             'CHARMAP.BIN': mapping, 'CHARD4.BIN': pixels, 'CHARZ4.BIN': packed}
    manifest = json.loads((BASE / 'r09_manifest.json').read_text())
    cell_repack = repack_cells(prior, manifest)
    for name, blob in blobs.items():
        (CD / name).write_bytes(blob)
        manifest['files'][name].update(bytes=len(blob), sha256=sha(blob))
    definitions = {'ART_GLYPH_CODE_COUNT': len(ordered), 'ART_GLYPH_UNIQUE_COUNT': len(patterns),
                   'ART_GLYPH_MAP_BYTES': len(mapping), 'ART_GLYPH_BYTES': len(pixels), 'ART_GLYPH_PACKED_BYTES': len(packed),
                   'ART_CELL_HEAD_BYTES': cell_repack['head_bytes'], 'ART_CELL_TAIL_BYTES': cell_repack['tail_bytes'],
                   'ART_CELL_HEAD_PATTERNS': CELL_SPLIT_PATTERNS}
    header = (ART / 'authored_index.h').read_text()
    for name, value in definitions.items():
        header, replacements = re.subn(r'#define ' + name + r' \d+u', f'#define {name} {value}u', header)
        assert replacements == 1
    (ART / 'authored_index.h').write_text(header)
    layout = text_layout(data)
    banks = export_banks(data)
    record = dict(version='r10_frontend_original_v1', source_data_sha256=SOURCE_DATA_SHA,
                  source_glyphs_sha256=SOURCE_GLYPHS_SHA, source_rom_set='Super Pang World900914',
                  baseline=prior['files'], unchanged_files={k: v for k, v in prior['unchanged_files'].items() if k not in CELL_SPLIT_FILES},
                  cell_catalog_repack=cell_repack,
                  frontend_glyph_codes=sorted(frontend_codes), added_codes=sorted(set(ordered) - set(old_pairs)),
                  old_glyph_ordinals_preserved=True, old_glyph_pattern_prefix_preserved=True,
                  added_unique_patterns=len(patterns) - old_unique,
                  added_resident_glyph_bytes=len(mapping) + len(packed) - prior['files']['CHARMAP.BIN']['bytes'] - prior['files']['CHARZ4.BIN']['bytes'],
                  glyph_codes=len(ordered), unique_patterns=len(patterns), pattern_bytes=len(pixels), packed_bytes=len(packed),
                  files={name: dict(bytes=len(blob), sha256=sha(blob)) for name, blob in blobs.items()},
                  new_rom_banks=banks, text_layout=layout,
                  pixel_method='Original 4bpp glyph pens unchanged; existing cell textures and their15-to0 transparency remap untouched',
                  scope='Source-derived assets and layout only; native rendering and gameplay verified separately')
    manifest['compact_character_count'] = len(ordered)
    manifest['r10_frontend'] = {k: v for k, v in record.items() if k not in ('unchanged_files', 'files', 'frontend_glyph_codes', 'added_codes')}
    (ART / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (OUT / 'frontend_manifest.json').write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps({k: record[k] for k in ('glyph_codes', 'unique_patterns', 'added_unique_patterns', 'added_resident_glyph_bytes', 'new_rom_banks', 'text_layout')}, indent=2))


if __name__ == '__main__':
    main()
