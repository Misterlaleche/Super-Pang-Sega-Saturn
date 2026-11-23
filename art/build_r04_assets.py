#!/usr/bin/env python3
"""Append reviewed, ROM-derived R04 indexed art to the frozen R03 payload.

Whole source compounds are transferred once to the established native grid.
Explicit reviewed pen edits are replayed with old-pen assertions. No released
sprite, editable image, scene, or coordinate map is regenerated here.
"""
from pathlib import Path
import argparse
import ctypes as C
import hashlib
import json
import re
import struct
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.dont_write_bytecode = True
ART = Path(__file__).resolve().parent
PROJECT = ART.parent
ROOT = PROJECT.parent
C13 = ROOT/'c13_reference/fuentes'
CD = PROJECT/'cd/ART'
EDITABLE = PROJECT/'editable_art'
BASELINE = ART/'baseline'
WORK = ART/'r04'
sys.path.insert(0,str(ART))
sys.path.insert(0,str(C13))
from build_art import pack4, unpack4, vdp1_pens, source_pens, rgb444, saturn_colors, rgba, indices_png, tile_layer
from tools.visual_assets import decode_graphics
from tools.stage_art import background_writes, apply
from tools.tour_assets import stage_background_writes, stage_hud_writes, stage_palette

DATA_SHA = '9d460c57aac71d4ec9e7e8e6d361e259ff789fd7aeee7f545a60c688c63a09f2'
AUTH_SHA = '18b98148a3fdfd25e252ae5ebf96a2891f6c3d6cb968e82ec23831225188c1cd'


def sha(blob):
    return hashlib.sha256(blob).hexdigest()


def word(blob, offset):
    return int.from_bytes(blob[offset:offset+2],'little')


def read_sources():
    decoded=C13/'build/wp00/decoded'
    data=(decoded/'data.bin').read_bytes()
    assert sha(data)==DATA_SHA,'Original decoded ROM changed'
    cells=decode_graphics((decoded/'sprites.bin').read_bytes(),True)
    glyphs=decode_graphics((decoded/'chars.bin').read_bytes())
    assert cells.shape==(2048,16,16) and glyphs.shape==(32768,8,8)
    return data,cells,glyphs


def baseline_inputs():
    manifest=json.loads((BASELINE/'r03_manifest.json').read_text())
    payload=(BASELINE/'r03_AUTH4.BIN').read_bytes()
    editable=json.loads((BASELINE/'r03_editable_index.json').read_text())
    assert sha(payload)==AUTH_SHA and len(payload)==43008 and len(manifest['frames'])==112
    assert len(editable['frames'])==112
    for name,meta in manifest['files'].items():
        if name not in ('AUTH4.BIN','CHAR4.BIN','CHARIDX.BIN'):
            assert sha((CD/name).read_bytes())==meta['sha256'],('Approved R03 file changed',name)
    for name,meta in json.loads((BASELINE/'r03_editable_files.json').read_text()).items():
        assert sha((EDITABLE/name).read_bytes())==meta['sha256'],('Approved editable file changed',name)
    return manifest,payload,editable


def original_inventory(data):
    frames={};descriptors=[]
    commands={0x2112:(16,16),0x2108:(16,16),0x21ad:(32,32),0x21a3:(32,32),
              0x2702:(32,32),0x246f:(48,32),0xbde6:(32,32)}
    def add(bank,address,family,role):
        blob=data[0x10000+bank*0x4000:0x14000+bank*0x4000];p=address-0x8000
        control=blob[p];command=word(blob,p+1);assert command in commands,(bank,hex(address),hex(command))
        code=blob[p+3]+((blob[p+4]&0xe0)<<3);palette=blob[p+4]&15;sw,sh=commands[command]
        descriptor=dict(address=f'BANK{bank}:${address:04X}',control=control,ticks=control&127,
                        command=f'${command:04X}',source_code=code,palette_bank=palette,family=family)
        if control&128:descriptor['loop_to']=f'BANK{bank}:${word(blob,p+5):04X}'
        descriptors.append(descriptor);key=(code,sw,sh,role)
        frame=frames.setdefault(key,dict(id=f'{family}_{code:03x}',source_code=code,
            source_width=sw,source_height=sh,source_anchor=[sw//2,sh//2],
            role=role,kind={1:2,2:1,5:7,15:5,17:4}[role],palette_bank=palette,
            palette_banks=[],families=[],source_bank=bank,descriptors=[]))
        if palette not in frame['palette_banks']:frame['palette_banks'].append(palette)
        if family not in frame['families']:frame['families'].append(family)
        frame['descriptors'].append(descriptor)
        return address+(7 if control&128 else 5)
    def sequence(bank,start,end,family,role):
        p=start
        while p<end:p=add(bank,p,family,role)
        assert p==end,(bank,hex(start),hex(end),hex(p))
    for lo,hi in ((0x9496,0x9504),(0x967d,0x972d)):
        sequence(4,lo,hi,'puffer',17)
    sequence(4,0x9735,0x975d,'shared_puff',17)
    for lo,hi in ((0xaf60,0xafb0),(0xafb0,0xafc8),(0xafc8,0xafd2),
                  (0xafd2,0xb022),(0xb022,0xb03a),(0xb03a,0xb044)):
        sequence(4,lo,hi,'bird',17)
    sequence(4,0xb044,0xb06c,'shared_puff',17)
    for lo,hi in ((0x9ada,0x9aeb),(0x9aeb,0x9b0b),(0x9b0b,0x9b1c),(0x9b1c,0x9b2d)):
        sequence(1,lo,hi,'diamond',1)
    for address in (0xae82,0xae87,0xae8c,0xae91,0xae96):add(0,address,'p1_climb',2)
    add(3,0x8d8b,'map_plane',15)
    sequence(4,0xb36e,0xb39a,'secret_bonus',17)
    frames[(0x206,32,48,18)]=dict(id='ladder_206_len6',source_code=0x206,
        source_width=32,source_height=48,source_anchor=[16,24],role=18,kind=6,
        palette_bank=13,palette_banks=[13],families=['ladder'],source_bank=5,
        descriptors=[dict(address='BANK5:$8031..$8099',command='Repeated ladder columns',
            parts=[dict(code=0x206+x,x=x*16,y=y*16) for y in range(3) for x in range(2)])])
    # Integration exposed the original E680 impact and E700 bubble effects.
    # Append them after the entire approved207-record R04 inventory.
    add(3,0x962c,'puffer_impact',17)
    for address in (0x9d69,0x9d6e,0x9d73,0x9d78):add(2,address,'puffer_bubble',17)
    # Original hidden-block lucky item for phases3/4, frozen runtime roleITEM.
    add(1,0xae60,'lucky_item',5)
    result=list(frames.values())
    assert len(result)==101 and len(descriptors)==149
    return result,descriptors


def source_frame(cells,frame):
    sw,sh=frame['source_width'],frame['source_height']
    source=np.full((sh,sw),15,np.uint8)
    for row in range(sh//16):
        for col in range(sw//16):
            code=frame['source_code']+col+(0 if frame['role']==18 else row*8)
            source[row*16:(row+1)*16,col*16:(col+1)*16]=cells[code]
    return source


def native_grid(source):
    sw=source.shape[1];nw=(sw*200+109)//219
    columns=((2*np.arange(nw)+1)*sw)//(2*nw)
    return source[:,columns].copy(),columns


def pixel_text(pixels):
    return '\n'.join(''.join('0123456789ABCDEF'[int(p)] for p in row) for row in pixels)+'\n'


def palette_for(frame,palettes):
    return palettes['map' if frame['role']==15 else 'java3' if frame['role']==1 else 'silk1']


def apply_edits(frame,draft,patches):
    edits=patches.get(frame['id'],[]);final=draft.copy();seen=set()
    for edit in edits:
        x,y=edit['x'],edit['y'];assert (x,y) not in seen,(frame['id'],x,y)
        seen.add((x,y));assert 0<=edit['to_pen']<16 and edit['feature']
        assert int(final[y,x])==edit['from_pen'],(frame['id'],edit)
        assert edit['from_pen']!=edit['to_pen']
        final[y,x]=edit['to_pen']
    return final


def review_sheets(frames,cells,palettes,patches,exported=None):
    groups=[('puffer',[f for f in frames if f['families'][0]=='puffer']),
            ('burst',[f for f in frames if f['families'][0]=='shared_puff']),
            ('bird',[f for f in frames if f['families'][0]=='bird']),
            ('other',[f for f in frames if f['families'][0] not in ('puffer','shared_puff','bird')])]
    paths=[]
    for group,items in groups:
        for start in range(0,len(items),12):
            part=items[start:start+12];cw=454;ch=206;cols=3
            canvas=Image.new('RGB',(cw*cols,ch*((len(part)+cols-1)//cols)),(28,31,36));draw=ImageDraw.Draw(canvas)
            for n,f in enumerate(part):
                x=(n%cols)*cw;y=(n//cols)*ch;source=source_frame(cells,f);draft,_=native_grid(source)
                final=exported[f['id']][:,:((f['source_width']*200+109)//219)] if exported is not None else apply_edits(f,draft,patches)
                draw.text((x+8,y+7),f"{f['id']}  src {f['source_width']}x{f['source_height']}  pal {f['palette_bank']}",fill='white')
                colors=palette_for(f,palettes)
                variants=[('Original',source),('Grid',draft),('AUTH4' if exported is not None else 'Repaired',final)]
                for j,(title,p) in enumerate(variants):
                    xx=x+8+j*150;draw.text((xx,y+25),title,fill=(180,195,210))
                    pic=rgba(p,colors,f['palette_bank']).resize((p.shape[1]*3,p.shape[0]*3),Image.Resampling.NEAREST)
                    draw.rectangle((xx-1,y+43,xx+pic.width,y+44+pic.height),outline=(66,75,85))
                    canvas.paste(pic,(xx,y+44),pic)
            filename=f"{'exported' if exported is not None else 'review'}_{group}_{start//12+1:02d}.png"
            canvas.save(WORK/filename);paths.append(filename)
    return paths


def scene_resources(data,glyphs,baseline,save):
    build=ART/'build';build.mkdir(exist_ok=True);library=build/'r04_scene_sources.so'
    subprocess.run(['gcc','-shared','-fPIC','-O2','-Wall','-Wextra','-Werror',
        str(C13/'src/spang_stage_bootstrap.c'),str(C13/'src/spang_video_commands.c'),'-o',str(library)],check=True)
    api=C.CDLL(str(library));u8=C.c_uint8;capture=C13/'build/wp02/recovered-visual/run-1'
    bank5=(u8*16384).from_buffer_copy(data[0x24000:0x28000]);bank10=(u8*16384).from_buffer_copy(data[0x38000:0x3c000])
    bank12=data[0x40000:0x44000];base_palette=(capture/'palette.bin').read_bytes()
    scenes=list(baseline['scenes']);codes=set()
    for s in scenes:
        for key in ('chars_file','under_chars_file'):
            codes.update(int(v)&32767 for v in np.frombuffer((CD/s[key]).read_bytes(),'<u2'))
    xmaps=np.frombuffer((CD/'VIEWX.BIN').read_bytes(),'>u2').reshape(2,320).astype(int)
    ymap=np.frombuffer((CD/'VIEWY.BIN').read_bytes(),'>u2').astype(int)
    for index,phase,prefix,label in ((4,3,'J3','java3'),(5,None,'M2','map_java_silk'),(6,4,'S1','silk1')):
        attrs=bytearray((capture/'attrs.bin').read_bytes());chars=bytearray((capture/'chars.bin').read_bytes())
        if phase is None:
            attrs[0x40:0x7c0]=bytes([0x23])*0x780;chars[0x80:0xf80]=b'\x0e\x07'*0x780
            apply(background_writes(data),attrs,chars)
            callback=C.CFUNCTYPE(None,C.c_void_p,C.c_uint16,u8)(lambda _,a,v:apply([(a,v)],attrs,chars))
            for command,parameter in ((0x1496,0x8a),(0x11e8,0x4e)):
                assert api.spang_video_command(command,parameter,bank10,callback,None)==0
            palette=bytearray(base_palette);p=word(bank12,0x50)-0x8000
            palette[0x400:0x600]=bank12[p:p+512];palette[:512]=data[0x4420c:0x4440c]
        else:
            attrs[0x40:0x6c0]=bytes([16])*0x680;chars[0x80:0xd80]=b'\x20\0'*(0xd00//2)
            apply(stage_background_writes(data,phase),attrs,chars);apply(stage_hud_writes(data,phase),attrs,chars)
            palette=stage_palette(data,phase,base_palette)
        under_attrs,under_chars=bytes(attrs),bytes(chars)
        if phase is not None:
            ar=(u8*2048).from_buffer_copy(attrs);ch=(u8*4096).from_buffer_copy(chars);grid=(u8*2048)()
            api.spang_stage_grid_clear(grid);assert api.spang_stage_platforms(phase,grid,ar,ch,bank5)==0
            attrs,chars=bytes(ar),bytes(ch)
        paths=dict(attrs_file=prefix+'ATTR.BIN',chars_file=prefix+'CHARS.BIN',
            under_attrs_file=prefix+'UATR.BIN',under_chars_file=prefix+'UCHR.BIN',
            palette_file=prefix+'PAL.BIN',background_file=prefix+'BG.BIN')
        for key,blob in (('attrs_file',attrs),('chars_file',chars),('under_attrs_file',under_attrs),
                         ('under_chars_file',under_chars),('palette_file',palette)):
            save(paths[key],blob,f'Original {label} '+key.replace('_file','')+'; independent ROM expansion')
        save(prefix+'UNDER.BIN',under_attrs+under_chars,f'Original {label} under-platform attributes then characters')
        source=tile_layer(glyphs,attrs,chars);native=np.empty((240,320),np.uint16)
        for y in range(240):native[y]=source[ymap[y],xmaps[int(y>=208 and phase is not None)]]
        colors=rgb444(palette);colors555=saturn_colors(palette)
        save(paths['background_file'],colors555[native].astype('>u2').tobytes(),f'Original {label} native320 tile-only reference')
        Image.fromarray(colors[source[8:248,64:448]]).save(WORK/(label+'_original_tile_layer.png'))
        Image.fromarray(colors[native]).save(WORK/(label+'_native_tile_layer.png'))
        scene_codes={int(v)&32767 for b in (chars,under_chars) for v in np.frombuffer(b,'<u2')};codes|=scene_codes
        checks=[('under_attrs',under_attrs),('under_chars',under_chars),('initial_palette',bytes(palette))]
        if phase is not None:checks.extend((('platform_attrs',attrs),('platform_chars',chars)))
        comparisons={}
        for suffix,blob in checks:
            reference=PROJECT/'stage/data'/(label+'_'+suffix+'.bin')
            if reference.exists():
                assert reference.read_bytes()==blob,('Stage/art expansion differs',reference)
                comparisons[reference.name]='byte-identical'
        scenes.append(dict(scene_index=index,phase_index=phase,world_index=None if phase is None else data[0x1260+phase],
            label=label,**paths,character_count=len(scene_codes),stage_data_comparisons=comparisons,
            native_x_mapping='playfield_all_rows' if phase is None else 'playfield_then_native_HUD',
            native_y_mapping='source_y=native_y+8',palette_sha256=sha(bytes(palette))))
    for low,high in baseline['common_character_ranges_inclusive']:codes.update(range(low,high+1))
    return scenes,sorted(codes)


def append_frames(frames,cells,palettes,patches,review_notes,baseline,old_payload):
    payload=bytearray(old_payload);records=list(baseline['frames']);exported={}
    assert set(patches)=={f['id'] for f in frames},'Patch inventory must explicitly review every new resource'
    assert set(review_notes)==set(patches),'Every resource needs an honest visual-review note'
    for f in frames:
        source=source_frame(cells,f);draft,columns=native_grid(source);final=apply_edits(f,draft,patches)
        sh,nw=final.shape;pw=(nw+7)&~7;padded=np.full((sh,pw),15,np.uint8);padded[:,:nw]=final
        name=f['id'];path=EDITABLE/(name+'.indices.bin');path.write_bytes(padded.tobytes())
        png=EDITABLE/(name+'.png');indices_png(padded,palette_for(f,palettes),f['palette_bank'],png)
        text=EDITABLE/(name+'.pixels.txt');text.write_text(pixel_text(padded))
        packed=pack4(vdp1_pens(padded));offset=len(payload);assert offset%8==0;payload.extend(packed)
        assert np.array_equal(source_pens(unpack4(packed)).reshape(sh,pw),padded)
        ys,xs=np.where(padded!=15);edits=patches[name];ax=(f['source_width']*100+109)//219;ay=sh//2
        records.append(dict(f,approved_r01=False,approved_r02=False,approved_r03=False,added_r04=True,
            width=pw,height=sh,art_width=nw,padded_width=pw,anchor=[ax,ay],anchor_x=ax,anchor_y=ay,
            transparent_pen=15,runtime_transparent_pen=0,source_sha256=sha(source.tobytes()),
            initial_source_columns=columns.tolist(),source_descriptor=f['descriptors'][0]['address'],
            source_path=f'native320_r04/art/r04/drafts/{name}.source.png',
            indices=str(path.relative_to(ROOT)),image=str(png.relative_to(ROOT)),
            editable_indices=str(path.relative_to(PROJECT)),editable_png=str(png.relative_to(PROJECT)),
            editable_pixel_map=str(text.relative_to(PROJECT)),indices_sha256=sha(padded.tobytes()),
            offset=offset,bytes=len(packed),packed_sha256=sha(packed),pixel_edits=edits,
            authored_edit_count=len(edits),changed_silhouette_pixels=sum((e['from_pen']==15)!=(e['to_pen']==15) for e in edits),
            opaque_bbox_exclusive=[int(xs.min()),int(ys.min()),int(xs.max())+1,int(ys.max())+1],
            roundtrip_mismatched_pixels=0,visual_review=review_notes[name],
            method='Complete original indexed compound; fixed native grid plus explicitly reviewed pen repairs'))
        exported[name]=padded
    assert bytes(payload[:43008])==old_payload and records[:112]==baseline['frames']
    pinned=(BASELINE/'r04_207_AUTH4.BIN').read_bytes()
    assert len(pinned)==87936 and sha(pinned)=='12fd9563bdab99b4bcb07904e8cbec19d2f75732fe037e03220b948885b207b6'
    assert bytes(payload[:len(pinned)])==pinned
    assert records[:207]==json.loads((BASELINE/'r04_207_manifest.json').read_text())['frames']
    pinned212=(BASELINE/'r04_212_AUTH4.BIN').read_bytes()
    assert len(pinned212)==88576 and sha(pinned212)=='f4d3d4473926a1a238d17a66a0bd0ee5e85ee8e7ccc16391782ef7c19b19c53e'
    assert bytes(payload[:len(pinned212)])==pinned212
    assert records[:212]==json.loads((BASELINE/'r04_212_manifest.json').read_text())['frames']
    assert len(records)==213 and len(payload)==88704
    return records,bytes(payload),exported


def write_index(records):
    old_lines=re.findall(r'^    \{.*$',(BASELINE/'r03_authored_index.h').read_text(),re.M)
    assert len(old_lines)==112
    header=['/* Generated R04 indexed art. R03 entries are unchanged; sprites draw at native 1:1. */',
        '#ifndef SPANG_R04_AUTHORED_INDEX_H','#define SPANG_R04_AUTHORED_INDEX_H','#include <stdint.h>',
        'typedef struct { uint16_t source_code, palette_bank, source_width, source_height, width, height; int16_t anchor_x, anchor_y; uint16_t art_width, role, kind, size_class, flags; uint32_t offset, bytes; } ArtAuthoredRecord;',
        f'#define ART_AUTHORED_COUNT {len(records)}u',f'#define ART_AUTHORED_BYTES {sum(f["bytes"] for f in records)}u',
        '/* Original 112 records unchanged; flags8=R04; role18=whole ladder. */',
        'static const ArtAuthoredRecord art_authored_records[ART_AUTHORED_COUNT] = {',*old_lines]
    for f in records[112:]:
        values=[f['source_code'],f['palette_bank'],f['source_width'],f['source_height'],f['width'],f['height'],
            f['anchor_x'],f['anchor_y'],f['art_width'],f['role'],f['kind'],f.get('size_class',0),8,f['offset'],f['bytes']]
        header.append('    {'+','.join(map(str,values))+'}, /* '+f['id']+' */')
    header.extend(['};','#endif','']);(ART/'authored_index.h').write_text('\n'.join(header))


def export(data,cells,glyphs,frames,descriptors,patches,review_notes,palettes):
    baseline,old_payload,editable=baseline_inputs();files=dict(baseline['files'])
    def save(name,blob,meaning):
        blob=bytes(blob);(CD/name).write_bytes(blob);files[name]=dict(bytes=len(blob),sha256=sha(blob),meaning=meaning)
    scenes,codes=scene_resources(data,glyphs,baseline,save);assert len(codes)==3834
    packed_glyphs=pack4(glyphs)
    save('CHARIDX.BIN',struct.pack('>H',len(codes))+b''.join(struct.pack('>H',c) for c in codes),
         'Original sorted glyph codes for seven scenes and common dynamic ranges')
    save('CHAR4.BIN',b''.join(packed_glyphs[c*32:c*32+32] for c in codes),'Exact original source-pen glyphs in CHARIDX order')
    records,payload,exported=append_frames(frames,cells,palettes,patches,review_notes,baseline,old_payload)
    save('AUTH4.BIN',payload,'Immutable 88576-byte first212-record R04 prefix plus the native hidden-block lucky item')
    write_index(records);sheets=review_sheets(frames,cells,palettes,patches,exported)
    manifest=dict(baseline)
    manifest.update(version='tour_silk_art_r04_v003',authored_frame_count=len(records),approved_r03_frame_count=112,
        approved_r03_payload_sha256=AUTH_SHA,compact_character_count=len(codes),files=files,frames=records,scenes=scenes,
        r04_frame_counts=dict(puffer_and_shared_burst=52,bird=20,diamond=10,player_climb=5,map_plane=1,ladder=1,secret_bonus=6,puffer_transients=5,hidden_lucky_item=1),
        approved_r04_initial_frame_count=207,approved_r04_initial_payload_bytes=87936,
        approved_r04_initial_payload_sha256=sha(payload[:87936]),
        approved_r04_transient_frame_count=212,approved_r04_transient_payload_bytes=88576,
        approved_r04_transient_payload_sha256=sha(payload[:88576]),
        r04_original_descriptor_count=len(descriptors),r04_pixel_patches_sha256=sha((ART/'r04_pixel_patches.json').read_bytes()),
        r04_original_inventory='r04/original_frame_inventory.json',r04_review_sheets=sheets,
        palette_note='Original per-scene palettes. All R03 records remain unchanged; Java3 diamonds use bank14, puffer bank10, bird bank11/4, ladder bank13.',
        validations={**baseline['validations'],'approved_R03_prefix_exact':True,
            'all_new_indexed_frames_roundtrip_exact':True,'r04_animal_runtime_validated':False,
            'native_emulator_validated':False,'physical_saturn_validated':False})
    manifest['layout']=dict(baseline['layout'],map_scene_indices=[1,5],map_has_gameplay_hud=False)
    manifest['provenance_inputs']=dict(baseline['provenance_inputs'])
    for p in (BASELINE/'r03_AUTH4.BIN',BASELINE/'r03_manifest.json',ART/'build_r04_assets.py',
              ART/'r04_pixel_patches.json',ART/'r04_review_notes.json',BASELINE/'r04_207_AUTH4.BIN',
              BASELINE/'r04_207_manifest.json',BASELINE/'r04_212_AUTH4.BIN',BASELINE/'r04_212_manifest.json'):
        manifest['provenance_inputs'][str(p.relative_to(ROOT))]=dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes()))
    (ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    rows=list(editable['frames'])
    for f in records[112:]:
        row={k:f[k] for k in ('id','source_code','source_width','source_height','width','height','anchor_x','anchor_y',
                              'art_width','role','palette_bank','offset','bytes','indices_sha256')}
        row.update(png=f['id']+'.png',indices=f['id']+'.indices.bin',pixel_map=f['id']+'.pixels.txt');rows.append(row)
    editable.update(source='cd/ART/AUTH4.BIN',source_sha256=sha(payload),frames=rows)
    (EDITABLE/'index.json').write_text(json.dumps(editable,indent=2)+'\n')
    summary=dict(result='BUILT',authored_frames=len(records),authored_bytes=len(payload),new_frames=101,
        original_descriptors=len(descriptors),glyph_count=len(codes),glyph_bytes=len(codes)*32,
        explicit_pen_repairs=sum(len(v) for v in patches.values()),
        repaired_frames=sum(bool(v) for v in patches.values()),approved_R03_records_and_payload_preserved=True,
        scene_count=len(scenes),scene_labels=[s['label'] for s in scenes])
    (ART/'r04_build.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary))


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--review-drafts',action='store_true');args=parser.parse_args()
    WORK.mkdir(exist_ok=True);(WORK/'drafts').mkdir(exist_ok=True)
    data,cells,glyphs=read_sources();frames,descriptors=original_inventory(data)
    base=(C13/'build/wp02/recovered-visual/run-1/palette.bin').read_bytes()
    palettes=dict(java3=rgb444(stage_palette(data,3,base)),silk1=rgb444(stage_palette(data,4,base)),map=rgb444((CD/'MAPPAL.BIN').read_bytes()))
    patches=json.loads((ART/'r04_pixel_patches.json').read_text()) if (ART/'r04_pixel_patches.json').exists() else {}
    notes=json.loads((ART/'r04_review_notes.json').read_text()) if (ART/'r04_review_notes.json').exists() else {}
    for f in frames:
        source=source_frame(cells,f);draft,_=native_grid(source);colors=palette_for(f,palettes)
        indices_png(source,colors,f['palette_bank'],WORK/'drafts'/(f['id']+'.source.png'))
        indices_png(draft,colors,f['palette_bank'],WORK/'drafts'/(f['id']+'.grid.png'))
        (WORK/'drafts'/(f['id']+'.grid.pixels.txt')).write_text(pixel_text(draft))
    (WORK/'original_frame_inventory.json').write_text(json.dumps(frames,indent=2)+'\n')
    (WORK/'original_descriptor_inventory.json').write_text(json.dumps(descriptors,indent=2)+'\n')
    review_sheets(frames,cells,palettes,patches)
    if args.review_drafts:
        print(json.dumps(dict(result='DRAFT_REVIEW',new_frames=len(frames),descriptors=len(descriptors))))
        return
    export(data,cells,glyphs,frames,descriptors,patches,notes,palettes)


if __name__=='__main__':
    main()
