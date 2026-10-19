#!/usr/bin/env python3
"""Flatten MAME's static dependencies and frontend objects into a libretro archive."""
import os,subprocess,sys
from pathlib import Path
mode,ar,target,*inputs=sys.argv[1:]
target=Path(target).resolve(); target.parent.mkdir(parents=True,exist_ok=True)
tmp=target.with_suffix('.merging.a')
if tmp.exists():tmp.unlink()
if mode=='apple':
 subprocess.run(['xcrun','libtool','-static','-o',str(tmp),*inputs],check=True)
else:
 # MRI ADDLIB merges members rather than nesting archives. Absolute paths keep
 # this independent of the generator's working directory.
 lines=['CREATE '+str(tmp)]
 for name in inputs:
  path=str(Path(name).resolve())
  lines.append(('ADDLIB ' if name.endswith('.a') else 'ADDMOD ')+path)
 lines+=['SAVE','END']
 subprocess.run([ar,'-M'],input='\n'.join(lines)+'\n',text=True,check=True)
 subprocess.run([ar,'s',str(tmp)],check=True)
os.replace(tmp,target)
