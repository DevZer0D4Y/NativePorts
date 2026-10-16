#!/usr/bin/env python3
import argparse,os,subprocess,shutil,hashlib,json,filecmp,sys,platform
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('platform');p.add_argument('--toolchain',type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(r/'scripts'));from fetch_cores import fetch
b=r/'.build'/a.game/a.platform;b.mkdir(parents=True,exist_ok=True)
win=a.platform=='windows';ios=a.platform.startswith('iphone');cc='clang';cxx='clang++';ar='ar'
if win:
 t=a.toolchain or Path(os.environ.get('PORT_TOOLCHAIN',r/'vendor/llvm-mingw-20260616-ucrt-macos-universal'))
 cc=str(t/'bin/x86_64-w64-mingw32-clang');cxx=str(t/'bin/x86_64-w64-mingw32-clang++');ar=str(t/'bin/llvm-ar')
if ios:
 sdk=subprocess.check_output(['xcrun','--sdk',a.platform,'--show-sdk-path'],text=True).strip();flag=' -target arm64-apple-ios16.0'+('-simulator' if a.platform=='iphonesimulator' else '')+' -isysroot '+sdk
 cc+=flag;cxx+=flag
lib=b/'libcore.a'
def sync(src):
 # Copy the reviewed vendor sources into this cached build. The cores include
 # .c files from other .c files and make does not see headers, so any change
 # rebuilds every object of the core.
 changed=False
 for source in src.rglob('*'):
  if not source.is_file() or source.suffix in ['.o','.a','.d'] or '.git' in source.parts: continue
  target=b/source.relative_to(src)
  if not target.exists() or not filecmp.cmp(source,target,shallow=False):
   target.parent.mkdir(parents=True,exist_ok=True)
   shutil.copy2(source,target); changed=True
 if changed:
  for obj in b.rglob('*.o'): obj.unlink()
  if lib.exists(): lib.unlink()
# Every frontend compiles against the libretro headers shipped with this core.
fetch('mame2003-plus')
if a.game in ['ssf2t','umk3']:
 sync(r/'vendor/mame2003-plus')
 drv='drv_'+a.game
 cmd=['make','-j2','platform='+('win' if win else 'osx'),'INCLUDE_DRV='+drv,'STATIC_LINKING=1','TARGET='+str(lib),'CC='+cc,'AR='+ar,'PLATCFLAGS=-D'+drv+' -DSPLIT_CORE -Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-error=incompatible-pointer-types']
 if win:cmd+=['system_platform=win','SPLIT_UP_LINK=0']
elif a.game in ['slamjam96','nbashowt']:
 src=fetch('pcsx')
 sync(src)
 cmd=['make','-f','Makefile.libretro','-j2','platform='+('win' if win else 'ios-arm64' if ios else 'osx'),'STATIC_LINKING=1','TARGET='+str(lib),'CC='+cc,'CXX='+cxx,'AR='+ar,'DYNAREC=0','HAVE_PHYSICAL_CDROM=0','USE_ASYNC_CDROM=0','USE_ASYNC_GPU=0','USE_ASYNC_SPU=0','NDRC_THREAD=0']
elif a.game=='thps2':
 cxx+=' -std=c++11'
 fetch('flycast')
 if not (b/'Makefile').exists():shutil.copytree(r/'vendor/flycast',b,dirs_exist_ok=True)
 # Keep the cached build's engine sources in sync with reviewed vendor fixes.
 # Preserve unchanged timestamps so incremental builds stay incremental.
 for source in (r/'vendor/flycast').rglob('*'):
  if not source.is_file() or source.suffix in ['.o', '.a']: continue
  target = b/source.relative_to(r/'vendor/flycast')
  if not target.exists() or not filecmp.cmp(source,target,shallow=False):
   target.parent.mkdir(parents=True,exist_ok=True)
   shutil.copy2(source,target)
 cmd=['make','-j2','platform='+('win' if win else 'osx'),'STATIC_LINKING=1','TARGET='+str(lib),'CC='+cc,'CXX='+cxx,'AR='+ar,'WITH_DYNAREC='+('x86_64' if win else '0'),'NO_REC='+('0' if win else '1'),'HAVE_GENERIC_JIT=0','HAVE_GL2=1','HAVE_GL3=0','HAVE_VULKAN=0','HAVE_OPENMP=0','HAVE_TEXUPSCALE=0','CPUFLAGS=','MFLAGS=','SINGLE_PREC_FLAGS=0']
 if win:cmd+=['HOST_CPU_FLAGS=-DHOST_CPU=CPU_X64']
 if not win and not ios and platform.machine() == 'arm64':cmd+=['HOST_CPU_FLAGS=-DHOST_CPU=CPU_ARM64 -DTARGET_NO_JIT -DTARGET_NO_EXCEPTIONS']
 if ios:cmd+=['FORCE_GLES=1','HOST_CPU_FLAGS=-DIOS -DHOST_CPU=CPU_ARM64 -DTARGET_NO_JIT -DTARGET_NO_EXCEPTIONS']
else:raise SystemExit('Engine integration in progress for '+a.game)
stamp = b / 'build-configuration.sha256'
configuration = None
if a.game in ['ssf2t', 'umk3', 'thps2']:
 # Make does not track changes to CPU-selection macros. Objects compiled with
 # an older selection can link successfully but contain an incompatible CPU
 # table. Invalidate every object when the configuration changes.
 digest = hashlib.sha256(json.dumps(cmd).encode())
 names = ['Makefile', 'Makefile.split', 'Makefile.common'] if a.game != 'thps2' else ['Makefile', 'Makefile.common']
 for name in names:
  digest.update((b / name).read_bytes())
 if a.game == 'thps2':
  # This engine's Makefile does not track header dependencies. Invalidate
  # objects when a shared header changes, including fatal-error diagnostics.
  for header in sorted((r/'vendor/flycast/core').rglob('*.h')):
   digest.update(str(header.relative_to(r/'vendor/flycast')).encode())
   digest.update(header.read_bytes())
 configuration = digest.hexdigest()
 if not stamp.exists() or stamp.read_text().strip() != configuration:
  for obj in b.rglob('*.o'): obj.unlink()
  if lib.exists(): lib.unlink()
with (b/'core.log').open('w') as f:res=subprocess.run(cmd,cwd=b,stdout=f,stderr=subprocess.STDOUT)
if res.returncode:print((b/'core.log').read_text()[-10000:]);raise SystemExit(res.returncode)
if configuration: stamp.write_text(configuration+'\n')
print(lib)
