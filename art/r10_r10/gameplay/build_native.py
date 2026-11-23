#!/usr/bin/env python3
"""Losslessly pack reviewed native pen rows. Never import or resize art."""
from pathlib import Path
import hashlib, json

HERE=Path(__file__).resolve().parent
j=json.loads((HERE/'gameplay.rows.json').read_text())
def pack(rows,width,pad,remap=False):
 out=[]
 for row in rows:
  assert len(row)==width
  a=[int(c,16) for c in row]+[pad]*(16-width if remap else 8-width)
  if remap:a=[0 if p==15 else p+1 for p in a]
  out.extend((a[x]<<4)|a[x+1] for x in range(0,len(a),2))
 return bytes(out)
def numbers(blob):return ','.join(f'0x{x:02x}' for x in blob)
terrain_codes=sorted(j['terrain']);item_codes=sorted(j['items']);blobs=[]
terrain_pairs=[];terrain_indices={};pair_indices={};pair_codes=[]
for code in terrain_codes:
 pair=tuple(pack(j['terrain'][code]['widths'][str(w)],w,15) for w in (6,7))
 if pair not in pair_indices:
  pair_indices[pair]=len(terrain_pairs);terrain_pairs.append(pair);pair_codes.append([])
 index=pair_indices[pair];terrain_indices[code]=index;pair_codes[index].append(code)
out=['/* Generated from gameplay.rows.json. Native pixels; original palettes. */',
 '#ifndef SPANG_R10_GAMEPLAY_NATIVE_H','#define SPANG_R10_GAMEPLAY_NATIVE_H','#include <stdint.h>',
 f'#define R10_TERRAIN_COUNT {len(terrain_codes)}u',
 f'#define R10_TERRAIN_PAIR_COUNT {len(terrain_pairs)}u','#define R10_ITEM_COUNT 25u',
 'static const uint8_t r10_terrain_tiles[R10_TERRAIN_PAIR_COUNT][2][32]={']
for codes,pair in zip(pair_codes,terrain_pairs):
 out.append(' { /* '+', '.join('0x'+code for code in codes)+' */')
 for blob in pair:
  blobs.append(blob)
  out.append('  {'+numbers(blob)+'},')
 out.append(' },')
out+=['};','static inline const uint8_t *r10_terrain_pixels(unsigned code,unsigned width)',
 '{',' unsigned index;if(width!=6 && width!=7)return 0;',' switch(code){']
for code in terrain_codes:out.append(f' case 0x{code}:index={terrain_indices[code]};break;')
out+=[' default:return 0;',' }',' return r10_terrain_tiles[index][width-6];','}',
 'static const uint8_t r10_item_tiles[R10_ITEM_COUNT][128]={']
for code in item_codes:
 blob=pack(j['items'][code]['rows'],15,15,True);blobs.append(blob)
 out.append(' {'+numbers(blob)+'}, /* 0x'+code+' '+j['items'][code]['name']+' */')
out+=['};','static inline int r10_item_index(unsigned code)','{',' switch(code){']
for i,code in enumerate(item_codes):out.append(f' case 0x{code}:return {i};')
out+=[' default:return -1;',' }','}','static inline const uint8_t *r10_item_pixels(unsigned code)',
 '{',' int index=r10_item_index(code);return index<0?0:r10_item_tiles[index];','}','#endif','']
(HERE/'gameplay_native.h').write_text('\n'.join(out))
(HERE/'build_report.json').write_text(json.dumps({'terrain_codes':len(terrain_codes),'terrain_variants':len(terrain_codes)*2,
 'terrain_physical_pairs':len(terrain_pairs),'terrain_shared_codes':[codes for codes in pair_codes if len(codes)>1],
 'item_frames':len(item_codes),'terrain_rom_bytes':len(terrain_pairs)*2*32,'item_rom_bytes':len(item_codes)*128,'total_pixel_bytes':sum(map(len,blobs)),
 'rows_sha256':hashlib.sha256((HERE/'gameplay.rows.json').read_bytes()).hexdigest(),
 'header_sha256':hashlib.sha256((HERE/'gameplay_native.h').read_bytes()).hexdigest()},indent=2)+'\n')
print(f'Packed{len(terrain_codes)} terrain codes /{len(terrain_codes)*2} logical variants /'
      f'{len(terrain_pairs)} physical pairs /{len(item_codes)} item frames:{sum(map(len,blobs))} pixel bytes')
