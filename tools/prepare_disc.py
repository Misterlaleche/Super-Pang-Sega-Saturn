#!/usr/bin/env python3
"""Keep source/reference art intact while shipping the seven runtime ART assets.

Jo's directory table holds256 entries, including dot records. R09's complete
authoring collection exceeds that bound; the runtime reads only these seven files.
The generated mkisofs exclusions affect the ISO, never the source collection.
"""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
REQUIRED={'AUTH4.BIN','CELLH4.BIN','CELLT4.BIN','CHARDF.BIN','CHARMDF.BIN','VIEWX.BIN','VIEWY.BIN'}

def main():
    source=ROOT/'cd/ART';files={p.name:p for p in source.iterdir() if p.is_file()}
    missing=REQUIRED-files.keys()
    if missing:raise ValueError('Missing runtime art: '+','.join(sorted(missing)))
    if any(p.is_dir() for p in source.iterdir()):raise ValueError('Unexpected ART subdirectory')
    if any(not re.fullmatch(r'[A-Z0-9_]+\.[A-Z0-9]+',name) for name in files):
        raise ValueError('Unexpected name in the generated ART catalog')
    lines=['*/ART/'+name for name in sorted(files.keys()-REQUIRED)]
    payload='\n'.join(lines)+'\n'
    output=ROOT/'tools/disc_excludes.txt'
    if not output.exists() or output.read_text()!=payload:output.write_text(payload)
    print(f'ISO ART: {len(REQUIRED)} runtime files; {len(lines)} source/reference files retained outside image')

if __name__=='__main__':main()
