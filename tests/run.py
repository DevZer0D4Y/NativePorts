from pathlib import Path
import subprocess,json
r=Path(__file__).resolve().parents[1]
for game in ['ssf2t','umk3','slamjam96','nbashowt','thps2']:
 port=r/'ports'/game;g=json.loads((port/'game.json').read_text());common=r/'ports/ssf2t/ios/Hangtime';out=r/'.build'/('test-input-'+game)
 cmd=['clang','-O1','-DPORT_MAIN="'+str(port/'desktop/main.c')+'"','-DPORT_PLAYERS='+str(g['players']),'-DPORT_PLAYER_ORDER=((unsigned[]){'+','.join(str(n-1) for n in g.get('player_order',range(1,g['players']+1)))+'})','-DPORT_TIMED_COMBOS='+str(int(game=='nbashowt')),'-I'+str(common),'-I'+str(r/'vendor/mame2003-plus/src/libretro-common/include'),'-I'+str(r/'vendor/mame2003-plus/src/lib/zlib')]
 cmd+=subprocess.check_output(['sdl2-config','--cflags'],text=True).split()
 if game=='thps2':cmd+=['-DPORT_THPS2=1']
 cmd+=[str(r/'tests/test_controllers.c'),str(port/'desktop/CRTSDL.c'),str(common/'HangtimeCore.c'),str(common/'ROMAudit.c'),str(r/'.build/ssf2t/macos/libcore.a')]
 cmd+=subprocess.check_output(['sdl2-config','--libs'],text=True).split()+['-framework','OpenGL','-lz','-o',str(out)]
 subprocess.run(cmd,check=True);print(game,flush=True);subprocess.run([str(out)],check=True)
