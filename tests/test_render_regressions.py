"""Run the real HUD, camera instruction patch and sky-row helpers on regressions."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
work = root / '.build/render-regressions'
work.mkdir(parents=True, exist_ok=True)
def function(path, marker):
    source=(root/path).read_text();start=source.index(marker);brace=source.index('{',start)
    level=1;end=brace+1
    while level:
        level += (source[end]=='{') - (source[end]=='}');end+=1
    return source[start:end]
def run(name, code):
    p=work/(name+'.cpp');p.write_text(code)
    subprocess.run(['clang++','-std=c++11',str(p),'-o',str(work/name)],check=True)
    subprocess.run([str(work/name)],check=True)
common='#include <cassert>\n#include <cstdint>\n#include <cstring>\n#include <cstdio>\n#include <vector>\nusing u8=uint8_t;using u16=uint16_t;using u32=uint32_t;\n'
hud=function('vendor/flycast/core/rend/gles/gldraw.cpp','static bool thps2_hud(')
run('hud',common+r'''
struct Vertex {float x,y,z;};
template<class T> struct Store {T *p;T*head(){return p;}};
Vertex vertices[4];u32 indices[]={0,1,2,3};
struct {bool isRTT;Store<Vertex> verts;Store<u32>idx;}pvrrc={false,{vertices},{indices}};
struct {struct {bool WidescreenGameHacks;}rend;}settings={{true}};
struct {char product_number[16];}ip_meta={"T13006N"};
struct PolyParam {struct {unsigned DepthMode;}isp;u32 count,first;};
'''+hud+r'''
int main(){
 PolyParam p={{4},4,0};
 // Captured score glyph: the old ALWAYS-only classifier rejected it.
 vertices[0]={32,52,7.02f};vertices[1]={72,52,7.02f};
 vertices[2]={32,68,7.02f};vertices[3]={72,68,7.02f};
 assert(thps2_hud(&p));
 p.isp.DepthMode=7;assert(thps2_hud(&p));p.isp.DepthMode=4;
 for(auto &v:vertices)v.z=100;assert(!thps2_hud(&p)); // world billboard
 for(auto &v:vertices)v.z=.002f;assert(!thps2_hud(&p)); // court/world plane
 for(auto &v:vertices)v.z=7.02f;vertices[3].z=7.03f;assert(!thps2_hud(&p));
 vertices[3].z=7.02f;pvrrc.isRTT=true;assert(!thps2_hud(&p));pvrrc.isRTT=false;
 settings.rend.WidescreenGameHacks=false;assert(!thps2_hud(&p));
 puts("PASS: captured GREATER-depth HUD centered; world geometry, RTT and 4:3 excluded.");
}
''')
subprocess.run(['python3', str(root/'tests/test_thps2_camera.py')], check=True)
sky=function('vendor/mame2003-plus/src/vidhrdw/midtunit_vidhrdw.c','void midtunit_from_shiftreg(')
run('sky',common+r'''
using UINT32=u32;using UINT16=u16;
u16 storage[2048];u16*local_videoram=storage;bool wide;
int umk3_widescreen_active(){return wide;}
#define drv_umk3
'''+sky+r'''
int main(){
 u16 rows[1024]={};for(unsigned y=0;y<2;y++)for(unsigned x=56;x<456;x++)rows[y*512+x]=0x200+y;
 u16 saved[1024];memcpy(saved,rows,sizeof(rows));
 wide=false;midtunit_from_shiftreg(0,rows);assert(!memcmp(rows,storage,sizeof(rows)));
 wide=true;midtunit_from_shiftreg(0,rows);
 for(unsigned y=0;y<2;y++)for(unsigned x=0;x<512;x++)assert(storage[y*512+x]==0x200+y);
 assert(!memcmp(rows,saved,sizeof(rows))); // preserve hardware shift register
 rows[100]=0x321;midtunit_from_shiftreg(0,rows);
 assert(!memcmp(rows,storage,512*sizeof(u16))); // never extend a textured row
 puts("PASS: solid sky/gradient clears cover 512 columns; textures, native pixels and shift register preserved.");
}
''')
