#!/usr/bin/env python3
"""Append original Tour phases18..27 without changing approved R07 pixels.

New scene resources are independently expanded from ROM commands and compared
with stage/prepare_data.py. Whole ladders reuse only approved indexed rows.
"""
from pathlib import Path
import ctypes as C
import hashlib
import json
import re
import shutil
import struct
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.dont_write_bytecode = True
ART = Path(__file__).resolve().parent
P = ART.parent
C13 = P.parent / 'c13_reference/fuentes'
CD = P / 'cd/ART'
WORK = ART / 'r08'
BASE = ART / 'baseline'
EDIT = P / 'editable_art'
sys.path[:0] = [str(ART), str(C13)]
from build_r04_assets import read_sources, native_grid, pixel_text
from build_art import pack4, unpack4, vdp1_pens, source_pens, indices_png, rgb444, saturn_colors, tile_layer
from tools.stage_art import apply, background_writes
from tools.tour_assets import stage_background_writes, stage_hud_writes, stage_palette

sha = lambda b: hashlib.sha256(bytes(b)).hexdigest()
word = lambda b, p: int.from_bytes(b[p:p+2], 'little')
R07_AUTH_SHA = 'f66591918b912e2aa625994f615e09f0a10c4c267229d49df454ec6c02f46933'
SCENES = (
    (24, None, 'M7', 'map_venice_spain', (1, 6, 0x56)),
    (25, 18, 'SP1', 'spain1', None),
    (26, 19, 'SP2', 'spain2', None),
    (27, 20, 'SP3', 'spain3', None),
    (28, None, 'M8', 'map_spain_france', (1, 7, 0x58)),
    (29, 21, 'F1', 'france1', None),
    (30, 22, 'F2', 'france2', None),
    (31, 23, 'F3', 'france3', None),
    (32, None, 'M9', 'map_france_tower', (1, 8, 0x5a)),
    (33, 24, 'T1', 'tower1', None),
    (34, 25, 'T2', 'tower2', None),
    (35, 26, 'T3', 'tower3', None),
    (36, None, 'MA', 'map_europe_complete', (1, 9, None)),
    (37, 27, 'C1', 'canada1', None),
)
# BANK3:81CB+world9 supplies30; 8C74+region1 supplies40 for the
# five tally increments, and8CBA+2*region1 supplies084A for the flash.
REGIONAL_PARAMETERS = (0x32, 0x30, 0x42, 0x44, 0x46, 0x48, 0x4a, 0x40, 0x4a, 0x3e)


def read_baseline():
    for name, source in (
        ('r07_manifest.json', ART / 'manifest.json'),
        ('r07_AUTH4.BIN', CD / 'AUTH4.BIN'),
        ('r07_CHARIDX.BIN', CD / 'CHARIDX.BIN'),
        ('r07_authored_index.h', ART / 'authored_index.h'),
        ('r07_editable_index.json', EDIT / 'index.json'),
    ):
        if not (BASE / name).exists():
            shutil.copyfile(source, BASE / name)
    editable_files = BASE / 'r07_editable_files.json'
    if not editable_files.exists():
        editable_files.write_text(json.dumps({p.name: sha(p.read_bytes()) for p in sorted(EDIT.iterdir())
                                              if p.is_file() and p.name != 'index.json'}, indent=2) + '\n')
    baseline = json.loads((BASE / 'r07_manifest.json').read_text())
    old = (BASE / 'r07_AUTH4.BIN').read_bytes()
    assert len(old) == 92416 and len(baseline['frames']) == 217 and len(baseline['scenes']) == 24
    assert sha(old) == R07_AUTH_SHA == baseline['files']['AUTH4.BIN']['sha256']
    for name, metadata in baseline['files'].items():
        if name not in ('AUTH4.BIN', 'CHAR4.BIN', 'CHARIDX.BIN'):
            assert sha((CD / name).read_bytes()) == metadata['sha256'], ('Approved R07 file changed', name)
    for name, digest in json.loads(editable_files.read_text()).items():
        assert sha((EDIT / name).read_bytes()) == digest, ('Approved editable file changed', name)
    return baseline, old


def new_ladders(cells, palettes, records, payload):
    approved6 = np.frombuffer((EDIT / 'ladder_206_len6.indices.bin').read_bytes(), np.uint8).reshape(48, 32)
    approved13 = np.frombuffer((EDIT / 'ladder_206_len13.indices.bin').read_bytes(), np.uint8).reshape(112, 32)
    specifications = (
        (5, 18, np.concatenate((approved6[:32], approved13[96:112])), 'len6 rows0..31 + len13 rows96..111'),
        (14, 20, np.concatenate((approved6, approved6, approved6[:16])), 'len6 rows0..47 twice + rows0..15'),
        (20, 18, np.concatenate((approved6, approved6, approved6, approved6[:16])), 'len6 rows0..47 three times + rows0..15'),
    )
    for rungs, phase, expected, reuse in specifications:
        height = ((rungs + 1) // 2) * 16
        source = np.full((height, 32), 15, np.uint8)
        for row in range(height // 16):
            odd_end = (rungs & 1) and row == height // 16 - 1
            for col in range(2):
                source[row*16:(row+1)*16, col*16:(col+1)*16] = cells[0x206 + col + (8 if odd_end else 0)]
        draft, columns = native_grid(source)
        pixels = np.full((height, 32), 15, np.uint8)
        pixels[:, :29] = draft
        assert np.array_equal(pixels, expected), ('Original ladder differs from approved rows', rungs)
        name = f'ladder_206_len{rungs}'
        (EDIT / (name + '.indices.bin')).write_bytes(pixels.tobytes())
        (EDIT / (name + '.pixels.txt')).write_text(pixel_text(pixels))
        indices_png(pixels, rgb444(palettes[phase]), 13, EDIT / (name + '.png'))
        indices_png(source, rgb444(palettes[phase]), 13, WORK / (name + '_original.png'))
        runtime = pack4(vdp1_pens(pixels))
        assert np.array_equal(source_pens(unpack4(runtime)).reshape(height, 32), pixels)
        ys, xs = np.where(pixels != 15)
        records.append(dict(
            id=name, source_code=0x206, source_width=32, source_height=height, source_anchor=[16, height//2],
            role=18, kind=6, palette_bank=13, palette_banks=[13], families=['ladder'], source_bank=5,
            descriptors=[dict(address='BANK5:$8031..$8099', command=f'Original {rungs}-rung compound; repeated206/207 and20E/20F odd end')],
            approved_r01=False, approved_r02=False, approved_r03=False, added_r08=True,
            width=32, height=height, art_width=29, padded_width=32, anchor=[15, height//2],
            anchor_x=15, anchor_y=height//2, transparent_pen=15, runtime_transparent_pen=0,
            source_sha256=sha(source), initial_source_columns=columns.tolist(), source_descriptor='BANK5:$8031..$8099',
            editable_indices='editable_art/' + name + '.indices.bin', editable_png='editable_art/' + name + '.png',
            editable_pixel_map='editable_art/' + name + '.pixels.txt', indices_sha256=sha(pixels),
            offset=len(payload), bytes=len(runtime), packed_sha256=sha(runtime), pixel_edits=[],
            authored_edit_count=0, changed_silhouette_pixels=0, ladder_rungs=rungs,
            opaque_bbox_exclusive=[int(xs.min()), int(ys.min()), int(xs.max())+1, int(ys.max())+1],
            roundtrip_mismatched_pixels=0, approved_row_reuse=reuse,
            visual_review='Exact approved indexed rows; full original canvas and odd-end transparency retained.',
            method='Approved row reuse independently checked against complete original compound on the native grid',
        ))
        payload.extend(runtime)


def main():
    WORK.mkdir(exist_ok=True)
    baseline, old = read_baseline()
    manifest = dict(baseline)
    files = dict(baseline['files'])

    def save(name, blob, meaning):
        blob = bytes(blob)
        (CD / name).write_bytes(blob)
        files[name] = dict(bytes=len(blob), sha256=sha(blob), meaning=meaning)

    data, cells, glyphs = read_sources()
    capture = C13 / 'build/wp02/recovered-visual/run-1'
    base_attrs = (capture / 'attrs.bin').read_bytes()
    base_chars = (capture / 'chars.bin').read_bytes()
    base_palette = (capture / 'palette.bin').read_bytes()
    library = WORK / 'scene_sources.so'
    subprocess.run(['gcc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra', '-Werror',
                    str(P / 'stage/spang_stage_bootstrap.c'), str(P / 'stage/spang_video_commands.c'),
                    '-o', str(library)], check=True)
    api = C.CDLL(str(library))
    u8 = C.c_uint8
    bank5 = (u8*16384).from_buffer_copy(data[0x24000:0x28000])
    bank10 = (u8*16384).from_buffer_copy(data[0x38000:0x3c000])
    old_index = (BASE / 'r07_CHARIDX.BIN').read_bytes()
    codes = set(map(int, np.frombuffer(old_index[2:], '>u2')))
    scenes = list(baseline['scenes'])
    viewx = np.frombuffer((CD / 'VIEWX.BIN').read_bytes(), '>u2').reshape(2, 320).astype(int)
    viewx[1] = np.arange(320) + np.where(np.arange(320) < 256, 64, 80)
    viewy = np.frombuffer((CD / 'VIEWY.BIN').read_bytes(), '>u2').astype(int)
    scene_previews, pending, comparisons_total = [], [], 0
    regional_codes, palettes = set(), {}

    def command(handler, parameter, attrs, chars):
        cb = C.CFUNCTYPE(None, C.c_void_p, C.c_uint16, u8)(lambda _, a, v: apply([(a, v)], attrs, chars))
        assert api.spang_video_command(handler, parameter, bank10, cb, None) == 0, (handler, parameter)

    for index, phase, prefix, label, map_spec in SCENES:
        attrs, chars = bytearray(base_attrs), bytearray(base_chars)
        if map_spec is not None:
            region, world, parameter = map_spec
            attrs[0x40:0x7c0] = bytes([0x23]) * 0x780
            chars[0x80:0xf80] = b'\x0e\x07' * 0x780
            apply(background_writes(data, world=region), attrs, chars)
            commands = ((0x1496, 0x32), (0x1496, 0x30)) if parameter is None else ((0x1496, 0x8a), (0x11e8, parameter))
            for handler, value in commands:
                command(handler, value, attrs, chars)
            palette = bytearray(base_palette)
            table = 0x40000 + 0x50 + region*3
            address, bank_id = word(data, table), data[table+2]
            source = 0x10000 + bank_id*0x4000 + address-0x8000
            palette[0x400:0x600] = data[source:source+512]
            palette[:512] = data[0x4420c:0x4440c]
            if parameter is None:
                tally_attrs, tally_chars = bytearray(attrs), bytearray(chars)
                for value in REGIONAL_PARAMETERS:
                    command(0x1496, value, tally_attrs, tally_chars)
                    regional_codes.update(int(c) & 32767 for c in np.frombuffer(tally_chars, '<u2'))
        else:
            world = data[0x1260+phase]
            region = data[0x128a+world]
            attrs[0x40:0x6c0] = bytes([16]) * 0x680
            chars[0x80:0xd80] = b'\x20\0' * (0xd00//2)
            apply(stage_background_writes(data, phase), attrs, chars)
            apply(stage_hud_writes(data, phase), attrs, chars)
            palette = stage_palette(data, phase, base_palette)
            palettes[phase] = bytes(palette)
        under_attrs, under_chars = bytes(attrs), bytes(chars)
        if phase is not None:
            ar, ch = (u8*2048).from_buffer_copy(attrs), (u8*4096).from_buffer_copy(chars)
            grid = (u8*2048)()
            api.spang_stage_grid_clear(grid)
            assert api.spang_stage_platforms(phase, grid, ar, ch, bank5) == 0
            attrs, chars = bytes(ar), bytes(ch)
        paths = dict(attrs_file=prefix+'ATTR.BIN', chars_file=prefix+'CHARS.BIN',
                     under_attrs_file=prefix+'UATR.BIN', under_chars_file=prefix+'UCHR.BIN',
                     palette_file=prefix+'PAL.BIN', background_file=prefix+'BG.BIN')
        for key, blob in (('attrs_file', attrs), ('chars_file', chars), ('under_attrs_file', under_attrs),
                          ('under_chars_file', under_chars), ('palette_file', palette)):
            save(paths[key], blob, 'Original R08 ' + label + ' ' + key)
        save(prefix+'UNDER.BIN', under_attrs+under_chars, 'Original R08 ' + label + ' unoccluded tile data')
        source = tile_layer(glyphs, attrs, chars)
        native = np.empty((240, 320), np.uint16)
        for y in range(240):
            native[y] = source[viewy[y], np.arange(320)+96 if phase is None else viewx[int(y >= 208)]]
        colors = rgb444(palette)
        save(paths['background_file'], saturn_colors(palette)[native].astype('>u2').tobytes(),
             'Native tile-only R08 reference for ' + label)
        Image.fromarray(colors[source[8:248, 64:448]]).save(WORK / (label + '_original.png'))
        im = Image.fromarray(colors[native])
        im.save(WORK / (label + '_native.png'))
        scene_previews.append((label, im))
        scene_codes = {int(c) & 32767 for b in (chars, under_chars) for c in np.frombuffer(b, '<u2')}
        codes.update(scene_codes)
        comparisons = {}
        checks = [('under_attrs', under_attrs), ('under_chars', under_chars), ('initial_palette', bytes(palette))]
        if phase is not None:
            checks += [('platform_attrs', bytes(attrs)), ('platform_chars', bytes(chars))]
        for suffix, blob in checks:
            name = label + '_' + suffix + '.bin'
            path = P / 'stage/data' / name
            if path.exists():
                assert path.read_bytes() == blob, ('Independent stage data mismatch', name)
                comparisons[name] = 'byte-identical'
                comparisons_total += 1
            else:
                comparisons[name] = 'pending-independent-stage-export'
                pending.append(name)
        scenes.append(dict(scene_index=index, phase_index=phase, world_index=world, region_index=region,
                           label=label, **paths, character_count=len(scene_codes), stage_data_comparisons=comparisons,
                           native_x_mapping='map_native_1_to_1_x96' if phase is None else 'playfield_then_native_HUD',
                           native_y_mapping='source_y=native_y+8', palette_sha256=sha(palette)))

    codes.update(regional_codes)
    ordered = sorted(codes)
    packed = pack4(glyphs)
    save('CHARIDX.BIN', struct.pack('>H', len(ordered)) + b''.join(struct.pack('>H', c) for c in ordered),
         'Original glyph codes for thirty-eight scenes and all regional-completion commands')
    save('CHAR4.BIN', b''.join(packed[c*32:c*32+32] for c in ordered), 'Exact original glyph pixels in CHARIDX order')
    unique_patterns, unique_payload, pairs = {}, bytearray(), bytearray()
    for code in ordered:
        pattern = packed[code*32:code*32+32]
        if pattern not in unique_patterns:
            unique_patterns[pattern] = len(unique_patterns)
            unique_payload.extend(pattern)
        pairs.extend(struct.pack('>HH', code, unique_patterns[pattern]))
    glyph_map = struct.pack('>HH', len(ordered), len(unique_patterns)) + pairs
    save('CHARMAP.BIN', glyph_map, 'BE16codecount,BE16uniquecount,then sortedBE16code/BE16ordinal pairs')
    save('CHARD4.BIN', unique_payload, 'Original32-byte glyph patterns deduplicated in ascending-code first-seen order')
    records, payload = list(baseline['frames']), bytearray(old)
    new_ladders(cells, palettes, records, payload)
    save('AUTH4.BIN', payload, 'Frozen217-record R07 prefix plus exact approved-row ladders5/14/20')
    header = (BASE / 'r07_authored_index.h').read_text()
    header = re.sub(r'#define ART_AUTHORED_COUNT \d+u', f'#define ART_AUTHORED_COUNT {len(records)}u', header)
    header = re.sub(r'#define ART_AUTHORED_BYTES \d+u', f'#define ART_AUTHORED_BYTES {len(payload)}u', header)
    glyph_defines = (f'#define ART_GLYPH_CODE_COUNT {len(ordered)}u\n'
                     f'#define ART_GLYPH_UNIQUE_COUNT {len(unique_patterns)}u\n'
                     f'#define ART_GLYPH_MAP_BYTES {len(glyph_map)}u\n'
                     f'#define ART_GLYPH_BYTES {len(unique_payload)}u\n')
    header = header.replace('/* Original 112 records', glyph_defines + '/* Original 112 records')
    added = []
    for frame in records[217:]:
        values = [frame[k] for k in ('source_code', 'palette_bank', 'source_width', 'source_height', 'width', 'height',
                                    'anchor_x', 'anchor_y', 'art_width', 'role', 'kind')]
        values += [frame.get('size_class', 0), 64, frame['offset'], frame['bytes']]
        added.append('    {' + ','.join(map(str, values)) + '}, /* ' + frame['id'] + ' */\n')
    rungs = [int(frame['id'].rsplit('len', 1)[1]) if frame['role'] == 18 else 0 for frame in records]
    companion = '/* Original rung counts distinguish odd/even ladders with identical canvas dimensions. */\n'
    companion += 'static const uint8_t authored_ladder_rungs[ART_AUTHORED_COUNT] = {\n'
    companion += ''.join('    ' + ','.join(map(str, rungs[i:i+24])) + ',\n' for i in range(0, len(rungs), 24))
    companion += '};\n'
    header = header.replace('};\n#endif', ''.join(added) + '};\n' + companion + '#endif')
    (ART / 'authored_index.h').write_text(header)
    variants = [dict(record_index=i, id=records[i]['id'], rungs=rungs[i]) for i in range(len(records)) if rungs[i]]
    manifest.update(version='tour_twenty_eight_art_r08_v001', authored_frame_count=len(records),
                    compact_character_count=len(ordered), frames=records, files=files, scenes=scenes,
                    r08_original_stage_indices=list(range(18, 28)), r08_added_native_frames=3,
                    r08_regional_tally_glyphs=sorted(regional_codes), r08_ladder_variants=variants,
                    r08_regional_tally_commands=[dict(handler=0x1496, parameter=p) for p in REGIONAL_PARAMETERS],
                    r08_runtime_atlas=dict(map_file='CHARMAP.BIN', pixels_file='CHARD4.BIN',
                        code_count=len(ordered), unique_count=len(unique_patterns),
                        map_bytes=len(glyph_map), pixel_bytes=len(unique_payload),
                        ram_saving_bytes=len(ordered)*34+2-len(glyph_map)-len(unique_payload),
                        format='BE16codecount,BE16uniquecount,then sortedBE16code/BE16ordinal pairs;32bytes per unique pattern'))
    manifest['layout'] = dict(manifest['layout'], map_scene_indices=[1, 5, 9, 13, 17, 21, 24, 28, 32, 36])
    (ART / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    editable = json.loads((BASE / 'r07_editable_index.json').read_text())
    for frame in records[217:]:
        row = {k: frame[k] for k in ('id', 'source_code', 'source_width', 'source_height', 'width', 'height',
                                    'anchor_x', 'anchor_y', 'art_width', 'role', 'palette_bank', 'offset', 'bytes', 'indices_sha256')}
        row.update(png=frame['id']+'.png', indices=frame['id']+'.indices.bin', pixel_map=frame['id']+'.pixels.txt')
        editable['frames'].append(row)
    editable['source_sha256'] = sha(payload)
    (EDIT / 'index.json').write_text(json.dumps(editable, indent=2) + '\n')
    contact = Image.new('RGB', (320*4, 264*4), (24, 27, 33))
    draw = ImageDraw.Draw(contact)
    for i, (label, im) in enumerate(scene_previews):
        x, y = (i % 4)*320, (i // 4)*264
        draw.text((x+7, y+6), label, fill='white')
        contact.paste(im, (x, y+24))
    contact.save(WORK / 'r08_new_scenes_review.png')
    ladder_review = Image.new('RGB', (288*3, 704), (24, 27, 33))
    draw = ImageDraw.Draw(ladder_review)
    for column, rungs in enumerate((5, 14, 20)):
        x = column*288
        draw.text((x+8, 8), f'{rungs} rungs: original / approved native grid', fill='white')
        for side, path in enumerate((WORK / f'ladder_206_len{rungs}_original.png',
                                      EDIT / f'ladder_206_len{rungs}.png')):
            picture = Image.open(path).convert('RGBA')
            picture = picture.resize((picture.width*4, picture.height*4), Image.Resampling.NEAREST)
            ladder_review.paste(picture, (x+8+side*140, 40), picture)
    ladder_review.save(WORK / 'ladder_review.png')
    report = dict(added_scenes=list(range(24, 38)), total_scene_count=len(scenes), original_stage_count=28,
                  authored_frames=len(records), authored_bytes=len(payload), glyph_count=len(ordered),
                  glyph_bytes=len(ordered)*32, glyph_index_bytes=len(ordered)*2+2,
                  runtime_atlas=manifest['r08_runtime_atlas'],
                  old_authored_prefix_exact=bytes(payload[:len(old)]) == old, old_scene_files_exact=True,
                  old_editable_files_exact=True, ladder_variants=variants,
                  new_stage_data_comparisons=comparisons_total, pending_stage_data_comparisons=pending,
                  scope='Offline original ROM expansion and pixel/scene checks; native execution verified separately')
    (WORK / 'build_report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
