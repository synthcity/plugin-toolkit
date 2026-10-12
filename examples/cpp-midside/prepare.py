#!/usr/bin/env python3
"""Reproducible extraction for reviewed sources, NOT a general C++ converter."""
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parent
manifest=json.loads((root/'sources.json').read_text())
out=root/'build'
out.mkdir(exist_ok=True)
for name,expected in manifest['files'].items():
    data=(root/'upstream'/Path(name).name).read_bytes()
    if hashlib.sha256(data).hexdigest()!=expected:
        raise SystemExit(f'Pinned source changed: {name}; review the recipe before updating hashes')
for name,expected in manifest['clap_headers']['sha256'].items():
    if hashlib.sha256((root/'include'/name).read_bytes()).hexdigest()!=expected:
        raise SystemExit(f'Pinned CLAP header changed: {name}')
for name in ('MidSide','MinMax','Exponent'):
    source=(root/'upstream'/f'{name}.cpp').read_text()
    # These exact pinned files put all UI and model registration after this marker.
    marker=f'struct {name}Widget :'
    if source.count(marker)!=1: raise SystemExit(f'Unexpected UI boundary: {name}')
    dsp=source.split(marker)[0].replace('#include "HetrickCV.hpp"','#include "rack-subset.hpp"')
    (out/f'{name}.hpp').write_text(dsp)
print('Verified and extracted 3 unchanged DSP implementations; UI/registration omitted.')
