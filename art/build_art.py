#!/usr/bin/env python3
"""Reproducible Hong Kong resources; writes only art/ and cd/ART/ in R02.

Original ROM-index graphics are decoded once at build time. Approved R01 art is
copied by palette role, never redrawn or resized here. The playfield mapping is
prepared offline; dynamic HUD glyphs retain their original native sample width.
"""
from pathlib import Path
import ctypes as C
import hashlib
import json
import struct
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ART = Path(__file__).resolve().parent
PROJECT = ART.parent
ROOT = PROJECT.parent
C13 = ROOT / 'c13_reference/fuentes'
CD = PROJECT / 'cd/ART'
sys.dont_write_bytecode = True
sys.path.insert(0, str(C13))
from tools.visual_assets import decode_graphics
from tools.stage_art import apply
from tools.tour_assets import stage_background_writes, stage_hud_writes, stage_palette
from tools.life_assets import life_palette_data


def sha(data):
    return hashlib.sha256(data).hexdigest()


def pack4(pixels):
    flat = np.asarray(pixels, dtype=np.uint8).reshape(-1)
    assert len(flat) % 2 == 0 and np.all(flat < 16)
    return ((flat[::2] << 4) | flat[1::2]).tobytes()


def unpack4(data):
    values = np.frombuffer(data, dtype=np.uint8)
    out = np.empty(len(values)*2, dtype=np.uint8)
    out[::2], out[1::2] = values >> 4, values & 15
    return out


def vdp1_pens(pixels):
    """Lossless remap: source transparency15 ->0; opaque source0..14 ->1..15."""
    return np.where(pixels == 15, 0, pixels + 1).astype(np.uint8)


def source_pens(pixels):
    return np.where(pixels == 0, 15, pixels - 1).astype(np.uint8)


def rgb444(palette):
    words = np.frombuffer(palette, dtype='<u2')
    return np.stack([(words >> 8)&15, (words >> 4)&15, words&15], axis=1).astype(np.uint8)*17


def saturn_colors(palette):
    words = np.frombuffer(palette, dtype='<u2').astype(np.uint16)
    ex = lambda n: (n << 1) | (n >> 3)
    return (0x8000 | ex((words >> 8)&15) | (ex((words >> 4)&15) << 5) | (ex(words&15) << 10)).astype(np.uint16)


def rgba(pixels, colors, bank):
    rgb = colors[bank*16 + pixels]
    return Image.fromarray(np.concatenate([rgb, np.where(pixels == 15, 0, 255).astype(np.uint8)[...,None]], axis=2), mode='RGBA')


def indices_png(pixels, colors, bank, path):
    image = Image.fromarray(pixels.astype(np.uint8), mode='P')
    image.putpalette(colors[bank*16:bank*16+16].reshape(-1).tolist()+[0]*720)
    image.save(path, transparency=15)


def native_sources():
    build = ART / 'build'
    build.mkdir(exist_ok=True)
    sources = [C13/'src/spang_stage_bootstrap.c', C13/'src/spang_round_clear.c']
    library = build / 'art_sources.so'
    subprocess.run(['gcc','-shared','-fPIC','-O2','-Wall','-Wextra',*[str(p) for p in sources],'-o',str(library)], check=True)
    return C.CDLL(str(library))


def bootstrap_hk(data, library):
    base = C13 / 'build/wp02/recovered-visual/run-1'
    attrs = bytearray((base/'attrs.bin').read_bytes())
    chars = bytearray((base/'chars.bin').read_bytes())
    # Same original playfield clear and persistent-HUD initialization as C13.
    attrs[0x40:0x6c0] = bytes([16])*0x680
    chars[0x80:0xd80] = b'\x20\0'*(0xd00//2)
    apply(stage_background_writes(data, 0), attrs, chars)
    apply(stage_hud_writes(data, 0), attrs, chars)
    under = bytes(attrs + chars)
    a = (C.c_uint8*2048).from_buffer_copy(attrs)
    c = (C.c_uint8*4096).from_buffer_copy(chars)
    grid = (C.c_uint8*2048)()
    bank5 = (C.c_uint8*16384).from_buffer_copy(data[0x24000:0x28000])
    assert library.spang_stage_platforms(0, grid, a, c, bank5) == 0
    pal = bytes(stage_palette(data, 0, (base/'palette.bin').read_bytes()))
    return bytes(a), bytes(c), under, pal, bytes(grid)


def tile_layer(glyphs, attrs, chars):
    frame = np.zeros((256,512),dtype=np.uint16)
    for n, code in enumerate(np.frombuffer(chars,dtype='<u2')):
        attr = attrs[n]
        pixels = glyphs[int(code)&32767]
        if attr&128:
            pixels = pixels[:,::-1]
        ty, tx = divmod(n,64)
        area = frame[ty*8:ty*8+8,tx*8:tx*8+8]
        opaque = pixels != 15
        area[opaque] = (attr&127)*16 + pixels[opaque].astype(np.uint16)
    return frame


def source_compound(cells, code, width, height):
    pixels = np.full((height,width),15,dtype=np.uint8)
    for y in range(height//16):
        for x in range(width//16):
            pixels[y*16:y*16+16,x*16:x*16+16] = cells[code+y*8+x]
    return pixels


def extra_player_frames(cells, colors):
    definitions = [
        ('p1_death_left',128,32,29,32,15,16,'bank0:AE9B route2702'),
        ('p1_death_right',242,32,29,32,15,16,'bank0:AEA0 route2702'),
        ('p1_victory',224,48,44,48,22,16,'bank0:AEA5 route246F'),
    ]
    patch_path = ART/'supplemental_player_patches.json'
    patches = json.loads(patch_path.read_text()) if patch_path.exists() else {}
    out = ART/'supplemental_player'
    out.mkdir(exist_ok=True)
    result = []
    review = Image.new('RGB',(1300,800),(37,40,46))
    draw=ImageDraw.Draw(review)
    font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf',13)
    for row,(name,code,source_width,art_width,padded_width,ax,ay,descriptor) in enumerate(definitions):
        source = source_compound(cells,code,source_width,32)
        xs = ((2*np.arange(art_width)+1)*source_width)//(2*art_width)
        # The original death poses are exact mirrors. Mirror the sampling phase
        # too: an odd-width target must not lose different details by direction.
        if name == 'p1_death_right':
            xs = source_width - 1 - xs[::-1]
        draft = source[:,xs].copy()
        adapted = draft.copy()
        edits=[]
        for item in patches.get(name,[]):
            x,y,new,reason=item
            new=int(new,16) if isinstance(new,str) else new
            old=int(adapted[y,x]);assert old != new,(name,x,y,'no-op')
            assert 0 <= new <= 15
            adapted[y,x]=new
            edits.append(dict(x=x,y=y,from_pen=old,to_pen=new,reason=reason))
        padded=np.full((32,padded_width),15,dtype=np.uint8);padded[:,:art_width]=adapted
        (out/(name+'.indices.bin')).write_bytes(padded.tobytes())
        (out/(name+'.pixels.txt')).write_text('\n'.join(''.join('0123456789ABCDEF'[int(p)] for p in line) for line in adapted)+'\n')
        indices_png(padded,colors,2,out/(name+'.png'))
        indices_png(source,colors,2,out/(name+'.source.png'))
        indices_png(draft,colors,2,out/(name+'.draft.png'))
        (out/(name+'.draft_pixels.txt')).write_text('\n'.join(''.join('0123456789ABCDEF'[int(p)] for p in line) for line in draft)+'\n')
        for col,(label,pix) in enumerate((('source',source),('grid draft',draft),('authored',adapted))):
            xx,yy=col*425+12,row*265+24
            draw.text((xx,yy),f'{name} / {label}',font=font,fill='white')
            im=rgba(pix,colors,2);im=im.resize((pix.shape[1]*6,192),Image.Resampling.NEAREST)
            review.paste(im,(xx,yy+27),im)
        result.append(dict(id=name,source_code=code,palette_bank=2,width=padded_width,art_width=art_width,
            height=32,anchor_x=ax,anchor_y=ay,source_width=source_width,source_height=32,
            source_anchor=[source_width//2,16],source_descriptor=descriptor,
            sampling_phase='mirrored death-left phase' if name == 'p1_death_right' else 'pixel-center transfer',
            indices=str((out/(name+'.indices.bin')).relative_to(ROOT)),
            method='Source-index draft plus explicit authored palette-role repairs',
            kind=1,approved_r01=False,source_sha256=sha(source.tobytes()),
            authored_edit_count=len(edits),pixel_edits=edits,pixels=padded))
    review.save(ART/'supplemental_player_review.png')
    return result


def hongkong_ball_frames(cells, colors, approved):
    """Keep the approved tiny-ball contour with the actual HK code31 shading.

    Source15 and31 have identical masks but are not related by one pen
    permutation. The approved native mask is precisely that source mask
    translated left one pixel. Every code31 opaque pixel can therefore be
    transferred without discarding or inventing detail.
    """
    reference = next(f for f in approved if f['kind'] == 2 and f['size_class'] == 1)
    original = cells[31]
    target = np.full((16,16),15,dtype=np.uint8)
    target[:,:15] = original[:,1:]
    reference_mask = reference['pixels'] != 15
    assert np.array_equal(target != 15, reference_mask)
    assert np.array_equal(cells[15] != 15, original != 15)
    assert np.all(original[:,0] == 15)
    assert int(np.sum(target != 15)) == int(np.sum(original != 15)) == 44
    permutation = {str(p):sorted(set(int(v) for v in original[cells[15] == p])) for p in range(16)}
    assert any(len(values)>1 for values in permutation.values())
    out=ART/'hongkong_balls';out.mkdir(exist_ok=True)
    name='ball_size_1_hk_code31'
    path=out/(name+'.indices.bin');path.write_bytes(target.tobytes())
    indices_png(target,colors,15,out/(name+'.png'))
    indices_png(original,colors,15,out/(name+'.source.png'))
    (out/(name+'.pixels.txt')).write_text('\n'.join(''.join('0123456789ABCDEF'[int(p)] for p in row) for row in target)+'\n')
    spans=[int(np.sum(row)) for row in reference_mask if np.any(row)]
    occupied_y,occupied_x=np.where(reference_mask)
    fallback=original[:,((2*np.arange(15)+1)*16)//30]
    fallback_y,fallback_x=np.where(fallback!=15)
    proof={'source_code':31,'approved_shape_id':reference['id'],
        'original_source_anchor':[8,8],'native_anchor':[7,8],
        'source_lookup_for_native_pixel':'source_x=native_x+1;source_y=native_y',
        'approved_mask_identical':True,'original_opaque_pixels_preserved':44,
        'native_opaque_pixels':44,'native_opaque_bounds_inclusive':[int(occupied_x.min()),int(occupied_y.min()),int(occupied_x.max()),int(occupied_y.max())],
        'native_row_spans':spans,'lost_source_opaque_pixels':0,
        'previous_CELL15_fallback_opaque_pixels':int(np.sum(fallback!=15)),
        'previous_CELL15_fallback_opaque_width':int(fallback_x.max()-fallback_x.min()+1),
        'previous_CELL15_fallback_opaque_height':int(fallback_y.max()-fallback_y.min()+1),
        'additional_manual_pen_repairs':0,
        'pen15_to31_correspondence':permutation,
        'pen_permutation_possible':False,
        'method':'Exact original31 shading transfer onto the approved class1 contour and anchor; no additional manual repair was necessary.',
        'source_sha256':sha(original.tobytes()),'approved_shape_sha256':sha(reference['pixels'].tobytes()),
        'final_indices_sha256':sha(target.tobytes())}
    (out/'geometry_proof.json').write_text(json.dumps(proof,indent=2)+'\n')
    review=Image.new('RGB',(960,310),(37,40,46));draw=ImageDraw.Draw(review)
    font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf',13)
    for col,(label,pix) in enumerate((('Approved class1 shape/penmap',reference['pixels']),('Original Hong Kong code31',original),('Native code31 / same shape',target))):
        xx=col*320+12;draw.text((xx,14),label,font=font,fill='white')
        im=rgba(pix,colors,15).resize((256,256),Image.Resampling.NEAREST)
        review.paste(im,(xx,40),im)
    review.save(out/'variant_review.png')
    return [dict(id=name,source_code=31,palette_bank=15,width=16,art_width=15,height=16,
        anchor_x=7,anchor_y=8,source_width=16,source_height=16,size_class=1,kind=2,
        approved_r01=False,indices=str(path.relative_to(ROOT)),source_sha256=proof['source_sha256'],
        method=proof['method'],approved_shape_id=reference['id'],
        approved_shape_sha256=proof['approved_shape_sha256'],authored_edit_count=0,pixel_edits=[],
        geometry_proof=proof,pixels=target)]


def catalog_messages(data, cells, colors, library):
    out=ART/'message_references';out.mkdir(exist_ok=True)
    ready=np.concatenate([cells[0x70a+n] for n in range(6)],axis=1)
    rgba(ready,colors,0).save(out/'ready_original.png')
    actor=(C.c_uint8*32)();actor[6]=0x31;actor[7]=0x8a;actor[10]=96;actor[13]=24
    objects=(C.c_uint8*4096)();cursor=C.c_uint16();remaining=C.c_uint8(127)
    bank=(C.c_uint8*16384).from_buffer_copy(data[0x1c000:0x20000])
    count=library.spang_round_clear_sprite(actor,bank,objects,C.byref(cursor),C.byref(remaining))
    assert count==38
    records=[]
    for off in range(0,cursor.value,32):
        attr=objects[off+1];code=objects[off]+((attr&0xe0)<<3)
        x=objects[off+3]+((attr&16)<<4);y=((objects[off+2]+8)&255)-8
        records.append(dict(source_code=code,palette_bank=attr&15,x=x,y=y))
    x0=min(r['x'] for r in records);y0=min(r['y'] for r in records)
    width=max(r['x'] for r in records)+16-x0;height=max(r['y'] for r in records)+16-y0
    image=Image.new('RGBA',(width,height),(0,0,0,0))
    clear_palette=bytearray(4096);clear_palette[:32]=data[0x40068:0x40088]
    clear_colors=rgb444(clear_palette)
    for r in records[::-1]:
        tile=rgba(cells[r['source_code']],clear_colors,r['palette_bank'])
        image.paste(tile,(r['x']-x0,r['y']-y0),tile)
    image.save(out/'stage_clear_original.png')
    return {'ready':{'source_codes':list(range(0x70a,0x710)),'palette_bank':0,
        'source_dimensions':[96,16],'source_anchor':[0,0],'initial_world_xy':[208,112]},
        'stage_clear':{'composition':records,'source_dimensions':[width,height],
        'source_origin':[x0,y0],'actor_reference':[96,24],
        'palette_override_note':'Original fixed command uploads bank12:8068 to palette0 for the clear banner.',
        'palette_xrgb444le_hex':data[0x40068:0x40088].hex()}}


def main():
    CD.mkdir(parents=True,exist_ok=True)
    decoded=C13/'build/wp00/decoded'
    data=(decoded/'data.bin').read_bytes()
    source_files={p.name:{'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in [decoded/'data.bin',decoded/'chars.bin',decoded/'sprites.bin']}
    provenance_paths = [
        C13/'tools/visual_assets.py', C13/'tools/stage_art.py',
        C13/'tools/tour_assets.py', C13/'tools/life_assets.py',
        C13/'src/spang_stage_bootstrap.c', C13/'src/spang_round_clear.c',
        *[(C13/'build/wp02/recovered-visual/run-1'/name) for name in ('attrs.bin','chars.bin','palette.bin')],
        ROOT/'art320/target_v001/player/player_manifest.json',
        ROOT/'art320/target_v001/balls/manifest.json',
        ROOT/'art320/target_v001/rope/manifest.json',
    ]
    provenance_inputs = {str(p.relative_to(ROOT)):{'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())}
        for p in provenance_paths}
    assert sha(data)=='9d460c57aac71d4ec9e7e8e6d361e259ff789fd7aeee7f545a60c688c63a09f2'
    glyphs=decode_graphics((decoded/'chars.bin').read_bytes())
    cells=decode_graphics((decoded/'sprites.bin').read_bytes(),True)
    assert (glyphs.shape,cells.shape)==((32768,8,8),(2048,16,16))
    raw_chars,raw_cells=pack4(glyphs),pack4(cells)
    assert raw_chars==(C13/'build/wp02/native-renderer-assets/chars.n4').read_bytes()
    assert raw_cells==(C13/'build/wp02/native-renderer-assets/sprites.n4').read_bytes()
    library=native_sources()
    attrs,chars,under,palette,grid=bootstrap_hk(data,library)
    colors=rgb444(palette);colors555=saturn_colors(palette)
    files={}
    def save(name,blob,meaning):
        blob=bytes(blob);(CD/name).write_bytes(blob)
        files[name]={'bytes':len(blob),'sha256':sha(blob),'meaning':meaning}
    save('HKPAL.BIN',palette,'2048 original xRGB444 little-endian words, full Hong Kong stage0 palette')
    save('COLORS.BIN',colors555.astype('>u2').tobytes(),'2048 RGB555 big-endian words in original palette-index order')
    clut=np.zeros((16,16),dtype=np.uint16);clut[:,1:]=colors555[:256].reshape(16,16)[:,:15]
    save('CLUT.BIN',clut.astype('>u2').tobytes(),'16 VDP1 CLUTs; each16 BEu16 colors; index0 transparent, indices1..15 sourcepens0..14')
    save('HKATTR.BIN',attrs,'Original2048 tile attributes after actual HK platform bootstrap')
    save('HKCHARS.BIN',chars,'Original2048 character codes as4096 little-endian bytes')
    save('HKUNDER.BIN',under,'2048attrs then4096chars before platforms, for original background restoration')
    save('CELLS.N4',pack4(vdp1_pens(cells)),'2048 exact-source16x16 cells;128bytes/cell; high nibble left; lossless VDP1 pen remap')
    x15=((2*np.arange(15)+1)*16)//30
    native_cells=np.zeros((2048,16,16),dtype=np.uint8);native_cells[:,:,:15]=vdp1_pens(cells[:,:,x15])
    save('CELL15.N4',pack4(native_cells),'Fallback cell drafts:15x16 art,padded16,128bytes/cell; do not compose large actors by independently shrinking cells')
    codes=set(range(0x200))|set(range(0x600,0x800))|set(range(0xa00,0xc00))
    for raw in (chars,under[2048:]):codes.update(int(c)&32767 for c in np.frombuffer(raw,dtype='<u2'))
    codes=sorted(codes)
    save('CHARIDX.BIN',struct.pack('>H',len(codes))+b''.join(struct.pack('>H',c) for c in codes),'BEu16 count then sortedBEu16 original8x8 character codes')
    save('CHAR4.BIN',b''.join(raw_chars[c*32:c*32+32] for c in codes),'32bytes per8x8 glyph inCHARIDX order; originalsource pens0..15, pen15 transparent; high nibble left')
    save('FNT4.BIN',raw_chars[:0x80*32],'Original character codes0..127,32bytes/8x8 glyph; originalpens; optional directfont lookup')
    save('FLASH444.BIN',life_palette_data(data),'Original mode0 phases0,2,4;32raw xRGB444LE bytes perphase, for logicalC3E0 palette range')
    save('CLEARPAL.BIN',data[0x40068:0x40088],'Original clear-banner palette0 upload:16 xRGB444LE words frombank12:8068')

    original=tile_layer(glyphs,attrs,chars)
    Image.fromarray(colors[original[8:248,64:448]]).save(ART/'hongkong_original_tile_layer.png')
    source_x_play=64+(6*np.arange(320)+3)//5
    source_x_hud=64+np.where(np.arange(320)<104,(136*np.arange(320)+52)//104,np.arange(320)+32)
    source_y=np.arange(240)+8
    composed=np.empty((240,320),np.uint16)
    for y in range(240):composed[y]=original[y+8,source_x_hud if y>=208 else source_x_play]
    Image.fromarray(colors[composed]).save(ART/'hongkong_native_composition.png')
    save('HKBG.BIN',colors555[composed].astype('>u2').tobytes(),'Static320x240 RGB555BE tile-only bootstrap; no actors. Offlineplayfield reduction and native-widthHUD layout, not a handredrawn background')
    save('VIEWX.BIN',np.stack([source_x_play,source_x_hud]).astype('>u2').tobytes(),'Two320-entry BEu16 absolute source-X maps: rows0..207 playfield;208..239 native-HUD composition')
    save('VIEWY.BIN',source_y.astype('>u2').tobytes(),'240 BEu16 absolute sourceY entries; nativey0..239 maps to originaly8..247')

    approved=[]
    manifest_sources=[(ROOT/'art320/target_v001/player/player_manifest.json',1),
                      (ROOT/'art320/target_v001/balls/manifest.json',2),
                      (ROOT/'art320/target_v001/rope/manifest.json',3)]
    for manifest_path,kind in manifest_sources:
        source_manifest=json.loads(manifest_path.read_text())
        for f in source_manifest['frames']:
            path=manifest_path.parent/f['indices'];blob=path.read_bytes()
            pixels=np.frombuffer(blob,np.uint8).reshape(f['height'],f['width'])
            assert np.all(pixels<=15) and f['width']%8==0
            source_width = 32 if kind == 1 else (16 if kind == 3 else [16,16,32,48,64][f['size_class']-1])
            approved.append(dict(f,id=f['id'],kind=kind,approved_r01=True,
                source_width=source_width,source_height=32 if kind==1 else source_width,
                source_path=str(path.relative_to(ROOT)),source_sha256=sha(blob),pixels=pixels))
    assert len(approved)==68,(len(approved),'R01 approved frame count changed')
    # Append new records so every previously locked offset remains stable.
    records=approved+extra_player_frames(cells,colors)+hongkong_ball_frames(cells,colors,approved)
    payload=bytearray();metadata=[]
    for f in records:
        while len(payload)%8:payload.append(0)
        packed=pack4(vdp1_pens(f['pixels']));offset=len(payload);payload.extend(packed)
        decoded_again=source_pens(unpack4(packed)).reshape(f['height'],f['width'])
        assert np.array_equal(decoded_again,f['pixels'])
        m={k:v for k,v in f.items() if k not in ('pixels','pixel_edits')}
        m.update(offset=offset,bytes=len(packed),packed_sha256=sha(packed),runtime_transparent_pen=0,
            roundtrip_mismatched_pixels=0,role={1:2,2:1,3:3}[f['kind']])
        if 'pixel_edits' in f:m['pixel_edits']=f['pixel_edits']
        metadata.append(m)
    save('AUTH4.BIN',payload,'ApprovedR01 fullframes plus additionalP1; offsets/layout inart/authored_index.h;4bpp VDP1 remap;8byte-aligned starts')

    header=['/* Generated by art/build_art.py. Width is padded storage width; no runtime art scaling. */',
        '#ifndef SPANG_R02_AUTHORED_INDEX_H','#define SPANG_R02_AUTHORED_INDEX_H','#include <stdint.h>',
        'typedef struct { uint16_t source_code, palette_bank, source_width, source_height, width, height; int16_t anchor_x, anchor_y; uint16_t art_width, role, kind, size_class, flags; uint32_t offset, bytes; } ArtAuthoredRecord;',
        f'#define ART_AUTHORED_COUNT {len(metadata)}u',f'#define ART_AUTHORED_BYTES {len(payload)}u',
        '/* role1=ball,2=player,3=weapon; kind1=P1,2=ball,3=rope; flags1=approvedR01,2=supplementalR02 */',
        'static const ArtAuthoredRecord art_authored_records[ART_AUTHORED_COUNT] = {']
    for f in metadata:
        values=[f['source_code'],f['palette_bank'],f['source_width'],f['source_height'],f['width'],f['height'],f['anchor_x'],f['anchor_y'],
            f.get('art_width',f['width']),f['role'],f['kind'],f.get('size_class',0),1 if f['approved_r01'] else 2,f['offset'],f['bytes']]
        header.append('    {'+','.join(map(str,values))+'}, /* '+f['id']+' */')
    header.extend(['};','#endif',''])
    (ART/'authored_index.h').write_text('\n'.join(header))
    messages=catalog_messages(data,cells,colors,library)
    layout={'native':[320,240],'original_visible':[64,8,384,240],
        'playfield_rows':[0,208],'hud_rows':[208,240],'source_y':'native_y+8',
        'viewx_file':'VIEWX.BIN','playfield_source_x':'64+floor((6*x+3)/5)',
        'hud_source_x':'64+floor((136*x+52)/104) if x<104 else x+96',
        'hud_note':'The source136-pixel P1/lives block is allocated104 pixels; remaining original text samples use1:1 native pixels. Rightmost unused32 sourcepixels are omitted.',
        'background_method':'Build-time tile-only sampling plus separate HUD composition. Dynamic updates use exact original glyph indices and this fixed address map; no complete runtime frame resampling.',
        'dynamic_update_note':'A changed source tile affects the native x columns whoseVIEWX entry lies within its8px source range; native_y=source_y-8. Read attribute bit7 as horizontal flip and low7 as palette bank. Pen15 uses global backdrop index0.'}
    (ART/'native_layout.json').write_text(json.dumps(layout,indent=2)+'\n')
    manifest={'version':'hongkong_art_r02_v001','tour_phase':0,'world':int(data[0x1260]),'region':int(data[0x128a+data[0x1260]]),
        'source_files':source_files,'provenance_inputs':provenance_inputs,
        'graphics_decoder':'C13 tools.visual_assets.decode_graphics; fresh packed output equals pinned chars.n4/sprites.n4',
        'source_cell_geometry':[2048,16,16],'source_character_geometry':[32768,8,8],
        'compact_character_count':len(codes),'common_character_ranges_inclusive':[[0,511],[1536,2047],[2560,3071]],
        'approved_r01_frame_count':len(approved),'authored_frame_count':len(metadata),
        'palette_note':'HK palette2/P1 and3/rope match R01. HK palette15 is green; approvedball pen maps are preserved while original stage palette roles determine colors.',
        'vdp1_end_code_note':'Texturepen15 is a validopaque sourcepen14 after remap. SetVDP1 END_CODE_DISABLE for these4bpp textures.',
        'files':files,'frames':metadata,'messages':messages,'layout':layout,
        'validations':{'raw_decoded_graphics_match_C13_packed':True,'approved_R01_pixel_roundtrip_exact':True,
            'stage_bootstrap_source':'C13 stage_background_writes/stage_hud_writes/stage_palette + spang_stage_platforms phase0',
            'native_background_contains_objects':False,'physical_saturn_validated':False}}
    (ART/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({'characters':len(codes),'authored_frames':len(metadata),'authored_bytes':len(payload),
        'files':{k:v['bytes'] for k,v in files.items()},'additional_frame_edit_counts':{f['id']:f['authored_edit_count'] for f in metadata if not f['approved_r01']}},indent=2))


if __name__=='__main__':
    from build_r04_assets import main as build_r04
    build_r04()
