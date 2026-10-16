#!/usr/bin/env python3
"""Package Windows executables and runtime DLLs, excluding build/game files."""
from pathlib import Path
import hashlib,json,plistlib,struct,zipfile
root=Path(__file__).resolve().parents[1]
output=root/'dist/NativePorts-Windows-x64.zip'
games=['ssf2t','umk3','slamjam96','thps2','nbashowt'];manifest=[]
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
 for game in games:
  info=plistlib.loads((root/'ports'/game/'ios/Hangtime/Info.plist').read_bytes())
  names=[game+'.exe','SDL2.dll']+(['libc++.dll','libunwind.dll'] if game=='thps2' else [])
  assets=[]
  for name in names:
   p=root/'dist'/game/'windows'/name;data=p.read_bytes()
   pe=struct.unpack_from('<I',data,0x3c)[0]
   assert data[:2]==b'MZ' and data[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',data,pe+4)[0]==0x8664,name
   archive.writestr(game+'/'+name,data)
   assets.append(dict(file=name,sha256=hashlib.sha256(data).hexdigest()))
  manifest.append(dict(game=game,version=info['CFBundleShortVersionString'],files=assets))
 archive.writestr('versions.json',json.dumps(manifest,indent=2)+'\n')
 archive.write(root/'README.md','README.md')
 archive.write(root/'LICENSE','LICENSE')
 for p in sorted((root/'licenses').iterdir()):
  if p.is_file():archive.write(p,'licenses/'+p.name)
with zipfile.ZipFile(output) as archive:
 assert archive.testzip() is None
print(output,output.stat().st_size,'bytes')
