#!/usr/bin/env python3
from pathlib import Path
import subprocess,sys,os,shutil
r=Path(__file__).resolve().parents[1];game=sys.argv[1];platform=sys.argv[2];win=platform=='windows';port=r/'ports'/game;common=port/'ios/Hangtime'
subprocess.run([sys.executable,str(r/'scripts/build_core.py'),game,platform],check=True)
cc='clang';out=r/'dist'/game/platform;out.mkdir(parents=True,exist_ok=True)
cmd=['-O2','-I'+str(common),'-I'+str(r/'vendor/mame2003-plus/src/libretro-common/include'),'-I'+str(r/'vendor/mame2003-plus/src/lib/zlib')]
if win:
 t=Path(os.environ.get('PORT_TOOLCHAIN',r/'vendor/llvm-mingw-20260616-ucrt-macos-universal'))
 s=Path(os.environ.get('PORT_SDL',r/'vendor/SDL2-2.32.10/x86_64-w64-mingw32'))
 cc=str(t/'bin/x86_64-w64-mingw32-clang');cmd+=['-I'+str(s/'include/SDL2'),'-L'+str(s/'lib')]
 icon=port/"desktop/AppIcon.rc"
 if icon.exists():
  resource=out/"AppIcon.res"
  subprocess.run([str(t/"bin/x86_64-w64-mingw32-windres"),"-I"+str(icon.parent),str(icon),"-O","coff","-o",str(resource)],check=True)
  cmd+=[str(resource)]
else:cmd+=subprocess.check_output(['sdl2-config','--cflags'],text=True).split()
cmd+=[str(port/'desktop/main.c'),str(port/'desktop/CRTSDL.c'),str(common/'HangtimeCore.c'),str(common/'ROMAudit.c'),str(r/'.build'/game/platform/'libcore.a')]
if game=='thps2':cmd+=[str(port/'desktop/CoreGraphics.c'),'-lc++','-liphlpapi']
if win:cmd+=['-mwindows','-lSDL2','-lopengl32','-lcomdlg32','-lws2_32','-lole32','-lshell32','-luuid','-lm','-o',str(out/(game+'.exe'))]
else:cmd+=subprocess.check_output(['sdl2-config','--libs'],text=True).split()+['-framework','OpenGL','-lz','-o',str(out/game)]
subprocess.run([cc]+cmd,check=True)
if win:shutil.copy2(s/'bin/SDL2.dll',out/'SDL2.dll')
print(out)
