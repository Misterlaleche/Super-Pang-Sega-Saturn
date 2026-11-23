#!/usr/bin/env python3
"""Append original Tour phases 8..17 and region-aware native map resources.

The approved 214-frame R06 prefix and all prior scene files stay byte exact.
Scene expansion is independent of stage/prepare_data.py. The new ladders reuse
approved indexed rows; Europe monuments remain exact original 1:1 compounds.
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
ROOT = P.parent
C13 = ROOT / 'c13_reference/fuentes'
CD = P / 'cd/ART'
WORK = ART / 'r07'
BASE = ART / 'baseline'
EDIT = P / 'editable_art'
sys.path[:0] = [str(ART), str(C13)]
from build_r04_assets import read_sources, native_grid, pixel_text
from build_art import pack4, unpack4, vdp1_pens, source_pens, indices_png, rgb444, saturn_colors, tile_layer
from tools.stage_art import apply, background_writes
from tools.tour_assets import stage_background_writes, stage_hud_writes, stage_palette

sha = lambda b: hashlib.sha256(bytes(b)).hexdigest()
word = lambda b, p: int.from_bytes(b[p:p+2], 'little')
R06_AUTH_SHA = 'b2afc4dc27e0df2a0b857e9d4b5d9290e2b4717f43089280f3cb96bc54423758'
SCENES = (
    (11, 8, 'H2', 'himar2', None),
    (12, 9, 'H3', 'himar3', None),
    (13, None, 'M4', 'map_himar_istanbul', (0, 3, ((0x1496, 0x8a), (0x11e8, 0x52)))),
    (14, 10, 'I1', 'istanbul1', None),
    (15, 11, 'I2', 'istanbul2', None),
    (16, 12, 'I3', 'istanbul3', None),
    (17, None, 'M5', 'map_asia_complete', (0, 4, ((0x1496, 0x32), (0x1496, 0x2e)))),
    (18, 13, 'G1', 'germany1', None),
    (19, 14, 'G2', 'germany2', None),
    (20, 15, 'G3', 'germany3', None),
    (21, None, 'M6', 'map_germany_venice', (1, 5, ((0x1496, 0x8a), (0x11e8, 0x54)))),
    (22, 16, 'V1', 'venice1', None),
    (23, 17, 'V2', 'venice2', None),
)
REGIONAL_PARAMETERS = (0x32, 0x2e, 0x34, 0x36, 0x38, 0x3a, 0x3c, 0x40, 0x3c, 0x3e)


def read_baseline():
    for name, source in (
        ('r06_manifest.json', ART / 'manifest.json'),
        ('r06_AUTH4.BIN', CD / 'AUTH4.BIN'),
        ('r06_authored_index.h', ART / 'authored_index.h'),
        ('r06_editable_index.json', EDIT / 'index.json'),
    ):
        if not (BASE / name).exists():
            shutil.copyfile(source, BASE / name)
    baseline = json.loads((BASE / 'r06_manifest.json').read_text())
    old = (BASE / 'r06_AUTH4.BIN').read_bytes()
    assert len(old) == 90496 and len(baseline['frames']) == 214 and len(baseline['scenes']) == 11
    assert sha(old) == R06_AUTH_SHA == baseline['files']['AUTH4.BIN']['sha256']
    for name, meta in baseline['files'].items():
        if name not in ('AUTH4.BIN', 'CHAR4.BIN', 'CHARIDX.BIN'):
            assert sha((CD / name).read_bytes()) == meta['sha256'], ('Approved R06 file changed', name)
    return baseline, old


def new_ladders(cells, palette, records, payload):
    approved6 = np.frombuffer((EDIT / 'ladder_206_len6.indices.bin').read_bytes(), np.uint8).reshape(48, 32)
    approved13 = np.frombuffer((EDIT / 'ladder_206_len13.indices.bin').read_bytes(), np.uint8).reshape(112, 32)
    for rungs, expected in ((2, approved6[:16]), (11, np.concatenate((approved13[:80], approved13[96:112])))):
        height = ((rungs + 1) // 2) * 16
        source = np.full((height, 32), 15, np.uint8)
        for row in range(height // 16):
            odd_end = (rungs & 1) and row == height // 16 - 1
            for col in range(2):
                source[row*16:(row+1)*16, col*16:(col+1)*16] = cells[0x206 + col + (8 if odd_end else 0)]
        draft, columns = native_grid(source)
        pixels = np.full((height, 32), 15, np.uint8)
        pixels[:, :29] = draft
        assert np.array_equal(pixels, expected), ('Original ladder no longer reproduces approved rows', rungs)
        name = 'ladder_206_len' + str(rungs)
        (EDIT / (name + '.indices.bin')).write_bytes(pixels.tobytes())
        (EDIT / (name + '.pixels.txt')).write_text(pixel_text(pixels))
        indices_png(pixels, rgb444(palette), 13, EDIT / (name + '.png'))
        indices_png(source, rgb444(palette), 13, WORK / (name + '_original.png'))
        runtime = pack4(vdp1_pens(pixels))
        assert np.array_equal(source_pens(unpack4(runtime)).reshape(height, 32), pixels)
        ys, xs = np.where(pixels != 15)
        frame = dict(
            id=name, source_code=0x206, source_width=32, source_height=height, source_anchor=[16, height//2],
            role=18, kind=6, palette_bank=13, palette_banks=[13], families=['ladder'], source_bank=5,
            descriptors=[dict(address='BANK5:$8031..$8099', command=f'Original {rungs}-rung compound; repeated206/207 and20E/20F odd end')],
            approved_r01=False, approved_r02=False, approved_r03=False, added_r07=True,
            width=32, height=height, art_width=29, padded_width=32, anchor=[15, height//2],
            anchor_x=15, anchor_y=height//2, transparent_pen=15, runtime_transparent_pen=0,
            source_sha256=sha(source), initial_source_columns=columns.tolist(), source_descriptor='BANK5:$8031..$8099',
            editable_indices='editable_art/' + name + '.indices.bin', editable_png='editable_art/' + name + '.png',
            editable_pixel_map='editable_art/' + name + '.pixels.txt', indices_sha256=sha(pixels),
            offset=len(payload), bytes=len(runtime), packed_sha256=sha(runtime), pixel_edits=[],
            authored_edit_count=0, changed_silhouette_pixels=0,
            opaque_bbox_exclusive=[int(xs.min()), int(ys.min()), int(xs.max())+1, int(ys.max())+1],
            roundtrip_mismatched_pixels=0,
            visual_review='All native rows are exact approved ladder rows. Complete source canvas retained, including transparent odd-rung lower half.',
            approved_row_reuse='len6 rows0..15' if rungs == 2 else 'len13 rows0..79 followed by rows96..111',
            method='Exact approved indexed-row reuse checked against complete original compound on the established native grid',
        )
        records.append(frame)
        payload.extend(runtime)


def new_ball_variant(cells, palette, records, payload):
    """BANK1:9A91 type12 retains the approved size-two contour exactly."""
    source = cells[0x17]
    draft, columns = native_grid(source)
    pixels = np.full((16, 16), 15, np.uint8)
    pixels[:, :15] = draft
    approved = np.frombuffer((EDIT / 'ball_size_2.indices.bin').read_bytes(), np.uint8).reshape(16, 16)
    assert np.array_equal(pixels != 15, approved != 15)
    assert int(np.sum(pixels != 15)) == 162
    name = 'ball_size_2_code23'
    (EDIT / (name + '.indices.bin')).write_bytes(pixels.tobytes())
    (EDIT / (name + '.pixels.txt')).write_text(pixel_text(pixels))
    indices_png(pixels, rgb444(palette), 15, EDIT / (name + '.png'))
    indices_png(source, rgb444(palette), 15, WORK / (name + '_original.png'))
    runtime = pack4(vdp1_pens(pixels))
    assert runtime == (CD / 'CELL15.N4').read_bytes()[23*128:24*128]
    assert np.array_equal(source_pens(unpack4(runtime)).reshape(16, 16), pixels)
    frame = dict(
        id=name, source_code=0x17, source_width=16, source_height=16, source_anchor=[8, 8],
        role=1, kind=2, size_class=2, palette_bank=15, palette_banks=[15], families=['ball'], source_bank=1,
        descriptors=[dict(address='BANK1:$9A91', command='$2112', type=12)],
        approved_r01=False, approved_r02=False, approved_r03=False, added_r07=True,
        width=16, height=16, art_width=15, padded_width=16, anchor=[7, 8], anchor_x=7, anchor_y=8,
        transparent_pen=15, runtime_transparent_pen=0, source_sha256=sha(source),
        initial_source_columns=columns.tolist(), source_descriptor='BANK1:$9A91',
        editable_indices='editable_art/' + name + '.indices.bin', editable_png='editable_art/' + name + '.png',
        editable_pixel_map='editable_art/' + name + '.pixels.txt', indices_sha256=sha(pixels),
        offset=len(payload), bytes=len(runtime), packed_sha256=sha(runtime), pixel_edits=[],
        authored_edit_count=0, changed_silhouette_pixels=0, opaque_bbox_exclusive=[0, 1, 15, 15],
        roundtrip_mismatched_pixels=0, approved_geometry='ball_size_2; exact162-pixel mask and7,8anchor',
        visual_review='The original alternate shading on the established native grid has exactly the approved size-two silhouette. No new pen repair or contour change is needed.',
        method='Original code23 indexed shading transferred on the fixed native grid; exact approved size-two mask asserted',
    )
    records.append(frame)
    payload.extend(runtime)


def native_monuments(data, cells, palette):
    bank = data[0x1c000:0x20000]
    rows = []
    table = word(bank, 0xe3 + 2) - 0x8000
    for marker in range(0, 10, 2):
        descriptor = word(bank, table + marker)
        p = descriptor - 0x8000
        count, delta, attr = bank[p+1], word(bank, p+2)-0x8000, bank[p+4]
        parts, x, y = [], 0, 0
        for i in range(count):
            dx, dy = bank[delta+2*i:delta+2*i+2]
            x += dx if dx < 128 else dx-256
            y += dy if dy < 128 else dy-256
            parts.append(dict(code=bank[p+5+i] + ((attr & 0xe0) << 3), x=x, y=y))
        left, top = min(v['x'] for v in parts), min(v['y'] for v in parts)
        width, height = max(v['x'] for v in parts)+16-left, max(v['y'] for v in parts)+16-top
        for part in parts:
            part['x'] -= left
            part['y'] -= top
        pixels = np.full((height, width), 15, np.uint8)
        for part in reversed(parts):
            source = cells[part['code']]
            area = pixels[part['y']:part['y']+16, part['x']:part['x']+16]
            area[source != 15] = source[source != 15]
        name = f'europe_monument_{marker:02d}_native.png'
        indices_png(pixels, rgb444(palette), attr & 15, WORK / name)
        rows.append(dict(region_index=1, marker_id=marker, descriptor=f'BANK3:${descriptor:04X}',
                         source_code=parts[0]['code'], width=width, height=height, parts=parts,
                         palette_bank=attr & 15, original_anchor=[-left, -top],
                         indices_sha256=sha(pixels), source_png='art/r07/' + name,
                         rendering='rc_compose_native; original1:1cells; no authored sprite replacement'))
    return rows


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
    codes = set()
    scenes = list(baseline['scenes'])
    for scene in scenes:
        for key in ('chars_file', 'under_chars_file'):
            codes.update(int(c) & 32767 for c in np.frombuffer((CD / scene[key]).read_bytes(), '<u2'))
    for low, high in baseline['common_character_ranges_inclusive']:
        codes.update(range(low, high+1))
    viewx = np.frombuffer((CD / 'VIEWX.BIN').read_bytes(), '>u2').reshape(2, 320).astype(int)
    viewx[1] = np.arange(320) + np.where(np.arange(320) < 256, 64, 80)
    viewy = np.frombuffer((CD / 'VIEWY.BIN').read_bytes(), '>u2').astype(int)
    scene_previews, pending, comparisons_total = [], [], 0
    regional_codes = set()
    europe_palette = None
    ladder_palette = None

    def command(handler, parameter, attrs, chars):
        cb = C.CFUNCTYPE(None, C.c_void_p, C.c_uint16, u8)(lambda _, a, v: apply([(a, v)], attrs, chars))
        assert api.spang_video_command(handler, parameter, bank10, cb, None) == 0, (handler, parameter)

    for index, phase, prefix, label, map_spec in SCENES:
        attrs, chars = bytearray(base_attrs), bytearray(base_chars)
        if map_spec is not None:
            region, world, commands = map_spec
            attrs[0x40:0x7c0] = bytes([0x23]) * 0x780
            chars[0x80:0xf80] = b'\x0e\x07' * 0x780
            apply(background_writes(data, world=region), attrs, chars)
            for handler, parameter in commands:
                command(handler, parameter, attrs, chars)
            palette = bytearray(base_palette)
            table = 0x40000 + 0x50 + region*3
            address, bank_id = word(data, table), data[table+2]
            source = 0x10000 + bank_id*0x4000 + address-0x8000
            palette[0x400:0x600] = data[source:source+512]
            palette[:512] = data[0x4420c:0x4440c]
            if region == 1:
                europe_palette = bytes(palette)
            if index == 17:
                tally_attrs, tally_chars = bytearray(attrs), bytearray(chars)
                for parameter in REGIONAL_PARAMETERS:
                    command(0x1496, parameter, tally_attrs, tally_chars)
                    regional_codes.update(int(c) & 32767 for c in np.frombuffer(tally_chars, '<u2'))
        else:
            world = data[0x1260+phase]
            region = data[0x128a+world]
            attrs[0x40:0x6c0] = bytes([16]) * 0x680
            chars[0x80:0xd80] = b'\x20\0' * (0xd00//2)
            apply(stage_background_writes(data, phase), attrs, chars)
            apply(stage_hud_writes(data, phase), attrs, chars)
            palette = stage_palette(data, phase, base_palette)
            if phase == 8:
                ladder_palette = bytes(palette)
        under_attrs, under_chars = bytes(attrs), bytes(chars)
        if phase is not None:
            ar = (u8*2048).from_buffer_copy(attrs)
            ch = (u8*4096).from_buffer_copy(chars)
            grid = (u8*2048)()
            api.spang_stage_grid_clear(grid)
            assert api.spang_stage_platforms(phase, grid, ar, ch, bank5) == 0
            attrs, chars = bytes(ar), bytes(ch)
        paths = dict(attrs_file=prefix+'ATTR.BIN', chars_file=prefix+'CHARS.BIN',
                     under_attrs_file=prefix+'UATR.BIN', under_chars_file=prefix+'UCHR.BIN',
                     palette_file=prefix+'PAL.BIN', background_file=prefix+'BG.BIN')
        for key, blob in (('attrs_file', attrs), ('chars_file', chars), ('under_attrs_file', under_attrs),
                          ('under_chars_file', under_chars), ('palette_file', palette)):
            save(paths[key], blob, 'Original R07 ' + label + ' ' + key)
        save(prefix+'UNDER.BIN', under_attrs+under_chars, 'Original R07 ' + label + ' unoccluded tile data')
        source = tile_layer(glyphs, attrs, chars)
        native = np.empty((240, 320), np.uint16)
        for y in range(240):
            native[y] = source[viewy[y], np.arange(320)+96 if phase is None else viewx[int(y >= 208)]]
        colors = rgb444(palette)
        save(paths['background_file'], saturn_colors(palette)[native].astype('>u2').tobytes(),
             'Native tile-only R07 reference for ' + label)
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
         'Original glyph codes for twenty-four scenes and all Asia regional-completion commands')
    save('CHAR4.BIN', b''.join(packed[c*32:c*32+32] for c in ordered), 'Exact original glyph pixels in CHARIDX order')
    records = list(baseline['frames'])
    payload = bytearray(old)
    new_ladders(cells, ladder_palette, records, payload)
    new_ball_variant(cells, stage_palette(data, 14, base_palette), records, payload)
    save('AUTH4.BIN', payload, 'Frozen214-record R06 prefix plus exact approved-row ladders and approved-mask type12 ball')
    header = (BASE / 'r06_authored_index.h').read_text()
    header = re.sub(r'#define ART_AUTHORED_COUNT \d+u', f'#define ART_AUTHORED_COUNT {len(records)}u', header)
    header = re.sub(r'#define ART_AUTHORED_BYTES \d+u', f'#define ART_AUTHORED_BYTES {len(payload)}u', header)
    added = []
    for frame in records[214:]:
        values = [frame[k] for k in ('source_code', 'palette_bank', 'source_width', 'source_height', 'width', 'height',
                                    'anchor_x', 'anchor_y', 'art_width', 'role', 'kind')]
        values += [frame.get('size_class', 0), 32, frame['offset'], frame['bytes']]
        added.append('    {' + ','.join(map(str, values)) + '}, /* ' + frame['id'] + ' */\n')
    header = header.replace('};\n#endif', ''.join(added) + '};\n#endif')
    (ART / 'authored_index.h').write_text(header)
    monuments = native_monuments(data, cells, europe_palette)
    manifest.update(version='tour_eighteen_art_r07_v001', authored_frame_count=len(records),
                    compact_character_count=len(ordered), frames=records, files=files, scenes=scenes,
                    r07_original_stage_indices=list(range(8, 18)), r07_added_native_frames=3,
                    r07_regional_tally_glyphs=sorted(regional_codes),
                    r07_regional_tally_commands=[dict(handler=0x1496, parameter=p) for p in REGIONAL_PARAMETERS],
                    r07_native_map_monuments=monuments)
    manifest['layout'] = dict(manifest['layout'], map_scene_indices=[1, 5, 9, 13, 17, 21])
    (ART / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    editable = json.loads((BASE / 'r06_editable_index.json').read_text())
    for frame in records[214:]:
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
    contact.save(WORK / 'r07_new_scenes_review.png')
    report = dict(added_scenes=list(range(11, 24)), total_scene_count=len(scenes), original_stage_count=18,
                  authored_frames=len(records), authored_bytes=len(payload), glyph_count=len(ordered),
                  glyph_bytes=len(ordered)*32, glyph_index_bytes=len(ordered)*2+2,
                  old_authored_prefix_exact=bytes(payload[:len(old)]) == old, old_scene_files_exact=True,
                  new_stage_data_comparisons=comparisons_total, pending_stage_data_comparisons=pending,
                  native_monuments=5, max_monument_width=max(r['width'] for r in monuments),
                  scope='Offline original ROM expansion and pixel/scene checks; native execution verified separately')
    (WORK / 'build_report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
