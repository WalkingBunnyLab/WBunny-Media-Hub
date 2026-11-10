"""Run after make -f desktop.mk -f tools/tests/bilibili_live_ui.mk live-ui-preview.
Linux offscreen SDL preview; uses only fixture accounts and the test-only binary.
"""
import json
import os
from pathlib import Path
import subprocess

root = Path('build/live-preview').resolve()
root.mkdir(parents=True, exist_ok=True)
for scenario in ['rail', 'bookmarks', 'areas', 'signed', 'expired', 'network']:
    folder = root / scenario
    folder.mkdir(exist_ok=True)
    settings = dict(language='zh', startup_page='bilibili', ui_sounds=False,
                    greeting_style=0, idle_mug=False)
    if scenario == 'rail':
        settings['startup_page'] = 'bilibili_live'
    if scenario in ['signed', 'expired', 'network']:
        settings.update(bili_cookie='SESSDATA=fixture', bili_csrf='fixture',
                        bili_uid='123', bili_name='Fixture')
    config = folder / 'wbunny-media-hub.json'
    config.write_text(json.dumps({'settings': settings}), encoding='utf-8')
    commands = f'wait 60\nshot {folder}/initial.png\n'
    if scenario == 'rail':
        commands += (f'press B\nwait 30\nshot {folder}/rail-expanded.png\n'
                     f'tap 40 257\nwait 45\nshot {folder}/bilibili.png\n'
                     f'press B\nwait 30\ntap 40 311\nwait 45\n'
                     f'shot {folder}/live-returned.png\n')
    if scenario == 'bookmarks':
        commands += (f'tap 250 460\nwait 60\npress B\nwait 30\npress X\nwait 30\n'
                     f'tap 520 295\nwait 40\nshot {folder}/saved.png\n'
                     f'tap 250 460\nwait 60\npress B\nwait 30\npress X\nwait 40\n'
                     f'shot {folder}/removed.png\n')
    if scenario == 'areas':
        commands += (f'tap 350 295\nwait 40\nshot {folder}/parents.png\n'
                     f'tap 1000 545\nwait 30\nshot {folder}/next-parent-page.png\n'
                     f'tap 1000 275\nwait 30\n'
                     f'tap 1000 210\nwait 30\nshot {folder}/children.png\n'
                     f'tap 1000 210\nwait 40\nshot {folder}/filtered.png\n'
                     f'tap 340 225\nwait 40\nshot {folder}/refreshed.png\n')
    if scenario in ['signed', 'expired']:
        commands += f'tap 690 295\nwait 60\nshot {folder}/followed.png\n'
    script = folder / 'preview.script'
    script.write_text(commands + 'quit\n', encoding='utf-8')
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER='offscreen', SDL_RENDER_DRIVER='software',
               SDL_AUDIODRIVER='dummy',
               COFFEEFLIX_CJK_FONT=env.get('COFFEEFLIX_CJK_FONT', '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'),
               COFFEEFLIX_DATA=str(folder), COFFEEFLIX_SCRIPT=str(script),
               COFFEEFLIX_OPEN='bili-live', WBUNNY_LIVE_SCENARIO=scenario)
    if scenario == 'rail':
        env.pop('COFFEEFLIX_OPEN', None)
    log_path = folder / 'preview.log'
    with log_path.open('w') as log:
        result = subprocess.run(['./build-desktop/wbunny-live-preview'], env=env,
                                stdout=log, stderr=subprocess.STDOUT, timeout=120)
    assert result.returncode == 0, scenario
    log = log_path.read_text()
    saved = json.loads(config.read_text())
    if scenario == 'rail':
        assert log.count('QA live lobby') == 2 and 'QA popular' in log
        assert saved['settings']['last_section'] == 'bilibili_live'
    if scenario == 'bookmarks':
        assert log.count('QA offline room') == 2
        assert saved['favorites']['bilibili_live'] == []
    if scenario == 'areas':
        assert 'QA live areas' in log and log.count('QA live filtered') == 2
    if scenario == 'signed':
        assert 'QA followed live' in log and 'SESSDATA=fixture' in config.read_text()
    if scenario == 'expired':
        assert 'QA live expiry' in log and log.count('QA live lobby') == 2
        assert 'SESSDATA=fixture' not in config.read_text()
    if scenario == 'network':
        assert 'QA live network error' in log and 'SESSDATA=fixture' in config.read_text()
    print('PASS live UI:', scenario, flush=True)
