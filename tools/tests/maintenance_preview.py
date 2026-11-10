"""Offscreen account/settings/branding and real HTTP EOF regression checks."""
from pathlib import Path
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json, os, subprocess, threading

root = Path('build/maintenance-preview').resolve()
root.mkdir(parents=True, exist_ok=True)
base = os.environ.copy()
base.update(SDL_VIDEODRIVER='offscreen', SDL_RENDER_DRIVER='software', SDL_AUDIODRIVER='dummy',
            COFFEEFLIX_CJK_FONT='/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc')

def run(name, commands, opening='settings:4', signed=False, **extra):
    folder = root / name
    folder.mkdir(exist_ok=True)
    settings = dict(language='zh', startup_page='bilibili', ui_sounds=False, greeting_style=0, idle_mug=False)
    if signed:
        settings.update(bili_cookie='SESSDATA=fixture', bili_csrf='fixture', bili_uid='123', bili_name='Fixture')
    (folder/'wbunny-media-hub.json').write_text(json.dumps({'settings':settings}))
    script = folder/'preview.script'
    script.write_text(commands.replace('@', str(folder))+'quit\n')
    env = base.copy()
    env.update(COFFEEFLIX_DATA=str(folder), COFFEEFLIX_SCRIPT=str(script), COFFEEFLIX_OPEN=opening, **extra)
    with (folder/'preview.log').open('w') as out:
        subprocess.run(['./build-desktop/wbunny-live-preview'], env=env, stdout=out, stderr=subprocess.STDOUT, check=True, timeout=100)
    return (folder/'preview.log').read_text()

for signed in [False, True]:
    log=run('account-'+str(signed), 'wait 45\nshot @/settings.png\ntap 500 315\nwait 45\nshot @/account.png\n', signed=signed)
    if not signed: assert 'QA account QR requested' in log, log[-2000:]
run('languages', 'wait 45\ntap 500 158\nwait 30\nshot @/languages.png\n', opening='settings:1')
run('intro', 'wait 24\nshot @/intro.png\n', opening='settings:4', COFFEEFLIX_INTRO='1')
run('saver', 'wait 360\nshot @/saver.png\n', COFFEEFLIX_SAVER='1')

# A finite HTTP response represents a CDN prematurely ending a live stream.
subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','lavfi','-i','testsrc2=size=320x180:rate=24',
                '-t','3','-an','-c:v','libx264','-pix_fmt','yuv420p','-movflags','+faststart',str(root/'short.mp4')],check=True)
class Handler(SimpleHTTPRequestHandler):
    def log_message(self, *args): pass
server=ThreadingHTTPServer(('127.0.0.1',0),partial(Handler,directory=str(root)))
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    url='bili-file:http://127.0.0.1:'+str(server.server_port)+'/short.mp4'
    log=run('live-eof','wait 1500\nshot @/result.png\n',opening=url,COFFEEFLIX_LIVE='1')
    assert 'Live stream reached EOF' in log and 'Live stream lost: starting it again' in log, log[-4000:]
    assert 'Finished' not in log, log[-4000:]
    log=run('vod-eof','wait 1500\nshot @/result.png\n',opening=url)
    assert 'Finished' in log and 'Live stream lost' not in log, log[-4000:]
finally:
    server.shutdown()
print('PASS native branding/settings previews, real HTTP live EOF restarts, normal VOD completion')
