"""Execute the camera patch's real SH4 words with tracked and untracked cameras."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
source=(root/'vendor/flycast/core/reios/reios.cpp').read_text()
start=source.index('static bool thps2_follow_camera(')
end=source.index('\nstatic bool reios_locate_bootfile',start)
code=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;
'''+source[start:end]+r'''
int main(){
 std::vector<u8>ram(0x100000);u32 base=0x70700;u16*f=(u16*)(ram.data()+base);
 const unsigned offsets[]={0x8e,0x90,0xb0,0xb4,0xb6,0xb8,0xba,0xbc,0xc0,0xc2,0xc4,0xc6,0xca,0xcc,0xce,0xd0,0xd2,0x198,0x19a,0x19c};
 const u16 words[]={0xa04b,9,0x51c1,0x5237,0xe7f4,0x312c,0x417c,0x1511,0x5238,0x312c,0x417c,0x1512,0x5239,0x312c,0x966b,0x417c,0x1513,0xb,9,0x3b4};
 for(unsigned i=0;i<sizeof(words)/2;i++)f[offsets[i]/2]=words[i];
 for(unsigned i=0x1c4;i<0x1e0;i+=2)f[i/2]=9;
 for(unsigned i=0x384;i<0x3a0;i+=2)f[i/2]=9;
 u32 linked=0x8c000000+base;memcpy(ram.data()+base+0x380,&linked,4);
 f[0x1a8/2]=0x68c;
 auto original=ram;
 f[0x390/2]=0;assert(!thps2_follow_camera(ram.data()));
 ram=original;f=(u16*)(ram.data()+base);assert(thps2_follow_camera(ram.data()));
 for(unsigned site:{0xb8u,0xc2u,0xccu})for(int body:{-800000,0,800000})for(int offset:{-1000000,-1,0,1,1000000})for(bool tracked:{false,true}){
  u32 regs[16]={};regs[1]=1200000;regs[2]=offset;regs[7]=(u32)-12;regs[12]=0x20000;
  u32 bodyptr=tracked?0x30000:0;memcpy(ram.data()+0x203b4,&bodyptr,4);
  for(unsigned axis:{4u,8u,12u})memcpy(ram.data()+0x30000+axis,&body,4);
  unsigned pc=site,pr=0,steps=0;bool t=false;
  while(pc!=site+4){
   assert(++steps<30);u16 op=f[pc/2];
   if((op&0xf000)==0xb000){int d=(int16_t)(op<<4)>>4;u16 slot=f[(pc+2)/2];assert((slot&0xff00)==0xe600);regs[6]=slot&255;pr=pc+4;pc=pc+4+d*2;}
   else if((op&0xff00)==0x9000){regs[0]=(int16_t)f[(pc+4+(op&255)*2)/2];pc+=2;}
   else if(op==0x00ce || op==0x006e){u32 addr=regs[0]+regs[op==0x00ce?12:6];assert(addr+4<ram.size());memcpy(&regs[0],ram.data()+addr,4);pc+=2;}
   else if(op==0x2008){t=regs[0]==0;pc+=2;}
   else if((op&0xff00)==0x8900){pc=t?pc+4+(int8_t)op*2:pc+2;}
   else if((op&0xf000)==0xa000){int d=(int16_t)(op<<4)>>4;assert(f[(pc+2)/2]==9);pc=pc+4+d*2;}
   else if(op==0x6213){regs[2]=regs[1];pc+=2;}
   else if(op==0x3208){regs[2]-=regs[0];pc+=2;}
   else if(op==0x4221){regs[2]=(int32_t)regs[2]>>1;pc+=2;}
   else if(op==0x312c){regs[1]+=regs[2];pc+=2;}
   else if(op==0xb){assert(f[(pc+2)/2]==0x417c);regs[1]=(int32_t)regs[1]>>12;pc=pr;}
   else assert(false);
  }
  int eye=1200000+offset;
  assert((int32_t)regs[1]==(tracked?eye+((eye-body)>>2):eye)>>12);
  if(site==0xcc){u16 op=f[0xd0/2];assert((op&0xff00)==0x9600);assert(f[(0xd0+4+(op&255)*2)/2]==0x68c);}
 }
 assert(!thps2_follow_camera(ram.data()));
 puts("PASS: SH4 camera scales complete eye-to-skater vector by 25% on all axes; untracked camera, literal relocation and cave guards preserved.");
}
'''
b=root/'.build/thps-camera-validation';b.mkdir(parents=True,exist_ok=True)
(b/'camera.cpp').write_text(code)
subprocess.run(['clang++','-std=c++11',str(b/'camera.cpp'),'-o',str(b/'camera')],check=True)
subprocess.run([str(b/'camera')],check=True)
