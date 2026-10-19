#include "../vendor/mame-current/src/devices/cpu/mips/showtimecopy.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <random>
#include <vector>

// Interpret the actual six captured MIPS words, including the branch delay
// slot, independently of the production batching helper.
static constexpr uint32_t ops[]={0x90a20000,0x24a50001,0x2463ffff,0xa0820000,0x1466fffb,0x24840001};
static void reference(std::vector<uint8_t>&mem,std::array<uint64_t,32>&r,unsigned n,int&cycles){
 for(unsigned i=0;i<n;i++)for(uint32_t op:ops){
  unsigned rs=(op>>21)&31,rt=(op>>16)&31;int32_t imm=int16_t(op);
  switch(op>>26){
   case 0x24:r[rt]=mem[uint32_t(r[rs]+imm)-0x80000000];break;
   case 0x09:r[rt]=uint64_t(int64_t(int32_t(uint32_t(r[rs])+imm)));break;
   case 0x28:mem[uint32_t(r[rs]+imm)-0x80000000]=uint8_t(r[rt]);break;
   case 0x05:assert(r[rs]!=r[rt]);break;
   default:assert(false);
  }
  --cycles;
 }
}
int main(){
 std::mt19937 gen(913);
 for(unsigned test=0;test<15000;test++){
  std::vector<uint8_t>a(8192);for(auto&v:a)v=uint8_t(gen());auto b=a;
  std::array<uint64_t,32>r{};for(auto&v:r)v=(uint64_t(gen())<<32)|gen();
  unsigned src=gen()%4096,dst=gen()%4096;
  if(test%4==0)dst=src;
  if(test%4==1)dst=src+1+gen()%32;
  if(test%4==2)src=dst+1+gen()%32;
  r[3]=1+gen()%2048;r[6]=UINT64_MAX;
  r[4]=uint64_t(int64_t(int32_t(0x80000000u+dst)));
  r[5]=uint64_t(int64_t(int32_t(0x80000000u+src)));
  auto expected=r;int cycles=gen()%3000,expectedCycles=cycles;
  unsigned n=showtime_copy_count(r[3],r[6],cycles);
  showtime_copy_iterations(a.data()+dst,a.data()+src,n,r.data(),cycles);
  reference(b,expected,n,expectedCycles);
  assert(a==b && r==expected && cycles==expectedCycles);
  assert(r[3]!=UINT64_MAX); // final guest iteration is never swallowed
 }
 assert(!showtime_copy_count(0,UINT64_MAX,500));
 assert(!showtime_copy_count(10,0,500));
 assert(!showtime_copy_count(UINT64_MAX,UINT64_MAX,500));
 assert(!showtime_copy_count(10,UINT64_MAX,5));
 puts("PASS: 15,000 independent MIPS copy-loop comparisons, overlap, registers, cycles and final iteration");
}
