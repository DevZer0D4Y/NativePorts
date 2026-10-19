#!/usr/bin/env python3
"""Build the pinned Vegas arcade driver without embedding any game data."""
import argparse,hashlib,json,os,re,shutil,subprocess,sys
from pathlib import Path
from fetch_cores import fetch
p=argparse.ArgumentParser();p.add_argument('platform');p.add_argument('--toolchain',type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[1];src=fetch('mame-current');win=a.platform=='windows'
if a.platform not in ['iphoneos','windows']:raise SystemExit('Showtime arcade supports device iOS and Windows builds.')
build='build-windows' if win else 'build-ios';dest=r/'.build/nbashowt'/a.platform;dest.mkdir(parents=True,exist_ok=True)
# Cache identity includes the build recipe, patch, and the actual patched
# sources. App-only rebuilds stay fast, but an old core cannot mask fixes.
cached=dest/'libcore.a';stamp=dest/'core-fingerprint.json'
patch=r/'patches/mame-current.patch'
inputs=[Path(__file__),r/'scripts/merge_arcade_archive.py',patch,src/'.port-revision']
inputs += [src/'src/devices/cpu/mips/mips3.cpp',src/'src/emu/divtlb.h',src/'src/devices/cpu/mips/showtimecopy.h']
inputs += [src/m for m in re.findall(r'^\+\+\+ b/(.+)$',patch.read_text(),re.M)]
identity={'platform':a.platform,'toolchain':str(a.toolchain or os.environ.get('PORT_TOOLCHAIN','default')),
          'inputs':{str(f.relative_to(r)):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs if f.exists()}}
if cached.exists() and cached.stat().st_size>1024*1024 and stamp.exists() and not os.environ.get('REBUILD_SHOWTIME_CORE'):
 if json.loads(stamp.read_text())==identity:
  print(cached);raise SystemExit(0)
base=['make','-f','Makefile.libretro','-j2','platform='+('win' if win else 'ios-arm64'),'TARGET=mame','SUBTARGET=nbashowt','PROJECT=','SOURCES=src/mame/williams/vegas.cpp','BUILDDIR='+build]
ar='ar'
if win:
 t=a.toolchain or Path(os.environ.get('PORT_TOOLCHAIN',r/'vendor/llvm-mingw-20260616-ucrt-macos-universal'))
 ar=str(t/'bin/llvm-ar')
 base+=['CC='+str(t/'bin/x86_64-w64-mingw32-clang'),'CXX='+str(t/'bin/x86_64-w64-mingw32-clang++'),'AR='+ar,'WINDRES='+str(t/'bin/x86_64-w64-mingw32-windres'),'MINGW64='+str(t),'LIBRETRO_CPU=x86_64','NO_OPENGL=1','NOASM=1','NATIVE_DRC=drcbe_c']
# The generator omits all objects when libretro is made static. Generate,
# then replace only that final archive recipe with a dependency-flattening step.
project=src/build/'projects/retro/mamenbashowt'/('gmake-mingw-clang' if win else 'gmake-osx-clang')
with (dest/'core.log').open('w') as log:
 result=subprocess.run(base+['port-generate','PORT_PROJECT='+str((project/'Makefile').relative_to(src))],cwd=src,stdout=log,stderr=subprocess.STDOUT)
 if result.returncode or not project.exists():raise SystemExit('MAME project generation failed: '+str(dest/'core.log'))
 makefile=project/'nbashowt.make'
 if not makefile.exists():
  matches=list((src/build/'projects').rglob('nbashowt.make'))
  if len(matches)!=1:raise SystemExit('Cannot identify generated MAME project')
  makefile=matches[0]
 text=makefile.read_text()
 command=f'{sys.executable} {r}/scripts/merge_arcade_archive.py '+('llvm' if win else 'apple')+f' {ar} $(TARGET) $(OBJECTS) $(LIBDEPS)'
 text=re.sub(r'(^\s*LINKCMD\s*=).*$',lambda m:m[1]+' '+command if '$(AR)' in m[0] or 'merge_arcade_archive' in m[0] else m[0],text,flags=re.M)
 makefile.write_text(text)
 generated=['make','-j2','-R','-C',str(makefile.parent),'config=libretro64','AR='+ar]
 if win:generated+=['WINDRES='+str(t/'bin/x86_64-w64-mingw32-windres')]
 subprocess.run(generated+['precompile'],cwd=src,stdout=log,stderr=subprocess.STDOUT,check=True)
 result=subprocess.run(generated,cwd=src,stdout=log,stderr=subprocess.STDOUT)
 if result.returncode:raise SystemExit('MAME build failed: '+str(dest/'core.log'))
 target=src/('nbashowt_libretro.a' if win else 'nbashowt_libretro_ios.a')
 if not target.exists() or target.stat().st_size<1024*1024:raise SystemExit('MAME archive missing or empty: '+str(target))
 if cached.is_symlink():cached.unlink()
 shutil.copy2(target,cached)
 stamp.write_text(json.dumps(identity,sort_keys=True,indent=2)+'\n')
print(dest/'libcore.a')
