#!/usr/bin/env python3
"""Append three original phases, one map and the complete thirteen-rung ladder.

The shipped R05 sprite records and scene files are immutable. Original stage
tables are expanded independently then compared with stage/prepare_data.py.
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
from PIL import Image,ImageDraw

sys.dont_write_bytecode=True
ART=Path(__file__).resolve().parent;P=ART.parent;ROOT=P.parent;C13=ROOT/'c13_reference/fuentes'
CD=P/'cd/ART';WORK=ART/'r06';BASE=ART/'baseline';EDIT=P/'editable_art'
sys.path[:0]=[str(ART),str(C13)]
from build_r04_assets import read_sources,native_grid,pixel_text
from build_art import pack4,unpack4,vdp1_pens,source_pens,indices_png,rgb444,saturn_colors,tile_layer
from tools.stage_art import apply,background_writes
from tools.tour_assets import stage_background_writes,stage_hud_writes,stage_palette
sha=lambda b:hashlib.sha256(bytes(b)).hexdigest()
word=lambda b,p:int.from_bytes(b[p:p+2],'little')

def main():
 WORK.mkdir(exist_ok=True)
 for name,source in [('r05_manifest.json',ART/'manifest.json'),('r05_AUTH4.BIN',CD/'AUTH4.BIN'),('r05_authored_index.h',ART/'authored_index.h'),('r05_editable_index.json',EDIT/'index.json')]:
  if not (BASE/name).exists():shutil.copyfile(source,BASE/name)
 baseline=json.loads((BASE/'r05_manifest.json').read_text());m=dict(baseline);files=dict(m['files'])
 old=(BASE/'r05_AUTH4.BIN').read_bytes();assert len(old)==88704 and sha(old)=='d6729cc0be4999c0d756532856a9c5687c5d4b8f8bf104d323ae74e062c034cf'
 assert len(baseline['frames'])==213 and len(baseline['scenes'])==7
 for name,meta in files.items():
  if name not in ('AUTH4.BIN','CHAR4.BIN','CHARIDX.BIN'):assert sha((CD/name).read_bytes())==meta['sha256'],name
 def save(name,blob,meaning):
  blob=bytes(blob);(CD/name).write_bytes(blob);files[name]=dict(bytes=len(blob),sha256=sha(blob),meaning=meaning)
 data,cells,glyphs=read_sources();capture=C13/'build/wp02/recovered-visual/run-1'
 library=WORK/'scene_sources.so';subprocess.run(['gcc','-shared','-fPIC','-O2','-Wall','-Wextra','-Werror',str(P/'stage/spang_stage_bootstrap.c'),str(P/'stage/spang_video_commands.c'),'-o',str(library)],check=True)
 api=C.CDLL(str(library));u8=C.c_uint8
 bank5=(u8*16384).from_buffer_copy(data[0x24000:0x28000]);bank10=(u8*16384).from_buffer_copy(data[0x38000:0x3c000]);bank12=data[0x40000:0x44000]
 basepal=(capture/'palette.bin').read_bytes();scenes=list(m['scenes']);codes=set()
 for s in scenes:
  for k in ('chars_file','under_chars_file'):codes.update(int(x)&32767 for x in np.frombuffer((CD/s[k]).read_bytes(),'<u2'))
 xm=np.frombuffer((CD/'VIEWX.BIN').read_bytes(),'>u2').reshape(2,320).astype(int);ym=np.frombuffer((CD/'VIEWY.BIN').read_bytes(),'>u2').astype(int)
 for index,phase,prefix,label in [(7,5,'S2','silk2'),(8,6,'S3','silk3'),(9,None,'M3','map_silk_himar'),(10,7,'H1','himar1')]:
  attrs=bytearray((capture/'attrs.bin').read_bytes());chars=bytearray((capture/'chars.bin').read_bytes())
  if phase is None:
   attrs[0x40:0x7c0]=bytes([0x23])*0x780;chars[0x80:0xf80]=b'\x0e\x07'*0x780;apply(background_writes(data),attrs,chars)
   cb=C.CFUNCTYPE(None,C.c_void_p,C.c_uint16,u8)(lambda _,a,v:apply([(a,v)],attrs,chars))
   for command,value in [(0x1496,0x8a),(0x11e8,0x50)]:assert api.spang_video_command(command,value,bank10,cb,None)==0
   pal=bytearray(basepal);p=word(bank12,0x50)-0x8000;pal[0x400:0x600]=bank12[p:p+512];pal[:512]=data[0x4420c:0x4440c]
  else:
   attrs[0x40:0x6c0]=bytes([16])*0x680;chars[0x80:0xd80]=b'\x20\0'*(0xd00//2)
   apply(stage_background_writes(data,phase),attrs,chars);apply(stage_hud_writes(data,phase),attrs,chars);pal=stage_palette(data,phase,basepal)
  ua,uc=bytes(attrs),bytes(chars)
  if phase is not None:
   ar=(u8*2048).from_buffer_copy(attrs);ch=(u8*4096).from_buffer_copy(chars);grid=(u8*2048)();api.spang_stage_grid_clear(grid)
   assert api.spang_stage_platforms(phase,grid,ar,ch,bank5)==0;attrs,chars=bytes(ar),bytes(ch)
  paths=dict(attrs_file=prefix+'ATTR.BIN',chars_file=prefix+'CHARS.BIN',under_attrs_file=prefix+'UATR.BIN',under_chars_file=prefix+'UCHR.BIN',palette_file=prefix+'PAL.BIN',background_file=prefix+'BG.BIN')
  for key,blob in [('attrs_file',attrs),('chars_file',chars),('under_attrs_file',ua),('under_chars_file',uc),('palette_file',pal)]:save(paths[key],blob,'Original '+label+' '+key)
  save(prefix+'UNDER.BIN',ua+uc,'Original '+label+' under-platform tile data')
  source=tile_layer(glyphs,attrs,chars);native=np.empty((240,320),np.uint16)
  for y in range(240):native[y]=source[ym[y],np.arange(320)+96 if phase is None else xm[int(y>=208)]]
  colors=rgb444(pal);colors555=saturn_colors(pal);save(paths['background_file'],colors555[native].astype('>u2').tobytes(),'Native tile-only reference for '+label)
  Image.fromarray(colors[source[8:248,64:448]]).save(WORK/(label+'_original.png'));Image.fromarray(colors[native]).save(WORK/(label+'_native.png'))
  scene_codes={int(v)&32767 for b in (chars,uc) for v in np.frombuffer(b,'<u2')};codes|=scene_codes
  comparisons={}
  checks=[('under_attrs',ua),('under_chars',uc),('initial_palette',bytes(pal))]+([] if phase is None else [('platform_attrs',bytes(attrs)),('platform_chars',bytes(chars))])
  for suffix,blob in checks:
   name=label+'_'+suffix+'.bin';assert (P/'stage/data'/name).read_bytes()==blob,name;comparisons[name]='byte-identical'
  scenes.append(dict(scene_index=index,phase_index=phase,world_index=None if phase is None else data[0x1260+phase],label=label,**paths,character_count=len(scene_codes),stage_data_comparisons=comparisons,native_x_mapping='map_native_1_to_1_x96' if phase is None else 'playfield_then_native_HUD',native_y_mapping='source_y=native_y+8',palette_sha256=sha(pal)))
 for low,high in m['common_character_ranges_inclusive']:codes.update(range(low,high+1))
 codes=sorted(codes);packed=pack4(glyphs)
 save('CHARIDX.BIN',struct.pack('>H',len(codes))+b''.join(struct.pack('>H',c) for c in codes),'Original glyph codes for eleven scenes')
 save('CHAR4.BIN',b''.join(packed[c*32:c*32+32] for c in codes),'Exact original glyph pixels in CHARIDX order')
 # Original BANK5:8031..8099 uses206/207 for six complete16px rows;
 # the odd thirteenth rung uses20E/20F, including transparent lower8px.
 source=np.full((112,32),15,np.uint8)
 for row in range(7):
  for col in range(2):source[row*16:(row+1)*16,col*16:(col+1)*16]=cells[0x206+col+(8 if row==6 else 0)]
 draft,columns=native_grid(source);pixels=np.full((112,32),15,np.uint8);pixels[:,:29]=draft
 approved=np.frombuffer((EDIT/'ladder_206_len6.indices.bin').read_bytes(),np.uint8).reshape(48,32)
 assert np.array_equal(pixels[:48],approved) and np.array_equal(pixels[48:96],approved)
 assert np.all(pixels[104:]==15)
 name='ladder_206_len13';idx=EDIT/(name+'.indices.bin');idx.write_bytes(pixels.tobytes());pal=(P/'stage/data/silk3_initial_palette.bin').read_bytes()
 indices_png(pixels,rgb444(pal),13,EDIT/(name+'.png'));(EDIT/(name+'.pixels.txt')).write_text(pixel_text(pixels))
 indices_png(source,rgb444(pal),13,WORK/(name+'_original.png'))
 runtime=pack4(vdp1_pens(pixels));assert np.array_equal(source_pens(unpack4(runtime)).reshape(112,32),pixels)
 f=dict(id=name,source_code=0x206,source_width=32,source_height=112,source_anchor=[16,56],role=18,kind=6,palette_bank=13,palette_banks=[13],families=['ladder'],source_bank=5,
  descriptors=[dict(address='BANK5:$8031..$8099; BANK5:$9D1A',command='Six full rows206/207 plus odd final row20E/20F')],
  approved_r01=False,approved_r02=False,approved_r03=False,added_r06=True,width=32,height=112,art_width=29,padded_width=32,anchor=[15,56],anchor_x=15,anchor_y=56,transparent_pen=15,runtime_transparent_pen=0,
  source_sha256=sha(source),initial_source_columns=columns.tolist(),source_descriptor='BANK5:$8031..$8099',editable_indices='editable_art/'+name+'.indices.bin',editable_png='editable_art/'+name+'.png',editable_pixel_map='editable_art/'+name+'.pixels.txt',indices_sha256=sha(pixels),offset=len(old),bytes=len(runtime),packed_sha256=sha(runtime),pixel_edits=[],authored_edit_count=0,changed_silhouette_pixels=0,opaque_bbox_exclusive=[5,0,24,104],roundtrip_mismatched_pixels=0,
  visual_review='The twelve upper rungs reproduce two approved six-rung assets byte for byte. The original odd end row retains both rails, final rung and eight transparent lower rows; complete112px source canvas and29px native width.',method='Complete original compound on the established native grid; approved upper rail pixels preserved')
 records=baseline['frames']+[f];payload=old+runtime;save('AUTH4.BIN',payload,'Frozen213-record R05 prefix plus native thirteen-rung ladder')
 header=(BASE/'r05_authored_index.h').read_text();header=re.sub(r'#define ART_AUTHORED_COUNT \d+u',f'#define ART_AUTHORED_COUNT {len(records)}u',header);header=re.sub(r'#define ART_AUTHORED_BYTES \d+u',f'#define ART_AUTHORED_BYTES {len(payload)}u',header)
 values=[f[k] for k in ['source_code','palette_bank','source_width','source_height','width','height','anchor_x','anchor_y','art_width','role','kind']]+[0,16,f['offset'],f['bytes']]
 header=header.replace('};\n#endif','    {'+','.join(map(str,values))+'}, /* '+name+' */\n};\n#endif');(ART/'authored_index.h').write_text(header)
 m.update(version='tour_himar_art_r06_v001',authored_frame_count=len(records),compact_character_count=len(codes),frames=records,files=files,scenes=scenes,r06_original_stage_indices=[5,6,7],r06_added_native_frames=1)
 m['layout']=dict(m['layout'],map_scene_indices=[1,5,9]);(ART/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
 editable=json.loads((BASE/'r05_editable_index.json').read_text());row={k:f[k] for k in ['id','source_code','source_width','source_height','width','height','anchor_x','anchor_y','art_width','role','palette_bank','offset','bytes','indices_sha256']};row.update(png=name+'.png',indices=name+'.indices.bin',pixel_map=name+'.pixels.txt');editable['frames'].append(row);editable['source_sha256']=sha(payload);(EDIT/'index.json').write_text(json.dumps(editable,indent=2)+'\n')
 report=dict(added_scenes=[7,8,9,10],authored_frames=len(records),authored_bytes=len(payload),glyph_count=len(codes),glyph_bytes=len(codes)*32,old_authored_prefix_exact=True,old_scene_files_exact=True,new_stage_data_comparisons=18)
 (WORK/'build_report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

if __name__=='__main__':main()
