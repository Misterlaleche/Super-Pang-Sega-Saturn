#!/usr/bin/env python3
"""Recover the original game-over, continue and name-entry PCM without synthesis."""
from pathlib import Path
import argparse
import ctypes as C
import hashlib
import importlib.util
import json
import re
import zipfile

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[1]
REF=ROOT.parent/'c13_reference/fuentes'
RECOVERED=OUT/'recovered'
CD=ROOT/'cd/AUDIO'
ARCHIVE_SHA='9d38da721f1f1a5561114107c6f5407a83cc85083a557bef346f1716b8aec2ca'
DISC_SHA='023a3a006fd5c6f75531e28e628d12f0f54b8fb1643b9864318f4370befc3c93'


def digest(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f,'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive',type=Path)
    args=parser.parse_args()
    spec=importlib.util.spec_from_file_location('original_export',ROOT/'audio/r10/export_original.py')
    original=importlib.util.module_from_spec(spec);spec.loader.exec_module(original)
    original.OUT=OUT
    helper=original.original_helpers()
    names={'EFX.RIC','M050.C13','M051.C13','M052.C13'}
    expected=json.loads((REF/'evidence/c13/manifest-c13.json').read_text())['files']
    RECOVERED.mkdir(parents=True,exist_ok=True)
    unchanged={p.name:digest(p) for p in CD.iterdir() if p.is_file()
               and p.name not in ('M050.WAV','M051.WAV','M052.WAV','F13.PCM')}
    assert digest(args.archive)==ARCHIVE_SHA
    with zipfile.ZipFile(args.archive) as archive:
        with archive.open('SUPER-PANG-C13-COMPACTO.bin') as f:
            assert hashlib.file_digest(f,'sha256').hexdigest()==DISC_SHA
            root=helper.read_raw(f,16*2048+156,34)
            directory=helper.read_raw(f,int.from_bytes(root[2:6],'little')*2048,
                                      int.from_bytes(root[10:14],'little'))
            entries={};pos=0
            while pos<len(directory):
                size=directory[pos]
                if not size:
                    pos=(pos//2048+1)*2048;continue
                e=directory[pos:pos+size];pos+=size
                if e[33:34] in (b'\0',b'\1'):continue
                name=e[33:33+e[32]].decode('ascii').split(';')[0]
                if name in names:entries[name]=(int.from_bytes(e[2:6],'little')*2048,int.from_bytes(e[10:14],'little'))
            assert set(entries)==names
            for name in sorted(entries,key=lambda x:entries[x][0]):
                path=RECOVERED/name;path.write_bytes(helper.read_raw(f,*entries[name]))
                assert path.stat().st_size==expected[name]['bytes'] and digest(path)==expected[name]['sha256']
    lib=original.decoder()
    catalog=(REF/'vendor/options_o03/src/sound_catalog.h').read_text()
    music=[(n,int(c),int(s),int(e)) for n,c,s,e in re.findall(r'\{"([^\"]+)",(\d+)u,(\d+)u,(\d+)u\}',catalog)]
    fx=[tuple(map(int,m)) for m in re.findall(r'\{(\d+)u,(\d+)u,(\d+)u,(\d+)u\}',catalog[catalog.index('catalog_effects'):catalog.index('catalog_commands')])]
    payloads=json.loads((REF/'vendor/options_o03/assets/payloads.json').read_text())
    report=dict(archive_sha256=ARCHIVE_SHA,disc_sha256=DISC_SHA,
                original_inputs={n:expected[n] for n in sorted(names)},unchanged_audio=unchanged,music=[],effects=[])
    rows=[]
    for command,asset in ((50,17),(51,18),(52,19)):
        name,count,start,end=music[asset]
        source=RECOVERED/name
        source.write_bytes(helper.unpack(source.with_suffix('.C13').read_bytes(),lib))
        assert source.stat().st_size==payloads[name]['bytes'] and digest(source)==payloads[name]['sha256']
        output=CD/f'M{command:03d}.WAV'
        helper.write_music(source,output,count)
        report['music'].append(dict(file=output.name,command=command,asset=asset,samples=count,
             rate=44100,loop_start=start,loop_end=end,source_pcm16be_sha256=digest(source),
             bytes=output.stat().st_size,sha256=digest(output)))
        rows.append('{"%s",%du,%du,%du,%d},'%(output.name,count,start,end,asset))
    offset,count,priority,extent=fx[13]
    rice=(RECOVERED/'EFX.RIC').read_bytes()
    values=(C.c_int16*count)()
    assert lib.rice(rice[offset:offset+extent],extent,count,values)==0
    raw,stored=original.pcm(list(values))
    (CD/'F13.PCM').write_bytes(stored)
    report['effects'].append(dict(file='F13.PCM',asset=13,samples=count,rate=22050,
        priority_samples_44100=priority,bytes=len(stored),sha256=hashlib.sha256(stored).hexdigest(),
        source_pcm16be_sha256=hashlib.sha256(raw).hexdigest(),guard_samples=1,
        scsp_resident=False,lifetime='postgame only; release before game cache preparation'))
    rows.append('{"F13.PCM",%du,%du,13,0},'%(count,priority))
    assert all(digest(CD/n)==h for n,h in unchanged.items())
    (OUT/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    (OUT/'catalog_records.txt').write_text('\n'.join(rows)+'\n')
    print('\n'.join(rows))


if __name__=='__main__':main()
