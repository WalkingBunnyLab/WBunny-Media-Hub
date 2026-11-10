#!/usr/bin/env python3
"""Generate ignored live client protocol parameters; never print parameter values."""
import argparse, json, re
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--reference', type=Path, help='wiliwili include/api/bilibili/util/http.hpp')
p.add_argument('--config', type=Path, default=ROOT / '.local/bilibili-live-client.json')
a = p.parse_args()
if a.reference:
    source = a.reference.read_text(encoding='utf-8')
    data = {}
    for name, macro in [('key','BILIBILI_APP_KEY'),('secret','BILIBILI_APP_SECRET'),('build','BILIBILI_BUILD')]:
        m = re.search(macro + r'\s*=\s*"([A-Za-z0-9]+)"', source)
        if not m: raise SystemExit('Missing public client protocol parameter: ' + macro)
        data[name] = m.group(1)
    a.config.parent.mkdir(parents=True, exist_ok=True)
    a.config.write_text(json.dumps(data)+'\n',encoding='utf-8')
if not a.config.is_file():
    raise SystemExit('Configure Bilibili live first: tools/configure-bilibili-live.py --reference <wiliwili/include/api/bilibili/util/http.hpp>')
data = json.loads(a.config.read_text(encoding='utf-8'))
if any(not re.fullmatch(r'[A-Za-z0-9]{1,128}', data.get(k,'')) for k in ['key','secret','build']):
    raise SystemExit('Invalid live client configuration')
header = '#pragma once\nnamespace bilibili_live_config {\n' + ''.join('inline constexpr const char* '+k+' = '+json.dumps(data[k])+';\n' for k in ['key','secret','build']) + '}\n'
for folder in ['build','build-desktop']:
    dest = ROOT / folder / 'bilibili_live_client.hpp'
    dest.parent.mkdir(parents=True, exist_ok=True)
    if not dest.exists() or dest.read_text(encoding='utf-8') != header: dest.write_text(header, encoding='utf-8')
print('Configured ignored live client headers for Wii U and desktop builds (values omitted).')
