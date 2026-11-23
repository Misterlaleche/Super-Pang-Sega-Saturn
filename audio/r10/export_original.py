#!/usr/bin/env python3
"""Recover selected original C13 audio without extracting its complete disc.

All music samples and loop points are the original delivered C13 payloads.
F26 uses its existing Rice decoder. New OKI samples use the same original ROM
ADPCM decoder and gain as the preserved R09 assets. No catalogue/runtime edit.
"""
from pathlib import Path
import argparse
import ctypes as C
import hashlib
import importlib.util
import json
import re
import struct
import subprocess
import sys
import zipfile

sys.dont_write_bytecode = True
OUT = Path(__file__).resolve().parent
P = OUT.parents[1]
REF = P.parent / 'c13_reference/fuentes'
CD = P / 'cd/AUDIO'
RECOVERED = OUT / 'recovered'
ARCHIVE_SHA = '9d38da721f1f1a5561114107c6f5407a83cc85083a557bef346f1716b8aec2ca'
DISC_SHA = '023a3a006fd5c6f75531e28e628d12f0f54b8fb1643b9864318f4370befc3c93'
sha = lambda b: hashlib.sha256(bytes(b)).hexdigest()


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def original_helpers():
    spec = importlib.util.spec_from_file_location('r09_audio_builder', P / 'audio/build_assets.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def extract(archive, helpers):
    assert digest(archive) == ARCHIVE_SHA, 'Unexpected original C13 archive'
    RECOVERED.mkdir(parents=True, exist_ok=True)
    names = {'EFX.RIC', 'M048.PCM', 'M056.C13', 'M057.C13', 'OKI.BIN'}
    expected = json.loads((REF / 'evidence/c13/manifest-c13.json').read_text())['files']
    with zipfile.ZipFile(archive) as zipped:
        with zipped.open('SUPER-PANG-C13-COMPACTO.bin') as stream:
            assert hashlib.file_digest(stream, 'sha256').hexdigest() == DISC_SHA
            root = helpers.read_raw(stream, 16 * 2048 + 156, 34)
            directory = helpers.read_raw(stream, int.from_bytes(root[2:6], 'little') * 2048,
                                         int.from_bytes(root[10:14], 'little'))
            files, pos = {}, 0
            while pos < len(directory):
                size = directory[pos]
                if not size:
                    pos = (pos // 2048 + 1) * 2048; continue
                entry = directory[pos:pos + size]; pos += size
                raw_name = entry[33:33 + entry[32]]
                if raw_name in (b'\0', b'\1'):
                    continue
                name = raw_name.decode('ascii').split(';')[0]
                if name in names:
                    files[name] = (int.from_bytes(entry[2:6], 'little') * 2048,
                                   int.from_bytes(entry[10:14], 'little'))
            assert set(files) == names
            for name in sorted(files, key=lambda n: files[n][0]):
                blob = helpers.read_raw(stream, *files[name])
                assert len(blob) == expected[name]['bytes'] and sha(blob) == expected[name]['sha256'], name
                (RECOVERED / name).write_bytes(blob)
    return {name: expected[name] for name in sorted(names)}


def decoder():
    vendor = P / 'audio/vendor'
    source = OUT / 'decoder.c'
    source.write_text('''#include "audio_c13_codec.h"
#include "spang_rice.h"
#include "spang_oki_adpcm.h"
int decode(const unsigned char *s,unsigned n,unsigned char *d){return c13_decode_block(s,n,d);}
int rice(const unsigned char *p,unsigned n,unsigned count,short *out){
 spang_rice r;if(spang_rice_init(&r,p,n,0,0,0,n,count))return -1;
 for(unsigned i=0;i<count;i++)if(spang_rice_next(&r,out+i)!=1)return -2;
 return spang_rice_next(&r,out)!=0?-3:0;
}
void oki(const unsigned char *p,unsigned bytes,short *out){
 spang_oki_adpcm s;spang_oki_adpcm_reset(&s);
 for(unsigned i=0;i<bytes*2;i++){
  unsigned nibble=(i&1)?p[i/2]:p[i/2]>>4;
  out[i]=(short)((int)spang_oki_adpcm_clock(&s,nibble)*32*3/20);
 }
}
''')
    output = OUT / 'decoder.so'
    subprocess.run(['gcc', '-shared', '-fPIC', '-O2', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(vendor), str(source), str(vendor / 'spang_rice.c'),
                    str(vendor / 'spang_oki_adpcm.c'), '-o', str(output)], check=True)
    lib = C.CDLL(str(output)); lib.decode.argtypes = [C.c_void_p, C.c_uint, C.c_void_p]
    lib.rice.argtypes = [C.c_void_p, C.c_uint, C.c_uint, C.c_void_p]
    lib.oki.argtypes = [C.c_void_p, C.c_uint, C.c_void_p]
    return lib


def pcm(values):
    raw = struct.pack('>' + str(len(values)) + 'h', *values)
    stored = raw + b'\0\0'
    stored += bytes(-len(stored) % 32)
    return raw, stored


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    original_assets = json.loads((P / 'audio/asset_manifest.json').read_text())
    unchanged = {e['file']: digest(CD / e['file']) for kind in ('music', 'effects', 'oki') for e in original_assets[kind]}
    helper = original_helpers()
    inputs = extract(args.archive, helper)
    lib = decoder()
    catalog = (REF / 'vendor/options_o03/src/sound_catalog.h').read_text()
    effects = [tuple(map(int, match)) for match in re.findall(r'\{(\d+)u,(\d+)u,(\d+)u,(\d+)u\}',
               catalog[catalog.index('catalog_effects'):catalog.index('catalog_commands')])]
    music = [(name, int(count), int(start), int(end)) for name, count, start, end in
             re.findall(r'\{"([^\"]+)",(\d+)u,(\d+)u,(\d+)u\}', catalog)]
    payloads = json.loads((REF / 'vendor/options_o03/assets/payloads.json').read_text())
    report = dict(archive_sha256=ARCHIVE_SHA, disc_sha256=DISC_SHA, original_inputs=inputs,
                  unchanged_audio=unchanged, music=[], effects=[], oki=[])
    for command, asset in ((48, 15), (56, 23), (57, 24)):
        name, count, start, end = music[asset]
        source = RECOVERED / name
        if not source.exists():
            source.write_bytes(helper.unpack(source.with_suffix('.C13').read_bytes(), lib))
        assert source.stat().st_size == payloads[name]['bytes'] and digest(source) == payloads[name]['sha256']
        output = f'M{command:03d}.WAV'
        helper.write_music(source, CD / output, count)
        report['music'].append(dict(command=command, asset=asset, file=output, samples=count, rate=44100,
                                   loop_start=start, loop_end=end, prefix_bytes=65536,
                                   source_pcm16be_sha256=digest(source), bytes=(CD / output).stat().st_size,
                                   sha256=digest(CD / output),
                                   conversion='Original C13 PCM16BE mono to WAV PCM16LE mono; every sample verified'))
    rice = (RECOVERED / 'EFX.RIC').read_bytes()

    def effect(asset):
        offset, count, priority, extent = effects[asset]
        values = (C.c_int16 * count)()
        assert lib.rice(rice[offset:offset + extent], extent, count, values) == 0
        raw, stored = pcm(list(values))
        return raw, stored, count, priority

    raw, stored, count, priority = effect(26)
    (CD / 'F26.PCM').write_bytes(stored)
    report['effects'].append(dict(file='F26.PCM', samples=count, priority_samples_44100=priority, asset=26,
                                 rate=22050, scsp_resident=True, bytes=len(stored), sha256=sha(stored),
                                 source_pcm16be_sha256=sha(raw), guard_samples=1,
                                 conversion='Original C13 lossless Rice decode, signed16 BE'))
    oki = (RECOVERED / 'OKI.BIN').read_bytes()
    assert len(oki) == 131072 and sha(oki) == payloads['OKI.BIN']['sha256']

    def sample(number):
        pointer = oki[number * 8:number * 8 + 6]
        start, end = int.from_bytes(pointer[:3], 'big') & 0x3ffff, int.from_bytes(pointer[3:], 'big') & 0x3ffff
        assert start < end < len(oki)
        count = 2 * (end - start + 1); values = (C.c_int16 * count)()
        lib.oki(oki[start:end + 1], end - start + 1, values)
        raw, stored = pcm(list(values))
        return raw, stored, count, start, end

    for number, control in ((0x17, 0x80), (0x19, 0x20), (0x1a, 0x40)):
        raw, stored, count, start, end = sample(number)
        name = f'O{number:02X}.PCM'; (CD / name).write_bytes(stored)
        report['oki'].append(dict(file=name, sample=number, control=control, samples=count, rate=7576,
                                 original_clock_numerator=250000, original_clock_denominator=33,
                                 source_rom_range=[start, end], scsp_resident=True, bytes=len(stored),
                                 sha256=sha(stored), source_pcm16be_sha256=sha(raw), guard_samples=1,
                                 conversion='Original OKI ADPCM decoder; same R09 voice volume32 and gain3/20'))
    assert effect(3)[1] == (CD / 'F03.PCM').read_bytes()
    assert sample(0x0c)[1] == (CD / 'O0C.PCM').read_bytes()
    report['existing_F03_and_O0C_decodes_identical'] = True
    for name, expected in unchanged.items():
        assert digest(CD / name) == expected
    report['memory_recommendation'] = dict(move_existing_F11_to_wram_bytes=44960,
        new_resident_effect_and_oki_bytes=72736, resulting_scsp_pcm_bytes=357440,
        resulting_wram_pcm_bytes=452864, resulting_upload_room_bytes=97184,
        existing_F19_upload_bytes=93728, upload_margin_bytes=3456,
        frontend_prefix_bytes=65536, panic_prefix_bytes=196608, retained_tour_prefix_peak_bytes=262144)
    report['source_hashes'] = {str(path.relative_to(P)): digest(path) for path in
        (P / 'audio/vendor/spang_rice.c', P / 'audio/vendor/spang_oki_adpcm.c', P / 'audio/vendor/audio_c13_codec.h',
         P / 'audio/build_assets.py', Path(__file__))}
    (OUT / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    rows = []
    for e in report['effects']:
        rows.append('{"%s",%du,%du,%d,1},' % (e['file'], e['samples'], e['priority_samples_44100'], e['asset']))
    for e in report['music']:
        rows.append('{"%s",%du,%du,%du,%d},' % (e['file'], e['samples'], e['loop_start'], e['loop_end'], e['asset']))
    for e in report['oki']:
        rows.append('{"%s",%du,%d},' % (e['file'], e['samples'], e['sample']))
    (OUT / 'catalog_records.txt').write_text('\n'.join(rows) + '\n')
    print('\n'.join(rows)); print(json.dumps(report['memory_recommendation'], indent=2))


if __name__ == '__main__':
    main()
