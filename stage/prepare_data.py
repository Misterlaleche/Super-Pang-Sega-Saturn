#!/usr/bin/env python3
"""Reproduce original Tour stages 0..27 and ten map/completion scenes.

Decoded evidence files retain their original byte layout. Linked scene data
uses the included Jo Engine LZSS format and one bounded runtime scene buffer.
"""
import ctypes as C
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
C13 = ROOT / 'c13_reference/fuentes'
sys.path.insert(0, str(C13))
from tools.stage_art import apply, background_writes
from tools.tour_assets import stage_background_writes, stage_hud_writes, stage_palette
from tools.life_assets import life_palette_data
sys.path.insert(0, str(HERE.parent/'Compiler/COMMON'))
import jo_lz

modules = ['actor_motion','geometry','player_control','player_damage','ball_phase','ball_collision',
           'small_ball','round_ball','round_scene','harpoon','weapons','stage_bootstrap','stage_session',
           'round_frame','round_aux','round_rewards','round_clear','round_clock','round_flow',
           'round_bonus_box','round_speed','platform_effect','items','life_sequence','score',
           'score_draw','lives_draw','motion_bonus','bonus_life','video_commands','tour_plane','tour_map']
files = [f'spang_{name}.{ext}' for name in modules for ext in ('c','h')]
files += ['spang_frame_clock.h','spang_sprite_overlay.h','spang_collision_tables.inc']
def write_changed(path, blob):
    """Leave identical production inputs in place during a concurrent build."""
    if not path.exists() or path.read_bytes() != blob:
        path.write_bytes(blob)

provenance = []
patched = []
for name in files:
    source = C13/'src'/name
    blob = source.read_bytes()
    source_sha = hashlib.sha256(blob).hexdigest()
    if name == 'spang_round_frame.c':
        # FIXED09EC..0C81 draws a frozen ball only when visibility byte1 is 1.
        # Pending split children can remain uninitialized on a death frame.
        assert source_sha == 'd4654a85bd101fb74a41017c979a94772dfa9b4ce3b9f59d9f83c692f5e67992'
        old = b' uint8_t *a=v->balls+slot*32;if(!a[0])return 0;\n'
        new = old + b' /* FIXED09EC..0C81: frozen drawing owns visibility byte1, not active byte0. */\n if(!v->advance && a[1]!=1)return 0;\n'
        assert blob.count(old) == 1
        native = blob.replace(old, new)
        patch = ''.join(difflib.unified_diff(blob.decode().splitlines(True), native.decode().splitlines(True),
                                           fromfile='a/'+name, tofile='b/'+name)).encode()
        patch_name = 'patches/spang_round_frame_death_visibility.patch'
        (HERE/'patches').mkdir(exist_ok=True)
        write_changed(HERE/patch_name, patch)
        write_changed(HERE/name, native)
        patched.append(dict(file=name, source=str(source.relative_to(ROOT)), source_sha256=source_sha,
                            native_sha256=hashlib.sha256(native).hexdigest(), source_bytes=len(blob), native_bytes=len(native),
                            patch=patch_name, patch_sha256=hashlib.sha256(patch).hexdigest(),
                            reason='Original FIXED09EC..0C81 frozen ball drawing requires visibility byte1==1; advancing dispatch is unchanged.',
                            evidence='tests/r03/draw_only_oracle/manifest.json; tests/r03/frozen_split_replay.json'))
    elif name == 'spang_round_clear.c':
        # FIXED38FF preserves the ladder's active2. Original BANK0:A8F9
        # initializes its victory pose exactly as it does for other states.
        old = b' if((p[0]!=1 && p[0]!=3 && p[0]!=4 && p[0]!=5) || p[0x11]>1)return -1;\n'
        new = b' /* FIXED38FF retains active2 on a ladder; BANK0:A8F9 then enters victory. */\n if((p[0]!=1 && p[0]!=2 && p[0]!=3 && p[0]!=4 && p[0]!=5) || p[0x11]>1)return -1;\n'
        assert blob.count(old) == 1
        native = blob.replace(old,new)
        patch = ''.join(difflib.unified_diff(blob.decode().splitlines(True), native.decode().splitlines(True),
                                           fromfile='a/'+name, tofile='b/'+name)).encode()
        patch_name = 'patches/spang_round_clear_ladder_victory.patch'
        write_changed(HERE/patch_name,patch)
        write_changed(HERE/name,native)
        patched.append(dict(file=name,source=str(source.relative_to(ROOT)),source_sha256=source_sha,
                            native_sha256=hashlib.sha256(native).hexdigest(),source_bytes=len(blob),native_bytes=len(native),
                            patch=patch_name,patch_sha256=hashlib.sha256(patch).hexdigest(),
                            reason='Original FIXED38FF preserves active2 on ladders; BANK0:A8F9 initializes victory without rejecting that state.',
                            evidence='tests/r07/ladder_clear/red_trace.json; tests/test_r07_ladder_clear.py'))
    elif name in ('spang_tour_plane.c','spang_tour_plane.h','spang_tour_map.c','spang_tour_map.h'):
        # BANK3:8B22 uses both world-indexed tables. Worlds 0,1,2 share
        # the same 2/4/6 controller; subsequent worlds have other handlers.
        # Preserve the reviewed native implementation while regenerating data.
        # Regeneration must never silently revert later routes to C13's world0.
        native = (HERE/name).read_bytes()
        patch = ''.join(difflib.unified_diff(blob.decode().splitlines(True), native.decode().splitlines(True),
                                           fromfile='a/'+name, tofile='b/'+name)).encode()
        patch_name = 'patches/'+name.replace('.','_')+'_r08.patch'
        write_changed(HERE/patch_name, patch)
        write_changed(HERE/name, native)
        patched.append(dict(file=name, source=str(source.relative_to(ROOT)), source_sha256=source_sha,
                            native_sha256=hashlib.sha256(native).hexdigest(), source_bytes=len(blob), native_bytes=len(native),
                            patch=patch_name, patch_sha256=hashlib.sha256(patch).hexdigest(),
                            reason='Original world-indexed flights through world8 and region-indexed marker tables; the separate region owner handles worlds4/9 state8.',
                            evidence='tests/test_r08_stage.py; BANK3:800C,8B30,8CE9,8BF0'))
    else:
        write_changed(HERE/name, blob)
        provenance.append(dict(file=name,source=str(source.relative_to(ROOT)),sha256=source_sha,bytes=len(blob)))

data_dir = HERE/'data'
data_dir.mkdir(exist_ok=True)
data = (C13/'build/wp00/decoded/data.bin').read_bytes()
assert hashlib.sha256(data).hexdigest()=='9d460c57aac71d4ec9e7e8e6d361e259ff789fd7aeee7f545a60c688c63a09f2'
capture = C13/'build/wp02/recovered-visual/run-1'
attrs = bytearray((capture/'attrs.bin').read_bytes())
chars = bytearray((capture/'chars.bin').read_bytes())
palette = bytearray((capture/'palette.bin').read_bytes())
attrs[0x40:0x6c0] = bytes([16])*0x680
chars[0x80:0xd80] = b'\x20\0'*(0xd00//2)
apply(stage_background_writes(data,0),attrs,chars)
apply(stage_hud_writes(data,0),attrs,chars)
palette = stage_palette(data,0,palette)
bank12,bank13 = data[0x40000:0x44000],data[0x44000:0x48000]
word=lambda b,n:int.from_bytes(b[n:n+2],'little')
pal_index=data[0x1f9e]
ready_palette=word(data,0x1fce+pal_index)-0x8000
bonus_palette=word(data,0x1fd4+pal_index)-0x8000
# FIXED3527 dispatch: state0E (Continue) ->3845 requests12h; state10
# (Game Over) ->3876 requests0Ch. FIXED1C4B dispatches each request to
# an LD HL,nn palette source in bank12; keep the distinct original sources.
gameover_palette=word(data,word(data,0x1c4b+12)+1)-0x8000
continue_palette=word(data,word(data,0x1c4b+18)+1)-0x8000
assert gameover_palette==0xa8 and continue_palette==0xc8
warning = b''.join(bank13[word(data,0x1f7a+i)-0x8000:word(data,0x1f7a+i)-0x8000+32] for i in range(0,20,2))
arrays = {'hk_fixed':data[:0x8000]}
for bank in (0,1,2,3,4,5,10):
    arrays[f'hk_bank{bank}'] = data[0x10000+bank*0x4000:0x14000+bank*0x4000]
arrays.update(hk_under_attrs=bytes(attrs),hk_under_chars=bytes(chars),hk_initial_palette=bytes(palette),
              hk_flash_palettes=life_palette_data(data),hk_warning_palettes=warning,
              hk_clear_palette=bank12[0x68:0x88],hk_timeout_palette=bank12[0x88:0xa8],
              hk_gameover_palette=bank12[gameover_palette:gameover_palette+32],
              hk_continue_palette=bank12[continue_palette:continue_palette+32],hk_ready_palette=bank12[ready_palette:ready_palette+32],
              hk_bonus_palette=bank12[bonus_palette:bonus_palette+32],
              hk_clock_tables=data[0x1dba4:0x1dbf4]+data[0x1db9c:0x1dba4]+data[0x1c7f3:0x1c806]+data[0xe4a:0xe50])
# Original phase-specific scene data. The approved R03 arrays remain exact.
stage_prefixes = ((1,'java1'),(2,'java2'),(3,'java3'),(4,'silk1'),(5,'silk2'),(6,'silk3'),(7,'himar1'),
                  (8,'himar2'),(9,'himar3'),(10,'istanbul1'),(11,'istanbul2'),(12,'istanbul3'),
                  (13,'germany1'),(14,'germany2'),(15,'germany3'),(16,'venice1'),(17,'venice2'),
                  (18,'spain1'),(19,'spain2'),(20,'spain3'),(21,'france1'),(22,'france2'),(23,'france3'),
                  (24,'tower1'),(25,'tower2'),(26,'tower3'),(27,'canada1'),(28,'canada2'),(29,'canada3'),(30,'usa1'),(31,'usa2'),(32,'usa3'),
                  (33,'mexico1'),(34,'mexico2'),(35,'brazil1'),(36,'brazil2'),(37,'brazil3'),
                  (38,'argentina1'),(39,'argentina2'))
for phase,prefix in stage_prefixes:
    ar=bytearray((capture/'attrs.bin').read_bytes());ch=bytearray((capture/'chars.bin').read_bytes())
    ar[0x40:0x6c0]=bytes([16])*0x680;ch[0x80:0xd80]=b'\x20\0'*(0xd00//2)
    apply(stage_background_writes(data,phase),ar,ch);apply(stage_hud_writes(data,phase),ar,ch)
    arrays[prefix+'_under_attrs']=bytes(ar);arrays[prefix+'_under_chars']=bytes(ch)
    arrays[prefix+'_initial_palette']=bytes(stage_palette(data,phase,(capture/'palette.bin').read_bytes()))
    pi=data[0x1f9e+phase]
    for kind,table in (('ready',0x1fce),('bonus',0x1fd4)):
        at=word(data,table+pi)-0x8000;arrays[prefix+'_'+kind+'_palette']=bank12[at:at+32]
subprocess.run(['gcc','-std=c11','-O2','-shared','-fPIC',str(HERE/'spang_video_commands.c'),'-o',str(data_dir/'video_host.so')],check=True)
video=C.CDLL(str(data_dir/'video_host.so'))
for prefix,region,parameter in (('map',0,0x4c),('map_java_silk',0,0x4e),('map_silk_himar',0,0x50),
                                ('map_himar_istanbul',0,0x52),('map_asia_complete',0,None),('map_germany_venice',1,0x54),
                                ('map_venice_spain',1,0x56),('map_spain_france',1,0x58),('map_france_tower',1,0x5a),
                                ('map_europe_complete',1,None),('map_canada_usa',2,0x5c),('map_usa_mexico',2,0x5e),
                                ('map_mexico_brazil',2,0x60),('map_brazil_argentina',2,0x62)):
    mp=bytearray((capture/'palette.bin').read_bytes())
    entry=0x50+region*3;at=word(bank12,entry)-0x8000;source_bank=bank12[entry+2]
    source=data[0x10000+source_bank*0x4000:0x14000+source_bank*0x4000]
    mp[0x400:0x600]=source[at:at+512];mp[:512]=data[0x4420c:0x4440c]
    # Rebuild the common background before each name command so a shorter
    # route name cannot leave characters from the preceding map.
    ar=bytearray((capture/'attrs.bin').read_bytes());ch=bytearray((capture/'chars.bin').read_bytes())
    ar[0x40:0x7c0]=bytes([0x23])*0x780;ch[0x80:0xf80]=b'\x0e\x07'*0x780
    apply(background_writes(data,world=region),ar,ch)
    cb=C.CFUNCTYPE(None,C.c_void_p,C.c_uint16,C.c_uint8)(lambda _,a,v:apply([(a,v)],ar,ch))
    commands=((0x1496,0x32),(0x1496,0x2e+region*2)) if parameter is None else ((0x1496,0x8a),(0x11e8,parameter))
    for command,value in commands:
        assert video.spang_video_command(command,value,(C.c_uint8*16384).from_buffer_copy(arrays['hk_bank10']),cb,None)==0
    arrays.update({prefix+'_under_attrs':bytes(ar),prefix+'_under_chars':bytes(ch),prefix+'_initial_palette':bytes(mp)})
# FIXED3A43/10B9/105D: original ending starts with a blank display.
# Its IRQ palette task0D,param18 (FIXED1E8F) replaces C400..C5FF only.
ar=bytearray(arrays['argentina2_under_attrs']);ch=bytearray(arrays['argentina2_under_chars'])
ar[0x40:0x7c0]=bytes([0x10])*0x780;ch[0x80:0xf80]=b'\x20\0'*0x780
mp=bytearray(arrays['argentina2_initial_palette']);mp[0x400:0x600]=bank13[0x40c:0x60c]
arrays.update(ending_under_attrs=bytes(ar),ending_under_chars=bytes(ch),ending_initial_palette=bytes(mp))
scene_labels=['hk','map','java1','java2','java3','map_java_silk','silk1','silk2','silk3','map_silk_himar','himar1',
              'himar2','himar3','map_himar_istanbul','istanbul1','istanbul2','istanbul3','map_asia_complete',
              'germany1','germany2','germany3','map_germany_venice','venice1','venice2',
              'map_venice_spain','spain1','spain2','spain3','map_spain_france','france1','france2','france3',
              'map_france_tower','tower1','tower2','tower3','map_europe_complete','canada1',
              'canada2','canada3','map_canada_usa','usa1','usa2','usa3','map_usa_mexico','mexico1','mexico2',
              'map_mexico_brazil','brazil1','brazil2','brazil3','map_brazil_argentina','argentina1','argentina2','ending']
linked_arrays={k:v for k,v in arrays.items() if not any(k.endswith(s) for s in ('under_attrs','under_chars','initial_palette'))}
def scene_patch(base,raw):
    """Exact literal runs: big-endian offset, length-minus-one, then bytes.

    A world's first scene is immutable and independently compressed. Later
    palettes/HUDs share that base without keeping a second decoded buffer.
    """
    out=bytearray();i=0
    while i<len(raw):
        if base[i]==raw[i]:i+=1;continue
        start=i;i+=1
        while i<len(raw) and i-start<256:
            if all(i+j>=len(raw) or base[i+j]==raw[i+j] for j in range(3)):break
            i+=1
        out+=start.to_bytes(2,'big')+bytes([i-start-1])+raw[start:i]
    return bytes(out)

scene_raw=[];scene_bases=[]
full_bases={0,1,2,6,10,14,18,22,25,29,33,37,40,41,45,48,52}
for index,label in enumerate(scene_labels):
    raw=b''.join(arrays[label+'_'+suffix] for suffix in ('under_attrs','under_chars','initial_palette'))
    packed=jo_lz.compress(raw);assert len(raw)==10240 and jo_lz.decompress(packed)==raw
    base=index
    if index not in full_bases:
        for previous in sorted(full_bases):
            if previous>=index:continue
            delta=scene_patch(scene_raw[previous],raw)
            if len(delta)<len(packed):packed=delta;base=previous
    scene_raw.append(raw);scene_bases.append(base)
    linked_arrays[label+'_scene_data']=packed
header=['#ifndef HK_STAGE_DATA_H','#define HK_STAGE_DATA_H','#include <stdint.h>',
        'typedef struct { const uint8_t *bytes; unsigned length,base; } stage_scene_blob;',
        f'#define STAGE_SCENE_COUNT {len(scene_labels)}u','extern const stage_scene_blob stage_scenes[STAGE_SCENE_COUNT];']
source=['/* Generated original data; native tables only, no CPU emulation. */','#include "stage_data.h"']
for name,blob in linked_arrays.items():
    header.append(f'extern const uint8_t {name}[{len(blob)}];')
    source.append(f'const uint8_t {name}[{len(blob)}]={{')
    source += [' '+','.join(f'0x{x:02x}' for x in blob[p:p+24])+',' for p in range(0,len(blob),24)]
    source.append('};')
source.append('const stage_scene_blob stage_scenes[STAGE_SCENE_COUNT]={')
source.extend(' {'+label+'_scene_data,sizeof '+label+'_scene_data,'+str(scene_bases[i])+'},' for i,label in enumerate(scene_labels))
source.append('};')
header.append('#endif')
write_changed(HERE/'stage_data.h', ('\n'.join(header)+'\n').encode())
write_changed(HERE/'stage_data.c', ('\n'.join(source)+'\n').encode())
for name,blob in arrays.items():
    if any(name.endswith(suffix) for suffix in ('under_attrs','under_chars','initial_palette')):
        (data_dir/(name+'.bin')).write_bytes(blob)

# Earliest platform bootstrap dump for the independent art owner. The runtime
# repeats these same original functions on every life, with mutable tile RAM.
subprocess.run(['gcc','-std=c11','-O2','-shared','-fPIC',str(HERE/'spang_stage_bootstrap.c'),'-o',str(data_dir/'bootstrap_host.so')],check=True)
lib=C.CDLL(str(data_dir/'bootstrap_host.so'))
ar=(C.c_uint8*2048).from_buffer_copy(attrs);ch=(C.c_uint8*4096).from_buffer_copy(chars)
grid=(C.c_uint8*2048)();b5=(C.c_uint8*16384).from_buffer_copy(arrays['hk_bank5'])
lib.spang_stage_grid_clear(grid)
assert lib.spang_stage_platforms(0,grid,ar,ch,b5)==0
for name,blob in [('hk_platform_attrs',bytes(ar)),('hk_platform_chars',bytes(ch)),('hk_platform_grid',bytes(grid))]:
    (data_dir/(name+'.bin')).write_bytes(blob)
for phase,prefix in stage_prefixes:
    ar=(C.c_uint8*2048).from_buffer_copy(arrays[prefix+'_under_attrs']);ch=(C.c_uint8*4096).from_buffer_copy(arrays[prefix+'_under_chars'])
    grid=(C.c_uint8*2048)();lib.spang_stage_grid_clear(grid)
    assert lib.spang_stage_platforms(phase,grid,ar,ch,b5)==0
    for suffix,blob in [('platform_attrs',bytes(ar)),('platform_chars',bytes(ch)),('platform_grid',bytes(grid))]:
        (data_dir/(prefix+'_'+suffix+'.bin')).write_bytes(blob)
report=dict(original_files=provenance,patched_files=patched,table_bytes=sum(map(len,linked_arrays.values())),
            decoded_table_bytes=sum(map(len,arrays.values())),scene_format='Jo Engine LZSS bases and bounded literal patches, 10240-byte output',
            packed_scene_bytes=sum(len(linked_arrays[label+'_scene_data']) for label in scene_labels),
            scene_labels=scene_labels,scene_bases=scene_bases,
            generated_arrays={k:dict(bytes=len(v),sha256=hashlib.sha256(v).hexdigest()) for k,v in arrays.items()},
            table_source='c13_reference/fuentes/build/wp00/decoded/data.bin',
            table_source_sha256=hashlib.sha256(data).hexdigest(),
            native_route=dict(stage_count=40,scene_count=len(scene_labels),terminal_stage=39,
                preserved_R07_scenes=24,
                original_ice='BANK0:AF45,AF93,AFD5,A5E9,A660,A67E; original Z/C preservation retained',
                original_secret6='BANK4:89B2; timer0081 and FCC0&FCC2&08, no single-player replacement',
                ice_evidence='tests/r08/ice_oracle/manifest.json',
                map_evidence='tests/r08/map_oracle/oracle_report.json',
                files={name:hashlib.sha256((HERE/name).read_bytes()).hexdigest()
                       for name in ('stage.c','stage.h','fauna.c','player_vertical.c','spang_tour_region.c')}),
            hud_template_source=str(capture.relative_to(ROOT)),
            construction='C13 tools.tour_assets stage_background_writes/stage_hud_writes/stage_palette; original platform bootstrap. Base HUD template retained as in C13; gameplay state is newly bootstrapped, not replayed.',
            timeout_source='FIXED0DA5,37FD..3843,bank2:9F61/A03B;180logicaltick hold,life debit,then original retry/game-over branch.',
            gameover_palette_source=dict(
                main_dispatch='FIXED3527: state0E->3845 Continue; state10->3876 Game Over',
                gameover='FIXED38A1 request0C -> FIXED1C57 -> FIXED1DE8 LD HL,80A8; bank12 data 80A8..80C7 copied to C000..C01F',
                **{'continue':'FIXED3861 request12 -> FIXED1C5D -> FIXED1DE3 LD HL,80C8; bank12 data 80C8..80E7 copied to C000..C01F'},
                tour_expiry='BANK0:9631 LD HL,E080; LD(HL),10; INC HL; LD(HL),00 enters final Game Over after continue status checks',
                evidence='verification/r10_r9/gameover/palette_provenance.json'),
            reference_files_modified=False)
(HERE/'data_provenance.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(unchanged_files=len(provenance),patched_files=len(patched),table_bytes=report['table_bytes'],art_bootstrap=str(data_dir))))
