#!/usr/bin/env python3
import hashlib,json,mmap,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ISO=ROOT/'game.iso';BIN=ROOT/'game.bin';CUE=ROOT/'game.cue'

def sha_bytes(b): return hashlib.sha256(b).hexdigest()
def sha_path(p):
 digest=hashlib.sha256()
 with p.open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):digest.update(block)
 return digest.hexdigest()

def directory(mm,sector,size):
 blob=mm[sector*2048:sector*2048+size];out={};off=0
 while off<len(blob):
  length=blob[off]
  if not length:off=(off//2048+1)*2048;continue
  rec=blob[off:off+length]
  if length<34 or len(rec)!=length:raise ValueError('invalid directory record')
  n=rec[32];name=bytes(rec[33:33+n])
  if name not in (b'\0',b'\1'):
   key=name.decode('ascii').split(';')[0].rstrip('.')
   out[key]={'sector':int.from_bytes(rec[2:6],'little'),'bytes':int.from_bytes(rec[10:14],'little'),'directory':bool(rec[25]&2)}
  off+=length
 return out

def walk(mm,sector,size,prefix=''):
 out={};entries=directory(mm,sector,size)
 for name,e in entries.items():
  path=f'{prefix}/{name}' if prefix else name
  if e['directory']:out.update(walk(mm,e['sector'],e['bytes'],path))
  else:out[path]=e
 return out

with ISO.open('rb') as f:
 mm=mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ)
 pvd=mm[16*2048:17*2048]
 assert pvd[:7]==b'\x01CD001\x01'
 root_sector=int.from_bytes(pvd[158:162],'little');root_bytes=int.from_bytes(pvd[166:170],'little')
 files=walk(mm,root_sector,root_bytes)
 excluded={line.strip()[2:].upper() for line in (ROOT/'tools/disc_excludes.txt').read_text().splitlines() if line.strip().startswith('*/')}
 expected={}
 for path in (ROOT/'cd').rglob('*'):
  if not path.is_file():continue
  rel=path.relative_to(ROOT/'cd').as_posix().upper()
  if rel not in excluded:expected[rel]=path
 missing=sorted(set(expected)-set(files));extra=sorted(set(files)-set(expected))
 mismatches=[];payloads={}
 for name in sorted(set(expected)&set(files)):
  e=files[name];data=mm[e['sector']*2048:e['sector']*2048+e['bytes']]
  got=sha_bytes(data);want=sha_path(expected[name]);payloads[name]={'bytes':e['bytes'],'sha256':got}
  if got!=want:mismatches.append(name)
 iso_sectors=(len(mm)+2047)//2048
 mm.close()

raw_size=BIN.stat().st_size
assert raw_size%2352==0
raw_sectors=raw_size//2352
assert raw_sectors==iso_sectors,(raw_sectors,iso_sectors)
# Verify every MODE1 payload sector against the ISO, sequentially.
iso_payload=hashlib.sha256();bin_payload=hashlib.sha256();bad=[]
with ISO.open('rb') as fi,BIN.open('rb') as fb:
 for sector in range(iso_sectors):
  a=fi.read(2048);raw=fb.read(2352);b=raw[16:2064]
  if len(a)<2048:a=a+b'\0'*(2048-len(a))
  iso_payload.update(a);bin_payload.update(b)
  if a!=b and len(bad)<16:bad.append(sector)
cue=CUE.read_text()
cue_ok=cue=='FILE "game.bin" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n    POSTGAP 00:02:00\n'
report={'passed':not(missing or extra or mismatches or bad) and cue_ok,
 'iso_sha256':sha_path(ISO),'bin_sha256':sha_path(BIN),'cue_sha256':sha_path(CUE),
 'iso_bytes':ISO.stat().st_size,'bin_bytes':raw_size,'sectors':iso_sectors,
 'iso_payload_sha256':iso_payload.hexdigest(),'bin_payload_sha256':bin_payload.hexdigest(),
 'all_mode1_payload_sectors_equal':not bad,'first_bad_sectors':bad,
 'filesystem_file_count':len(files),'source_cd_file_count':len(expected),
 'missing':missing,'extra':extra,'payload_mismatches':mismatches,'cue_exact':cue_ok,
 'payloads':payloads}
(ROOT/'build-reports').mkdir(parents=True,exist_ok=True)
(ROOT/'build-reports/disc.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='payloads'},indent=2))
raise SystemExit(not report['passed'])
