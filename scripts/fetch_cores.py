#!/usr/bin/env python3
"""Download the pinned emulator cores into vendor/ and apply the port patches."""
import argparse, io, shutil, subprocess, tarfile, urllib.request
from pathlib import Path

r = Path(__file__).resolve().parents[1]
CORES = {
 'mame-current': ('libretro/mame', '5f5a07748ce5c239c4118463ce8e6473743d4859', 'mame-current.patch'),
 'mame2003-plus': ('libretro/mame2003-plus-libretro', 'e05caba0aa33cae580d104c45c6caa6406617e3b', 'mame2003-plus.patch'),
 'flycast': ('libretro/flycast', '497de2593bc8bef1a31a3105a0a57f6a059b6ca7', 'flycast.patch'),
 'pcsx': ('libretro/pcsx_rearmed', 'c8816799b50388e61cfe237fe2cdbb7d8175f20a', 'pcsx.patch'),
}

def fetch(name, force=False):
 repo, commit, patch = CORES[name]
 dest = r/'vendor'/name
 stamp = dest/'.port-revision'
 if stamp.exists() and stamp.read_text().strip() == commit and not force:
  return dest
 if dest.exists(): shutil.rmtree(dest)
 url = 'https://codeload.github.com/%s/tar.gz/%s' % (repo, commit)
 print('Downloading', repo, commit[:12], flush=True)
 data = urllib.request.urlopen(url).read()
 tmp = r/'vendor'/('.'+name+'-download')
 if tmp.exists(): shutil.rmtree(tmp)
 tmp.mkdir(parents=True)
 with tarfile.open(fileobj=io.BytesIO(data)) as t:
  t.extractall(tmp, filter='data') if hasattr(tarfile, 'data_filter') else t.extractall(tmp)
 (top,) = list(tmp.iterdir())
 top.rename(dest); tmp.rmdir()
 if patch:
  subprocess.run(['patch', '-p1', '-s', '-i', str(r/'patches'/patch)], cwd=dest, check=True)
 stamp.write_text(commit+'\n')
 return dest

if __name__ == '__main__':
 p = argparse.ArgumentParser()
 p.add_argument('cores', nargs='*', default=list(CORES), choices=list(CORES))
 p.add_argument('--force', action='store_true', help='download again and reapply the patch')
 a = p.parse_args()
 for name in a.cores: print(fetch(name, a.force))
