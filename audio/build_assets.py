#!/usr/bin/env python3
"""Recover original C13 audio for the all forty Tour phases, tallies and the original ending.

Input disc/archive are user-provided prior C13 deliverables. Their exact hashes,
all recovered payload hashes and generated asset transforms are recorded.
No changes are made to the C13 source tree.
"""
from pathlib import Path
import argparse
import array
import ctypes as C
import hashlib
import json
import re
import shutil
import struct
import subprocess
import sys
import wave
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
REFERENCE = ROOT.parent / 'c13_reference/fuentes'
RECOVERED = HERE / 'recovered'
OUT = ROOT / 'cd/AUDIO'
VENDOR = HERE / 'vendor'
FX_IDS = (0, 3, 4, 6, 7, 8, 9, 11, 12, 14, 17, 18, 19, 25, 29, 30)
# New FM assets and unchanged F03 use the existing exclusive upload voice.
# All OKI remains resident; F19 retains 31,232 bytes of upload headroom.
WRAM_FX = (3, 7, 9, 14, 17, 18, 19, 29, 30)
MUSIC = (33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 49, 53, 54, 55, 59)
PERMANENT_MUSIC = (49, 53, 59)
STAGE_COUNT = 40
OKI = (0x0c, 0x11, 0x12, 0x14, 0x1c)
DISC_SHA256 = '023a3a006fd5c6f75531e28e628d12f0f54b8fb1643b9864318f4370befc3c93'
ARCHIVE_SHA256 = '9d38da721f1f1a5561114107c6f5407a83cc85083a557bef346f1716b8aec2ca'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def file_digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()

def read_raw(f, offset, length):
    data = bytearray()
    while length:
        sector, within = divmod(offset, 2048)
        n = min(length, 2048 - within)
        f.seek(sector * 2352 + 16 + within)
        part = f.read(n)
        if len(part) != n:
            raise ValueError('Truncated original C13 disc')
        data += part
        offset += n
        length -= n
    return bytes(data)

def recover(disc=None, archive=None):
    RECOVERED.mkdir(parents=True, exist_ok=True)
    required = ['EFX.RIC', 'OKI.BIN'] + [f'M{x:03d}.PCM' for x in MUSIC]
    missing = [name for name in required if not (RECOVERED/name).is_file()
               and not (RECOVERED/name.replace('.PCM', '.C13')).is_file()]
    if not missing:
        return

    def extract(f):
        # ZipExtFile permits seeking without materializing the entire disc.
        # Hash its full decompressed stream first; all original inputs stay read-only.
        h = hashlib.sha256()
        while chunk := f.read(1024*1024):
            h.update(chunk)
        if h.hexdigest() != DISC_SHA256:
            raise ValueError('Unexpected original C13 disc')
        root = read_raw(f, 16*2048 + 156, 34)
        directory = read_raw(f, int.from_bytes(root[2:6], 'little')*2048,
                             int.from_bytes(root[10:14], 'little'))
        files = {}
        p = 0
        while p < len(directory):
            n = directory[p]
            if not n:
                p = (p // 2048 + 1)*2048
                continue
            record = directory[p:p+n]
            p += n
            if record[33:34] in (b'\0', b'\1'):
                continue
            name = record[33:33+record[32]].decode('ascii').split(';')[0]
            files[name] = (int.from_bytes(record[2:6], 'little')*2048,
                           int.from_bytes(record[10:14], 'little'))
        for name in missing:
            stored = name if name in files else name.replace('.PCM', '.C13')
            offset, length = files[stored]
            (RECOVERED / stored).write_bytes(read_raw(f, offset, length))
    if archive is not None:
        if file_digest(archive) != ARCHIVE_SHA256:
            raise ValueError('Unexpected original C13 archive')
        with zipfile.ZipFile(archive) as z:
            with z.open('SUPER-PANG-C13-COMPACTO.bin') as source:
                extract(source)
    else:
        if disc is None:
            disc = RECOVERED / 'SUPER-PANG-C13-COMPACTO.bin'
        with disc.open('rb') as source:
            extract(source)

def vendor():
    VENDOR.mkdir(exist_ok=True)
    paths = {
        **{n: REFERENCE/'vendor/options_o03/vendor/audio'/n for n in
           ('spang_rice.c', 'spang_rice.h', 'spang_oki_adpcm.c', 'spang_oki_adpcm.h')},
        'audio_c13_codec.h': REFERENCE/'saturn/motion/audio_c13_codec.h',
        'c13_encode.c': REFERENCE/'tools/c13_encode.c',
        'MAME-OKI-BSD-3-Clause.txt': REFERENCE/'reference/licenses/MAME-OKI-BSD-3-Clause.txt',
    }
    provenance = {}
    for name, source in paths.items():
        raw = source.read_bytes()
        (VENDOR/name).write_bytes(raw)
        provenance[name] = dict(source=str(source.relative_to(REFERENCE)), sha256=digest(raw))
    (VENDOR/'PROVENANCE.json').write_text(json.dumps(provenance, indent=2)+'\n')

def decoder():
    bridge = HERE/'decoder_bridge.c'
    bridge.write_text('''#include "audio_c13_codec.h"
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
    so = HERE/'decoder_bridge.so'
    subprocess.run(['cc', '-O2', '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(VENDOR), str(bridge), str(VENDOR/'spang_rice.c'),
                    str(VENDOR/'spang_oki_adpcm.c'), '-o', str(so)], check=True)
    lib = C.CDLL(str(so))
    lib.decode.argtypes = [C.c_void_p, C.c_uint, C.c_void_p]
    lib.rice.argtypes = [C.c_void_p, C.c_uint, C.c_uint, C.c_void_p]
    lib.oki.argtypes = [C.c_void_p, C.c_uint, C.c_void_p]
    return lib

def unpack(data, lib):
    magic, samples, base, count, start, loop, after, length, ls, le = struct.unpack('>4s9I', data[:40])
    assert magic == b'C13L' and length == len(data) and start == 2048
    anchors = {0:start, base:loop, base+count:after}
    pos, out = start, bytearray()
    block = C.create_string_buffer(2048)
    for i in range(0, samples, 1024):
        pos = anchors.get(i, pos)
        n = int.from_bytes(data[pos:pos+2], 'big')
        assert 3 <= n <= 2051 and pos+n <= length
        assert lib.decode(data[pos:pos+n], n, block) == 0
        out += block.raw[:min(1024, samples-i)*2]
        pos += n
    assert hashlib.sha256(out).digest() == data[40:72]
    return bytes(out)

def pcm_file(name, values):
    raw = struct.pack('>'+str(len(values))+'h', *values)
    # A zero interpolation/end guard means Jo's one-shot LEA=n-1 includes the
    # final original sample. Pad to32 for Jo's DMA alignment, not audible data.
    stored = raw + b'\0\0'
    stored += bytes(-len(stored) % 32)
    (OUT/name).write_bytes(stored)
    return dict(file=name, samples=len(values), source_pcm16be_sha256=digest(raw),
                bytes=len(stored), sha256=digest(stored), guard_samples=1)

def write_music(source, output, samples):
    """Container/endian change only, using bounded original PCM chunks."""
    with source.open('rb') as raw, wave.open(str(output), 'wb') as wav:
        wav.setparams((1, 2, 44100, samples, 'NONE', 'not compressed'))
        left = samples * 2
        while left:
            chunk = raw.read(min(left, 65536))
            if not chunk or len(chunk) & 1:
                raise ValueError('Truncated original music PCM')
            swapped = array.array('h')
            swapped.frombytes(chunk)
            swapped.byteswap()
            wav.writeframesraw(swapped.tobytes())
            left -= len(chunk)
    # Compare each source sample without a 44-million-element Python tuple.
    with source.open('rb') as raw, wave.open(str(output), 'rb') as wav:
        assert wav.getparams()[:3] == (1, 2, 44100)
        assert wav.getnframes() == samples
        left = samples
        while left:
            count = min(left, 32768)
            swapped = array.array('h')
            swapped.frombytes(wav.readframes(count))
            swapped.byteswap()
            assert swapped.tobytes() == raw.read(count * 2)
            left -= count

def m035_range():
    """Use the first complete verified 13:12 traversal, with original intro.

    The original catalog verified both traversals but retained the later one.
    All output samples are the unchanged prefix of the original full PCM.
    See verification/m035_loop_proof.json and REPORT_R04.md for the seam scope.
    """
    manifest = json.loads((REFERENCE/'vendor/options_o03/assets/manifest.json').read_text())
    entry = next(e for e in manifest['entries'] if e.get('command') == 35)
    loop = entry['loop']
    assert entry['sha256'] == '9b3834104e3ececfa6a43e01d847df39a5bfaf7017c3e875510c9ae01fa65b5c'
    assert entry['samples'] == 44583855
    assert (loop['first_start_frame'], loop['period_frames'], loop['play_start_frame'],
            loop['play_end_frame'], loop['verified_equal_event_traversals']) == (590,28730,29320,58050,2)
    start = 590 * 44100 * 50 // 2871
    end = 29320 * 44100 * 50 // 2871
    assert (start, end) == (453134, 22518495)
    return start, end

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group()
    inputs.add_argument('--source-disc', type=Path,
                        help='Original hash-verified C13 MODE1/2352 BIN; optional after recovery')
    inputs.add_argument('--source-archive', type=Path,
                        help='Original C13 ZIP; extract only missing payloads without writing its BIN')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    recover(args.source_disc, args.source_archive)
    vendor()
    lib = decoder()
    manifest = json.loads((REFERENCE/'vendor/options_o03/assets/payloads.json').read_text())
    catalog = (REFERENCE/'vendor/options_o03/src/sound_catalog.h').read_text()
    effects = [tuple(map(int, m)) for m in re.findall(r'\{(\d+)u,(\d+)u,(\d+)u,(\d+)u\}',
              catalog[catalog.index('catalog_effects'):catalog.index('catalog_commands')])]
    music = [(n, int(s), int(a), int(b)) for n,s,a,b in re.findall(
        r'\{"([^\"]+)",(\d+)u,(\d+)u,(\d+)u\}', catalog)]
    commands = [(int(i),int(asset),kind,int(priority)) for i,asset,kind,priority in
                re.findall(r'\[(\d+)\]=\{(-?\d+),(SPANG_PCM_\w+),(\d+)\}', catalog)]
    compressed_manifest = json.loads((REFERENCE/'evidence/c13/manifest-c13.json').read_text())['files']
    report = {'source_disc_sha256':DISC_SHA256,
              'original_payloads':{}, 'original_compressed_payloads':{},
              'music':[], 'effects':[], 'oki':[]}
    for name in ['EFX.RIC','OKI.BIN']+[f'M{x:03d}.PCM' for x in MUSIC]:
        p = RECOVERED/name
        packed = p.with_suffix('.C13')
        if packed.exists():
            expected = compressed_manifest[packed.name]
            assert packed.stat().st_size == expected['bytes'] and file_digest(packed) == expected['sha256'], packed.name
            report['original_compressed_payloads'][packed.name] = expected
        if not p.exists():
            p.write_bytes(unpack(packed.read_bytes(),lib))
        assert p.stat().st_size==manifest[name]['bytes'] and file_digest(p)==manifest[name]['sha256'], name
        report['original_payloads'][name] = manifest[name]
    for command in MUSIC:
        asset = next(a for c,a,k,p in commands if c==command)
        name,samples,loop_start,loop_end = music[asset]
        original_samples,original_start,original_end = samples,loop_start,loop_end
        if command == 35:
            loop_start,loop_end = m035_range()
            samples = loop_end
        output = f'M{command:03d}.WAV'
        write_music(RECOVERED/name, OUT/output, samples)
        entry = dict(command=command,asset=asset,file=output,samples=samples,
            rate=44100,loop_start=loop_start,loop_end=loop_end,
            bytes=(OUT/output).stat().st_size,sha256=file_digest(OUT/output),
            conversion='PCM16BE mono to WAV PCM16LE mono; all selected signed samples identical')
        if command == 35:
            entry.update(original_samples=original_samples,original_loop_start=original_start,
                original_loop_end=original_end,original_pcm_sha256=manifest[name]['sha256'],
                selection='Original PCM prefix; earlier complete verified 28730-frame 13:12 traversal',
                original_frame_bounds=[590,29320],whole_polyrhythm_steps=59904,
                chip_state_periodic_proven=False)
        report['music'].append(entry)
    rice = (RECOVERED/'EFX.RIC').read_bytes()
    for asset in FX_IDS:
        offset,samples,priority_samples,extent = effects[asset]
        array = (C.c_int16*samples)()
        assert lib.rice(rice[offset:offset+extent],extent,samples,array)==0
        entry = pcm_file(f'F{asset:02d}.PCM',list(array))
        entry.update(asset=asset,rate=22050,priority_samples_44100=priority_samples,
                     scsp_resident=asset not in WRAM_FX,conversion='Original C13 lossless Rice decode, signed16 BE')
        report['effects'].append(entry)
    oki = (RECOVERED/'OKI.BIN').read_bytes()
    for sample in OKI:
        ptr = oki[sample*8:sample*8+6]
        start,stop = int.from_bytes(ptr[:3],'big')&0x3ffff,int.from_bytes(ptr[3:],'big')&0x3ffff
        assert start<stop<len(oki)
        array = (C.c_int16*(2*(stop-start+1)))()
        lib.oki(oki[start:stop+1],stop-start+1,array)
        entry = pcm_file(f'O{sample:02X}.PCM',list(array))
        entry.update(sample=sample,rate=7576,original_clock_numerator=250000,
                     original_clock_denominator=33,scsp_resident=True,
                     conversion='Original OKI ADPCM decoder; C13 voice volume32 and gain3/20; SCSP PCM interpolation at requested7576Hz')
        report['oki'].append(entry)
    kinds = {'SPANG_PCM_NONE':0,'SPANG_PCM_MUSIC':1,'SPANG_PCM_EFFECT':2,
             'SPANG_PCM_RESET_ALL':3,'SPANG_PCM_RESET_EFFECT':4,
             'SPANG_PCM_RESET_MUSIC':5,'SPANG_PCM_FADE':6}
    h = ['/* Generated by audio/build_assets.py from exact C13 catalog. */',
         '#ifndef HK_AUDIO_CATALOG_H','#define HK_AUDIO_CATALOG_H','#include <stdint.h>',
         'typedef struct {int16_t asset;uint8_t kind,priority;} audio_catalog_command;',
         'typedef struct {const char *file;uint32_t samples,priority_samples;uint8_t asset,resident;} audio_fx_asset;',
         'typedef struct {const char *file;uint32_t samples,loop_start,loop_end;uint8_t asset;} audio_music_asset;',
         'typedef struct {const char *file;uint32_t samples;uint8_t sample;} audio_oki_asset;',
         'static const audio_catalog_command audio_commands[256]={']
    h += [f' [{i}]={{{asset},{kinds[kind]},{priority}}},' for i,asset,kind,priority in commands]
    h += ['};','static const audio_fx_asset audio_effects[]={']
    h += [f' {{"{e["file"]}",{e["samples"]}u,{e["priority_samples_44100"]}u,{e["asset"]},{int(e["scsp_resident"])}}},' for e in report['effects']]
    h += ['};','static const audio_music_asset audio_music[]={']
    h += [f' {{"{e["file"]}",{e["samples"]}u,{e["loop_start"]}u,{e["loop_end"]}u,{e["asset"]}}},' for e in report['music']]
    h += ['};','static const audio_oki_asset audio_oki_assets[]={']
    h += [f' {{"{e["file"]}",{e["samples"]}u,{e["sample"]}}},' for e in report['oki']]
    original_data = (REFERENCE/'build/wp00/decoded/data.bin').read_bytes()
    assert digest(original_data) == '9d460c57aac71d4ec9e7e8e6d361e259ff789fd7aeee7f545a60c688c63a09f2'
    stage_music = [original_data[0x18000+0x22cc+original_data[0x1260+stage]] for stage in range(STAGE_COUNT)]
    assert stage_music == [33,34,34,34,35,35,35,36,36,36,37,37,37,38,38,38,39,39,
                           40,40,40,41,41,41,42,42,42,43,43,43,44,44,44,45,45,46,46,46,47,47]
    assert all(command in MUSIC for command in stage_music)
    h += ['};','static const uint8_t audio_stage_music[]={'+','.join(map(str,stage_music))+'};',
          'static const uint8_t audio_permanent_music[]={'+','.join(map(str,PERMANENT_MUSIC))+'};',
          f'#define AUDIO_STAGE_COUNT {STAGE_COUNT}',
          f'#define AUDIO_PERMANENT_MUSIC_COUNT {len(PERMANENT_MUSIC)}',
          f'#define AUDIO_FX_COUNT {len(report["effects"])}',
          f'#define AUDIO_MUSIC_COUNT {len(report["music"])}',
          f'#define AUDIO_OKI_COUNT {len(report["oki"])}','#endif','']
    (HERE/'catalog.h').write_text('\n'.join(h))
    report['scsp_pcm_bytes'] = sum(e['bytes'] for e in report['effects']+report['oki'] if e['scsp_resident'])
    report['wram_pcm_bytes'] = sum(e['bytes'] for e in report['effects'] if not e['scsp_resident'])
    report['disc_asset_bytes'] = sum(e['bytes'] for e in report['effects']+report['oki']+report['music'])
    report['stage_music_commands'] = stage_music
    report['permanent_music_commands'] = list(PERMANENT_MUSIC)
    report['ending_music_commands'] = [54, 55]
    report['ending_cached_music_commands'] = [53, 54, 55, 59]
    report['ending_cache_policy'] = 'At stopped final entry retire world and warning49; prepare54/55; restore ordinary cache at next stage boundary'
    report['world_music_prefix_slots'] = 1
    report['music_prefix_bytes'] = 65536 * (len(PERMANENT_MUSIC)+1)
    report['wram_stream_bytes'] = 65536
    report['scsp_music_ring_bytes'] = 65568
    report['scsp_upload_room_bytes'] = 524288 - 4096 - 65568 - report['scsp_pcm_bytes']
    report['largest_fm_upload_bytes'] = max(e['bytes'] for e in report['effects'] if not e['scsp_resident'])
    assert report['largest_fm_upload_bytes'] <= report['scsp_upload_room_bytes'], 'SCSP would truncate an FM sample'
    report['source_catalog_sha256'] = digest((REFERENCE/'vendor/options_o03/src/sound_catalog.h').read_bytes())
    (HERE/'asset_manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:report[k] for k in ('scsp_pcm_bytes','wram_pcm_bytes','disc_asset_bytes')},indent=2))

if __name__=='__main__':
    main()
