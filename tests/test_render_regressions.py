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
camera=function('vendor/flycast/core/reios/reios.cpp','static bool thps2_follow_camera(')
run('camera',common+camera+r'''
int main(){
 std::vector<u8>ram(0x75000);u32 base=0x70700;u16*f=(u16*)(ram.data()+base);
 const unsigned offsets[]={0x8e,0x90,0xb0,0xb4,0xb6,0xb8,0xba,0xbc,0xc0,0xc2,0xc4,0xc6,0xca,0xcc,0xce,0xd0,0xd2};
 const u16 words[]={0xa04b,9,0x51c1,0x5237,0xe7f4,0x312c,0x417c,0x1511,0x5238,0x312c,0x417c,0x1512,0x5239,0x312c,0x966b,0x417c,0x1513};
 for(unsigned i=0;i<sizeof(words)/2;i++)f[offsets[i]/2]=words[i];
 for(unsigned i=0x92;i<=0x9e;i+=2)f[i/2]=9;
 f[0x1a8/2]=0x68c; // original PC-relative literal
 auto original=ram;
 f[0x98/2]=0;assert(!thps2_follow_camera(ram.data())); // occupied cave: refuse
 ram=original;f=(u16*)(ram.data()+base);assert(thps2_follow_camera(ram.data()));
 for(unsigned site:{0xb8u,0xc2u,0xccu})for(int offset:{-1000000,-1,0,1,1000000}){
  u32 regs[16]={};regs[1]=1200000;regs[2]=offset;regs[7]=(u32)-12;
  unsigned pc=site,pr=0,steps=0;
  while(pc!=site+4){
   assert(++steps<20);u16 op=f[pc/2];
   if((op&0xf000)==0xb000){int d=(int16_t)(op<<4)>>4;assert(f[(pc+2)/2]==9);pr=pc+4;pc=pc+4+d*2;}
   else if(op==0x6023){regs[0]=regs[2];pc+=2;}
   else if(op==0x4021){regs[0]=(int32_t)regs[0]>>1;pc+=2;}
   else if(op==0x320c){regs[2]+=regs[0];pc+=2;}
   else if(op==0x312c){regs[1]+=regs[2];pc+=2;}
   else if(op==0xb){assert(f[(pc+2)/2]==0x417c);regs[1]=(int32_t)regs[1]>>12;pc=pr;}
   else assert(false);
  }
  assert((int32_t)regs[1]==(1200000+offset+(offset>>2))>>12);
  if(site==0xcc){u16 op=f[0xd0/2];assert((op&0xff00)==0x9600);assert(f[(0xd0+4+(op&255)*2)/2]==0x68c);}
 }
 assert(!thps2_follow_camera(ram.data())); // applied once, never patched twice
 puts("PASS: all three camera axes execute 1.25x follow offsets; literal relocation and signature guards valid.");
}
''')
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
