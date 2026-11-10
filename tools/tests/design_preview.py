"""Native visual/interaction previews; run from repo root after desktop build.
Requires build/design-preview/fixture.mp4 (45s H.264/AAC test pattern).
Artifacts are for visual review, not a substitute for Wii U hardware testing.
"""
from pathlib import Path
import json, os, subprocess

root=Path('build/design-preview').resolve()
assert (root/'fixture.mp4').is_file()
for scenario in ['player','remote','account']:
    p=root/scenario;p.mkdir(exist_ok=True)
    (p/'wbunny-media-hub.json').write_text(json.dumps({'settings':dict(language='zh',startup_page='bilibili',ui_sounds=False,greeting_style=0,idle_mug=False)}))
    script=f'wait 100\nshot {p}/initial.png\n'
    if scenario=='player':
        script+=(f'tap 640 339\nwait 20\nshot {p}/paused.png\n'
                 f'drag 100 592 470 592 30\nwait 30\nshot {p}/seeked.png\n'
                 f'tap 1216 610\nwait 30\nshot {p}/speed-menu.png\n'
                 f'press B\nwait 20\ntap 1156 610\nwait 30\nshot {p}/sleep-menu.png\n'
                 f'press B\nwait 20\ntap 640 339\nwait 45\nshot {p}/resumed.png\n')
    elif scenario=='remote':
        script+=(f'tap 640 407\nwait 20\nshot {p}/paused.png\n'
                 f'drag 100 608 500 608 30\nwait 30\nshot {p}/seeked.png\n'
                 f'tap 400 56\nwait 30\nshot {p}/speed-menu.png\n'
                 f'press B\nwait 20\n')
    (p/'preview.script').write_text(script+'quit\n')
    env=os.environ.copy();env.update(SDL_VIDEODRIVER='offscreen',SDL_RENDER_DRIVER='software',SDL_AUDIODRIVER='dummy',
        COFFEEFLIX_CJK_FONT='/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc',COFFEEFLIX_DATA=str(p),
        COFFEEFLIX_SCRIPT=str(p/'preview.script'),COFFEEFLIX_OPEN='bili-file:'+str(root/'fixture.mp4'))
    if scenario=='remote':env.update(COFFEEFLIX_PAD_LAB='1',COFFEEFLIX_PAD_FULL='1')
    if scenario=='account':env.update(COFFEEFLIX_OPEN='bili-account',WBUNNY_LIVE_SCENARIO='account')
    binary='./build-desktop/wbunny-live-preview' if scenario=='account' else './build-desktop/wbunny-media-hub'
    with (p/'preview.log').open('w') as out:result=subprocess.run([binary],env=env,stdout=out,stderr=subprocess.STDOUT,timeout=120)
    assert result.returncode==0,scenario
    if scenario!='account':assert 'Video decoder: h264' in (p/'preview.log').read_text()
    print('PASS native design preview:',scenario,flush=True)
