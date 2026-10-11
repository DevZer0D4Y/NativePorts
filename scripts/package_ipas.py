#!/usr/bin/env python3
"""Package the Release-iphoneos apps built by build_ios.py as unsigned IPAs in dist/ios."""
from pathlib import Path
import zipfile,plistlib,stat,hashlib,json,re,sys
root=Path(__file__).resolve().parents[1];out=root/'dist/ios';out.mkdir(parents=True,exist_ok=True);rows=[]
games=sys.argv[1:] or ['ssf2t','umk3','slamjam96','thps2']
# Game files and signing material never go into a package.
BLOCKED=re.compile(r'\.(zip|chd|cue|bin|iso|gdi|cdi|7z|mobileprovision|p12|cer)$',re.I)
PRIVATE=re.compile(rb'/Users/(?!runner/)[^/\0]+/|/home/[^/\0]+/|[A-Z]:\\Users\\')
for game in games:
 apps=list((root/'.build'/game/'app/Build/Products/Release-iphoneos').glob('*.app'));assert len(apps)==1,game+': build it first with scripts/build_ios.py'
 app=apps[0];info=plistlib.loads((app/'Info.plist').read_bytes())
 assert info['UIFileSharingEnabled'] and info['LSSupportsOpeningDocumentsInPlace']
 assert (app/info['CFBundleExecutable']).is_file()
 target=out/(app.stem+'-'+info['CFBundleShortVersionString']+'-unsigned.ipa')
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for p in sorted(app.rglob('*')):
   if p.is_file():
    rel=p.relative_to(app);path='Payload/'+app.name+'/'+rel.as_posix()
    if '_CodeSignature' in rel.parts or rel.name=='embedded.mobileprovision':continue
    assert not BLOCKED.search(rel.name),'refusing to package '+str(rel)
    data=p.read_bytes();m=PRIVATE.search(data)
    assert not m,'%s contains a private build path: %r'%(rel,m.group())
    item=zipfile.ZipInfo(path);item.compress_type=zipfile.ZIP_DEFLATED;item.external_attr=(stat.S_IFREG|(0o755 if p.name==info['CFBundleExecutable'] else 0o644))<<16
    z.writestr(item,data)
 with zipfile.ZipFile(target) as z:assert z.testzip() is None
 rows.append({'game':game,'ipa':target.name,'bundle_id':info['CFBundleIdentifier'],'unsigned':True,'size':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
 print(target.name,target.stat().st_size,'bytes')
(out/'manifest.json').write_text(json.dumps(rows,indent=2)+'\n')
