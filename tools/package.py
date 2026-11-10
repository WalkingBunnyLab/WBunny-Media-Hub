#!/usr/bin/env python3
"""Package a clean, committed local version; never build, deploy or publish."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(
        ['git', '-c', 'safe.directory=' + ROOT.as_posix(), *args], cwd=ROOT
    ).decode().strip()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wuhb', type=Path, required=True)
    parser.add_argument('--record', type=Path, help='JSON build record with source_commit and wuhb_sha256')
    args = parser.parse_args()
    version = (ROOT / 'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?', version):
        raise SystemExit('Invalid VERSION')
    if git('status', '--porcelain'):
        raise SystemExit('Commit the source changes before packaging')
    commit = git('rev-parse', 'HEAD')
    app = args.wuhb.resolve()
    if not app.is_file() or app.stat().st_size < 1024:
        raise SystemExit('A built WUHB is required')
    app_hash = digest(app)
    evidence = {}
    if args.record:
        evidence = json.loads(args.record.read_text(encoding='utf-8'))
        if evidence.get('source_commit') != commit or evidence.get('wuhb_sha256') != app_hash:
            raise SystemExit('Build record does not match the committed source and exact WUHB')
    else:
        raise SystemExit('--record is required to bind this package to its build source')

    out = ROOT / 'artifacts' / version
    out.mkdir(parents=True, exist_ok=True)
    archive = out / f'wbunny-media-hub-{version}.zip'
    if archive.exists():
        raise SystemExit('Package already exists; preserve the recorded version or increment VERSION')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as bundle:
        prefix = 'wiiu/apps/wbunny-media-hub/'
        bundle.write(app, prefix + 'wbunny-media-hub.wuhb')
        bundle.write(ROOT / 'LICENSE.md', prefix + 'LICENSE.md')
        bundle.write(ROOT / 'content/licenses/NOTICES.txt', prefix + 'NOTICES.txt')
        bundle.write(ROOT / 'README.md', 'README.md')
    with zipfile.ZipFile(archive) as bundle:
        if bundle.testzip() is not None:
            raise SystemExit('ZIP verification failed')
        if hashlib.sha256(bundle.read(prefix + 'wbunny-media-hub.wuhb')).hexdigest() != app_hash:
            raise SystemExit('Packaged app differs from the built app')

    source = out / f'wbunny-media-hub-{version}-source.zip'
    subprocess.run(['git', '-c', 'safe.directory=' + ROOT.as_posix(), 'archive', '--format=zip',
                    '--output=' + str(source), commit], cwd=ROOT, check=True)
    manifest = out / 'manifest.json'
    manifest.write_text(json.dumps({
        'application': 'WBunny Media Hub', 'version': version, 'source_commit': commit,
        'install_directory': 'wiiu/apps/wbunny-media-hub',
        'wuhb_sha256': app_hash, 'package_sha256': digest(archive),
        'source_archive_sha256': digest(source), 'build': evidence,
        'hardware_test': 'Not yet tested under the new application identity'
    }, indent=2) + '\n', encoding='utf-8')
    checksums = out / 'SHA256SUMS.txt'
    checksums.write_text(''.join(f'{digest(p)}  {p.name}\n' for p in [app, archive, source, manifest]), encoding='ascii')
    print(archive)
    print('Verified package, source commit and SHA-256 checksums')


if __name__ == '__main__':
    main()
