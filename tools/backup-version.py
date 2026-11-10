#!/usr/bin/env python3
"""Create and verify a local Git bundle without adding a remote or pushing."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(['git', '-c', 'safe.directory=' + ROOT.as_posix(), *args], cwd=ROOT).decode().strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tag', required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._/-]*', args.tag):
        raise SystemExit('Invalid tag')
    if git('status', '--porcelain'):
        raise SystemExit('Commit the source changes before backing up a version')
    commit = git('rev-parse', '--verify', 'refs/tags/' + args.tag + '^{commit}')
    if commit != git('rev-parse', 'HEAD'):
        raise SystemExit('The version tag must point to the current clean HEAD')
    out = ROOT / 'backups'
    out.mkdir(exist_ok=True)
    stem = 'wbunny-media-hub-' + args.tag.replace('/', '-') + '-' + commit[:12]
    bundle = out / (stem + '.bundle')
    if bundle.exists():
        raise SystemExit('Backup already exists; refusing to overwrite it')
    git('bundle', 'create', str(bundle), '--all')
    git('bundle', 'verify', str(bundle))
    sha = hashlib.sha256(bundle.read_bytes()).hexdigest()
    (out / (stem + '.sha256')).write_text(sha + '  ' + bundle.name + '\n', encoding='ascii')
    (out / (stem + '.json')).write_text(json.dumps({
        'tag': args.tag, 'commit': commit, 'bundle_sha256': sha,
        'refs': git('show-ref').splitlines(),
        'scope': 'Local Git history and tags; build outputs and device settings are not in this bundle'
    }, indent=2) + '\n', encoding='utf-8')
    print(bundle)


if __name__ == '__main__':
    main()
