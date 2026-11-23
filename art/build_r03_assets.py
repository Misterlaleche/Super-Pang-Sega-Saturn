#!/usr/bin/env python3
"""Append reproducible native-indexed Java, map and crocodile resources.

The released R02 payload is a pinned prefix. New compound sprites are built
as complete source frames, transferred to a fixed grid, then repaired using
reviewed, explicit palette-index edits from r03_pixel_patches.json. Runtime
positions never choose source samples. PNGs and hexadecimal pen maps remain
lossless editable assets rather than baked screenshots.
"""
from pathlib import Path
import argparse
import ctypes as C
import hashlib
import json
import shutil
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
WORK = ART/'r03'
sys.path.insert(0,str(ART))
sys.path.insert(0,str(C13))
from build_art import (pack4,unpack4,vdp1_pens,source_pens,rgb444,saturn_colors,
                       rgba,indices_png,tile_layer,source_compound)
from tools.visual_assets import decode_graphics
from tools.stage_art import background_writes,apply
from tools.tour_assets import stage_background_writes,stage_hud_writes,stage_palette

DATA_SHA = '9d460c57aac71d4ec9e7e8e6d361e259ff789fd7aeee7f545a60c688c63a09f2'
AUTH_SHA = '64b9a8956d3ca8049e20e1ee09120ae19bdb613c7ea486236e7fad4e5ec06d3a'


def sha(blob):
    return hashlib.sha256(blob).hexdigest()


def word(b,p):
    return int.from_bytes(b[p:p+2],'little')


def read_sources():
    decoded=C13/'build/wp00/decoded'
    data=(decoded/'data.bin').read_bytes()
    assert sha(data)==DATA_SHA
    cells=decode_graphics((decoded/'sprites.bin').read_bytes(),True)
    glyphs=decode_graphics((decoded/'chars.bin').read_bytes())
    assert cells.shape==(2048,16,16) and glyphs.shape==(32768,8,8)
    return data,cells,glyphs


def original_animal_inventory(data):
    bank=data[0x20000:0x24000]
    frames={};descriptors=[];p=0xa8b5
    while p<0xa975:
        control,low,high,cell,attr=bank[p-0x8000:p-0x8000+5]
        command=low|(high<<8)
        assert command in (0x21ad,0x2702,0x2635),(hex(p),hex(command))
        code=cell+((attr&0xe0)<<3);width=64 if command==0x2635 else 32
        row={'address':f'BANK4:${p:04X}','control':control,'ticks':control&127,
             'command':f'${command:04X}','source_code':code,'palette_bank':attr&15}
        p+=5
        if control&128:
            row['loop_to']=f'BANK4:${word(bank,p-0x8000):04X}';p+=2
        descriptors.append(row)
        if code not in frames:
            frames[code]=dict(id=f'croc_{code:03x}' if width==32 else f'croc_explosion_{code:03x}',
                source_code=code,palette_bank=attr&15,source_width=width,source_height=32,
                source_anchor=[width//2,16],kind=4,role=17,descriptors=[],source_bank=4)
        frames[code]['descriptors'].append(row)
    assert p==0xa975 and len(descriptors)==36 and len(frames)==29
    schedule=[]
    for phase in (0,1,2):
        index=bank[0x242+phase];p=word(bank,0x26a+index);start=p;events=[]
        while word(bank,p-0x8000):
            q=bank[p-0x8000:p-0x8000+6]
            events.append(dict(address=f'${p:04X}',delay=word(q,0),selector=q[2],variant=q[3],argument=word(q,4)))
            p+=6
        schedule.append(dict(phase=phase,index=index,address=f'${start:04X}',events=events))
    return list(frames.values()),dict(descriptors=descriptors,schedules=schedule)


def original_map_inventory(data):
    bank=data[0x1c000:0x20000];frames=[]
    route=0xcf8+bank[0xce9];descriptor=word(bank,0xd58+bank[route+3]);p=descriptor-0x8000
    assert word(bank,p+1)==0x21ad
    code=bank[p+3]+((bank[p+4]&0xe0)<<3)
    frames.append(dict(id=f'map_plane_{code:03x}',source_code=code,palette_bank=bank[p+4]&15,
        source_width=32,source_height=32,source_anchor=[16,16],kind=5,role=15,
        descriptors=[{'address':f'BANK3:${descriptor:04X}','command':'$21AD','ticks':bank[p]&127}],source_bank=3))
    # Marker layout is defined by original incremental XY offsets. Construct
    # the complete rectangle first; labels/ring have distinct palette groups.
    for marker in (0,2,4,6,8):
        descriptor=word(bank,word(bank,0xe3)-0x8000+marker);p=descriptor-0x8000
        n=bank[p+1];delta=word(bank,p+2)-0x8000;attr=bank[p+4];x=y=0;parts=[]
        for i in range(n):
            dx,dy=bank[delta+2*i:delta+2*i+2]
            x+=dx-256 if dx>=128 else dx;y+=dy-256 if dy>=128 else dy
            parts.append(dict(code=bank[p+5+i]+((attr&0xe0)<<3),x=x,y=y))
        left=min(q['x'] for q in parts);top=min(q['y'] for q in parts)
        width=max(q['x']+16 for q in parts)-left;height=max(q['y']+16 for q in parts)-top
        code=next(q['code'] for q in parts if q['x']==left and q['y']==top)
        frames.append(dict(id=f'map_marker_{code:03x}',source_code=code,palette_bank=attr&15,
            source_width=width,source_height=height,source_anchor=[width//2,height//2],kind=5,role=16,
            descriptors=[{'address':f'BANK3:${descriptor:04X}','marker':marker,'parts':parts,
                          'source_origin_from_actor':[left,top]}],source_bank=3))
    code=bank[0x194]+((bank[0x195]&0xe0)<<3)
    frames.append(dict(id=f'map_selected_{code:03x}',source_code=code,palette_bank=bank[0x195]&15,
        source_width=32,source_height=32,source_anchor=[16,16],kind=5,role=16,
        descriptors=[{'address':'BANK3:$8191','command':'$21AD'}],source_bank=3))
    return frames


def original_ball_variants(data):
    # Ordinary Java type32/20/8/0 and type2 already match the72 approved records.
    # These four alternate original animation frames cover stop/flash variants.
    bank=data[0x14000:0x18000];frames=[]
    for descriptor,width,size in ((0x9a68,16,1),(0x9a8a,16,2),(0x9aac,32,3),(0x9ace,48,4)):
        p=descriptor-0x8000;code=bank[p+3]+((bank[p+4]&0xe0)<<3)
        frames.append(dict(id=f'ball_flash_{code:03x}',source_code=code,palette_bank=bank[p+4]&15,
            source_width=width,source_height=width,source_anchor=[width//2,width//2],
            kind=2,role=1,size_class=size,descriptors=[{'address':f'BANK1:${descriptor:04X}',
                'command':f'${word(bank,p+1):04X}','ticks':bank[p]&127}],source_bank=1))
    return frames


def source_frame(cells,frame):
    parts=frame['descriptors'][0].get('parts')
    if not parts:return source_compound(cells,frame['source_code'],frame['source_width'],frame['source_height'])
    x0,y0=frame['descriptors'][0]['source_origin_from_actor']
    result=np.full((frame['source_height'],frame['source_width']),15,np.uint8)
    for part in reversed(parts):
        x,y=part['x']-x0,part['y']-y0;pixels=cells[part['code']]
        area=result[y:y+16,x:x+16];mask=pixels!=15;area[mask]=pixels[mask]
    return result


def initial_grid(source):
    width=source.shape[1];native_width=(width*200+109)//219
    columns=((2*np.arange(native_width)+1)*width)//(2*native_width)
    return source[:,columns].copy(),columns


def pixel_text(pixels):
    return '\n'.join(''.join('0123456789ABCDEF'[int(p)] for p in row) for row in pixels)+'\n'


def contact_sheet(frames,cells,colors_by_role,patches=None,name='draft_review'):
    columns=4;cell_width=420;cell_height=182;rows=(len(frames)+columns-1)//columns
    image=Image.new('RGB',(columns*cell_width,rows*cell_height),(28,31,36));draw=ImageDraw.Draw(image)
    baseline,old_payload=baseline_inputs() if any(f['role']==1 for f in frames) else (None,None)
    for i,f in enumerate(frames):
        x=(i%columns)*cell_width;y=(i//columns)*cell_height
        source=source_frame(cells,f);draft,_=initial_grid(source);final=draft.copy()
        for edit in (patches or {}).get(f['id'],[]):
            final[edit['y'],edit['x']]=edit['to_pen']
        if f['role']==1:
            reference=next(r for r in baseline['frames'] if r['id']==f"ball_size_{f['size_class']}")
            approved=baseline_frame_pixels(reference,old_payload)[:,:reference['art_width']]
            # The exported alternates retain the approved normal-ball mask;
            # the draft grid alone would misrepresent their final contour.
            final=source[:,1:].copy() if f['source_code']==0x2f3 else np.where(approved!=15,6,15).astype(np.uint8)
            assert np.array_equal(final!=15,approved!=15)
        colors=colors_by_role['map' if f['role'] in (15,16) else 'java']
        draw.text((x+6,y+5),f"{f['id']}  source{source.shape[1]}x{source.shape[0]}  pal{f['palette_bank']}",fill='white')
        variants=[('Source',source),('Grid',draft),('Repaired',final)]
        scale=3 if source.shape[1]<=32 else 2
        for j,(label,pixels) in enumerate(variants):
            xx=x+6+j*138;draw.text((xx,y+23),label,fill=(180,190,205))
            pic=rgba(pixels,colors,f['palette_bank']).resize((pixels.shape[1]*scale,pixels.shape[0]*scale),Image.Resampling.NEAREST)
            image.paste(pic,(xx,y+40),pic)
    image.save(WORK/(name+'.png'))


def baseline_inputs():
    BASELINE.mkdir(exist_ok=True)
    released=ROOT/'native320_r02'
    for local,source in [('r02_manifest.json',released/'art/manifest.json'),
                         ('r02_authored_index.h',released/'art/authored_index.h'),
                         ('r02_AUTH4.BIN',released/'cd/ART/AUTH4.BIN')]:
        if not (BASELINE/local).exists():shutil.copyfile(source,BASELINE/local)
    payload=(BASELINE/'r02_AUTH4.BIN').read_bytes()
    baseline=json.loads((BASELINE/'r02_manifest.json').read_text())
    assert sha(payload)==AUTH_SHA and len(baseline['frames'])==72
    return baseline,payload


def scene_resources(data,glyphs,save):
    build=ART/'build';build.mkdir(exist_ok=True)
    library=build/'r03_scene_sources.so'
    subprocess.run(['gcc','-shared','-fPIC','-O2','-Wall','-Wextra','-Werror',
        str(C13/'src/spang_stage_bootstrap.c'),str(C13/'src/spang_video_commands.c'),
        '-o',str(library)],check=True)
    api=C.CDLL(str(library));u8=C.c_uint8
    capture=C13/'build/wp02/recovered-visual/run-1'
    base_attrs=(capture/'attrs.bin').read_bytes();base_chars=(capture/'chars.bin').read_bytes()
    base_palette=(capture/'palette.bin').read_bytes()
    bank5=(u8*16384).from_buffer_copy(data[0x24000:0x28000])
    bank10=(u8*16384).from_buffer_copy(data[0x38000:0x3c000])
    bank12=data[0x40000:0x44000]
    scenes=[];all_codes=set()
    xplay=64+(6*np.arange(320)+3)//5
    xhud=64+np.where(np.arange(320)<104,(136*np.arange(320)+52)//104,np.arange(320)+32)
    for scene,phase,prefix,label in ((0,0,'HK','hk'),(1,None,'MAP','map'),(2,1,'J1','java1'),(3,2,'J2','java2')):
        attrs=bytearray(base_attrs);chars=bytearray(base_chars)
        if phase is None:
            attrs[0x40:0x7c0]=bytes([0x23])*0x780;chars[0x80:0xf80]=b'\x0e\x07'*0x780
            apply(background_writes(data),attrs,chars)
            callback=C.CFUNCTYPE(None,C.c_void_p,C.c_uint16,u8)(lambda _,a,v:apply([(a,v)],attrs,chars))
            for command,parameter in ((0x1496,0x8a),(0x11e8,0x4c)):
                assert api.spang_video_command(command,parameter,bank10,callback,None)==0
            palette=bytearray(base_palette);p=word(bank12,0x50)-0x8000
            palette[0x400:0x600]=bank12[p:p+512];palette[:512]=data[0x4420c:0x4440c]
        else:
            attrs[0x40:0x6c0]=bytes([16])*0x680;chars[0x80:0xd80]=b'\x20\0'*(0xd00//2)
            apply(stage_background_writes(data,phase),attrs,chars)
            apply(stage_hud_writes(data,phase),attrs,chars)
            palette=stage_palette(data,phase,base_palette)
        under_attrs,under_chars=bytes(attrs),bytes(chars)
        if phase is not None:
            ar=(u8*2048).from_buffer_copy(attrs);ch=(u8*4096).from_buffer_copy(chars);grid=(u8*2048)()
            api.spang_stage_grid_clear(grid)
            assert api.spang_stage_platforms(phase,grid,ar,ch,bank5)==0
            attrs,chars=bytes(ar),bytes(ch)
        else:attrs,chars=bytes(attrs),bytes(chars)
        paths={'attrs_file':prefix+'ATTR.BIN','chars_file':prefix+'CHARS.BIN',
               'under_attrs_file':prefix+'UATR.BIN','under_chars_file':prefix+'UCHR.BIN',
               'palette_file':prefix+'PAL.BIN','background_file':prefix+'BG.BIN'}
        for key,blob in [('attrs_file',attrs),('chars_file',chars),('under_attrs_file',under_attrs),
                         ('under_chars_file',under_chars),('palette_file',palette)]:
            save(paths[key],blob,f'Original {label} '+key.replace('_file','')+'; independent ROM expansion')
        save(prefix+'UNDER.BIN',under_attrs+under_chars,f'Original {label} under-platform attributes then characters')
        source=tile_layer(glyphs,attrs,chars);colors=rgb444(palette);colors555=saturn_colors(palette)
        native=np.empty((240,320),np.uint16)
        for y in range(240):native[y]=source[y+8,xhud if y>=208 and phase is not None else xplay]
        save(paths['background_file'],colors555[native].astype('>u2').tobytes(),
             f'Original {label} tile-only native320 composition; no actors; map uses uniform X over all rows')
        Image.fromarray(colors[source[8:248,64:448]]).save(WORK/(label+'_original_tile_layer.png'))
        Image.fromarray(colors[native]).save(WORK/(label+'_native_tile_layer.png'))
        scene_codes=set(int(c)&32767 for b in (chars,under_chars) for c in np.frombuffer(b,'<u2'))
        all_codes|=scene_codes
        comparisons={}
        checks=[('under_attrs',under_attrs),('under_chars',under_chars),('initial_palette',bytes(palette))]
        if phase is not None:checks += [('platform_attrs',attrs),('platform_chars',chars)]
        for suffix,blob in checks:
            reference=PROJECT/'stage/data'/(label+'_'+suffix+'.bin')
            if reference.exists():
                assert reference.read_bytes()==blob,('Stage/art bootstrap differs',reference)
                comparisons[reference.name]='byte-identical'
        scenes.append(dict(scene_index=scene,phase_index=phase,world_index=None if phase is None else data[0x1260+phase],
             label=label,**paths,character_count=len(scene_codes),stage_data_comparisons=comparisons,
             native_x_mapping='playfield_all_rows' if phase is None else 'playfield_then_native_HUD',
             native_y_mapping='source_y=native_y+8',palette_sha256=sha(bytes(palette))))
    return scenes,all_codes


def baseline_frame_pixels(frame,payload):
    return source_pens(unpack4(payload[frame['offset']:frame['offset']+frame['bytes']])).reshape(frame['height'],frame['width'])


def append_frames(frames,cells,colors,patches,baseline,old_payload):
    payload=bytearray(old_payload);records=list(baseline['frames'])
    for f in frames:
        source=source_frame(cells,f);draft,columns=initial_grid(source)
        final=draft.copy();edits=patches.get(f['id'],[])
        for e in edits:
            assert int(final[e['y'],e['x']])==e['from_pen'],(f['id'],e)
            final[e['y'],e['x']]=e['to_pen']
        if f['role']==1:
            reference=next(r for r in baseline['frames'] if r['id']==f"ball_size_{f['size_class']}")
            approved=baseline_frame_pixels(reference,old_payload);mask=approved!=15
            pens=np.unique(source[source!=15])
            width=reference['width'];art_width=reference['art_width'];height=reference['height']
            padded=np.full((height,width),15,np.uint8)
            if f['source_code']==0x2f3:
                # Tiny flash has eight edge pixels in pen3,36 inner pixels
                # in pen1. The exact one-column shift retains every source
                # pixel on the approved44-pixel contour (same as R02 code31).
                assert np.all(source[:,0]==15)
                padded[:,:15]=source[:,1:]
                assert np.array_equal(padded!=15,mask)
                method='All44 original pixels, including8 edge-shading pixels, translated left1 onto approved contour'
            else:
                # Larger alternate frames are genuinely flat pen6 masks.
                assert pens.tolist()==[6],(f['id'],pens.tolist())
                padded[mask]=6
                method='Original flat pen6 on byte-exact approved ball silhouette'
            anchor_x,anchor_y=reference['anchor_x'],reference['anchor_y']
            extra=dict(approved_contour_id=reference['id'],approved_contour_identical=True,
                       original_opaque_pens=pens.tolist(),method=method)
        else:
            assert edits,(f['id'],'Native animal/map frame needs explicit reviewed repairs')
            height,art_width=final.shape;width=(art_width+7)&~7
            padded=np.full((height,width),15,np.uint8);padded[:,:art_width]=final
            anchor_x=(f['source_width']*100+109)//219;anchor_y=f['source_height']//2
            extra=dict(method='Whole original compound, fixed native index grid, then explicit reviewed feature/contour pen repairs')
        name=f['id'];path=EDITABLE/(name+'.indices.bin');path.write_bytes(padded.tobytes())
        color=colors['map' if f['role'] in (15,16) else 'java']
        png=EDITABLE/(name+'.png');indices_png(padded,color,f['palette_bank'],png)
        txt=EDITABLE/(name+'.pixels.txt');txt.write_text(pixel_text(padded))
        packed=pack4(vdp1_pens(padded));offset=len(payload)
        assert offset%8==0;payload.extend(packed)
        ys,xs=np.where(padded!=15)
        bbox=[int(xs.min()),int(ys.min()),int(xs.max())+1,int(ys.max())+1] if len(xs) else [0,0,0,0]
        record=dict(f,approved_r01=False,approved_r02=False,added_r03=True,
            width=width,height=height,art_width=art_width,padded_width=width,
            anchor=[anchor_x,anchor_y],anchor_x=anchor_x,anchor_y=anchor_y,
            transparent_pen=15,runtime_transparent_pen=0,source_sha256=sha(source.tobytes()),
            initial_source_columns=columns.tolist(),source_descriptor=f['descriptors'][0]['address'],
            source_path=f"native320_r03/art/r03/drafts/{name}.source.png",
            indices=str(path.relative_to(ROOT)),image=str(png.relative_to(ROOT)),
            editable_indices=str(path.relative_to(PROJECT)),editable_png=str(png.relative_to(PROJECT)),
            editable_pixel_map=str(txt.relative_to(PROJECT)),indices_sha256=sha(padded.tobytes()),
            offset=offset,bytes=len(packed),packed_sha256=sha(packed),pixel_edits=edits,
            authored_edit_count=len(edits),changed_silhouette_pixels=int(sum((e['from_pen']==15)!=(e['to_pen']==15) for e in edits)),
            opaque_bbox_exclusive=bbox,roundtrip_mismatched_pixels=0,**extra)
        assert np.array_equal(source_pens(unpack4(packed)).reshape(height,width),padded)
        records.append(record)
    assert bytes(payload[:18560])==old_payload and records[:72]==baseline['frames']
    return records,bytes(payload)


def write_index(records):
    header=['/* Generated R03 indexed frames. Storage padding is never runtime scaling. */',
        '#ifndef SPANG_R03_AUTHORED_INDEX_H','#define SPANG_R03_AUTHORED_INDEX_H','#include <stdint.h>',
        'typedef struct { uint16_t source_code, palette_bank, source_width, source_height, width, height; int16_t anchor_x, anchor_y; uint16_t art_width, role, kind, size_class, flags; uint32_t offset, bytes; } ArtAuthoredRecord;',
        f'#define ART_AUTHORED_COUNT {len(records)}u',f'#define ART_AUTHORED_BYTES {sum(f["bytes"] for f in records)}u',
        '/* Original72 records unchanged; flags4=R03. Roles15mapplane,16mapmarker,17animal. */',
        'static const ArtAuthoredRecord art_authored_records[ART_AUTHORED_COUNT] = {']
    for i,f in enumerate(records):
        values=[f['source_code'],f['palette_bank'],f['source_width'],f['source_height'],f['width'],f['height'],
                f['anchor_x'],f['anchor_y'],f['art_width'],f['role'],f['kind'],f.get('size_class',0),
                4 if i>=72 else (1 if f['approved_r01'] else 2),f['offset'],f['bytes']]
        header.append('    {'+','.join(map(str,values))+'}, /* '+f['id']+' */')
    header.extend(['};','#endif','']);(ART/'authored_index.h').write_text('\n'.join(header))


def export_final(data,cells,glyphs,frames,patches,colors,inventory):
    baseline,old_payload=baseline_inputs();files=dict(baseline['files'])
    def save(name,blob,meaning):
        blob=bytes(blob);(CD/name).write_bytes(blob)
        files[name]=dict(bytes=len(blob),sha256=sha(blob),meaning=meaning)
    scenes,codes=scene_resources(data,glyphs,save)
    raw_chars=pack4(glyphs)
    for low,high in baseline['common_character_ranges_inclusive']:codes.update(range(low,high+1))
    codes=sorted(codes)
    save('CHARIDX.BIN',struct.pack('>H',len(codes))+b''.join(struct.pack('>H',c) for c in codes),
         'BEu16 count then sorted original8x8 character codes for HK/map/Java1/Java2 plus dynamic HUD/platform ranges')
    save('CHAR4.BIN',b''.join(raw_chars[c*32:c*32+32] for c in codes),'Original source-pen glyphs in CHARIDX order; sourcepen15 transparent')
    records,payload=append_frames(frames,cells,colors,patches,baseline,old_payload)
    save('AUTH4.BIN',payload,'Immutable18560-byte R02 prefix; appended native crocodile/map/flash assets; index in authored_index.h')
    write_index(records)
    manifest=dict(baseline)
    manifest.update(version='tour_java_art_r03_v001',authored_frame_count=len(records),
        approved_r02_frame_count=72,approved_r02_payload_sha256=AUTH_SHA,
        compact_character_count=len(codes),files=files,frames=records,scenes=scenes,
        new_frame_counts={'animal':29,'map':7,'ball_flash':4},
        r03_pixel_patches_sha256=sha((ART/'r03_pixel_patches.json').read_bytes()),
        original_animal_catalog='r03/original_animal_inventory.json',
        palette_note='Original per-scene palettes; R02 index masks unchanged. Java1/2 backgrounds and ball colors differ. Crocodilebank5 and explosionbank4 match across the three game stages.',
        validations={**baseline['validations'],'approved_R02_prefix_exact':True,
                     'all_new_indexed_frames_roundtrip_exact':True,'animal_runtime_validated':False,
                     'native_emulator_validated':False,'physical_saturn_validated':False})
    manifest['layout']=dict(baseline['layout'],map_source_x='64+floor((6*x+3)/5) for all240 rows',
        map_scene_index=1,map_has_gameplay_hud=False)
    manifest['provenance_inputs']=dict(baseline['provenance_inputs'])
    for path in [ART/'baseline/r02_AUTH4.BIN',ART/'baseline/r02_manifest.json',ART/'r03_pixel_patches.json',
                 ART/'build_r03_assets.py',C13/'src/spang_tour_map.c',C13/'src/spang_tour_plane.c']:
        manifest['provenance_inputs'][str(path.relative_to(ROOT))]=dict(bytes=path.stat().st_size,sha256=sha(path.read_bytes()))
    (ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    existing=json.loads((EDITABLE/'index.json').read_text());rows=[]
    for i,f in enumerate(records):
        if i<72:rows.append(existing['frames'][i]);continue
        row={k:f[k] for k in ('id','source_code','source_width','source_height','width','height','anchor_x','anchor_y',
                               'art_width','role','palette_bank','offset','bytes','indices_sha256')}
        row.update(png=f['id']+'.png',indices=f['id']+'.indices.bin',pixel_map=f['id']+'.pixels.txt');rows.append(row)
    existing.update(source='cd/ART/AUTH4.BIN',source_sha256=sha(payload),frames=rows)
    (EDITABLE/'index.json').write_text(json.dumps(existing,indent=2)+'\n')
    summary=dict(result='BUILT',authored_frames=len(records),authored_bytes=len(payload),glyph_count=len(codes),
        glyph_bytes=len(codes)*32,new_pixel_repairs=sum(len(v) for v in patches.values()),
        old72_records_and_payload_preserved=True,scenes=[s['label'] for s in scenes])
    (ART/'r03_build.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary))


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--review-drafts',action='store_true');args=parser.parse_args()
    WORK.mkdir(exist_ok=True);(WORK/'drafts').mkdir(exist_ok=True)
    data,cells,glyphs=read_sources()
    animals,animal_inventory=original_animal_inventory(data)
    maps=original_map_inventory(data);balls=original_ball_variants(data)
    frames=animals+maps+balls
    base=(C13/'build/wp02/recovered-visual/run-1/palette.bin').read_bytes()
    java=rgb444(stage_palette(data,1,base));map_pal=bytearray(base)
    map_pal[:512]=data[0x4420c:0x4440c]
    colors={'java':java,'map':rgb444(map_pal)}
    for f in frames:
        source=source_frame(cells,f);draft,columns=initial_grid(source)
        color=colors['map' if f['role'] in (15,16) else 'java']
        indices_png(source,color,f['palette_bank'],WORK/'drafts'/(f['id']+'.source.png'))
        indices_png(draft,color,f['palette_bank'],WORK/'drafts'/(f['id']+'.grid.png'))
        (WORK/'drafts'/(f['id']+'.grid.pixels.txt')).write_text(pixel_text(draft))
    (WORK/'original_animal_inventory.json').write_text(json.dumps(animal_inventory,indent=2)+'\n')
    (WORK/'original_frame_inventory.json').write_text(json.dumps(frames,indent=2)+'\n')
    patches=json.loads((ART/'r03_pixel_patches.json').read_text()) if (ART/'r03_pixel_patches.json').exists() else {}
    contact_sheet(animals,cells,colors,patches,'crocodile_review')
    contact_sheet(maps+balls,cells,colors,patches,'map_ball_review')
    if args.review_drafts:
        print(json.dumps(dict(result='DRAFT_REVIEW',animal_frames=len(animals),map_frames=len(maps),ball_variants=len(balls))))
        return
    export_final(data,cells,glyphs,frames,patches,colors,animal_inventory)


if __name__=='__main__':main()
