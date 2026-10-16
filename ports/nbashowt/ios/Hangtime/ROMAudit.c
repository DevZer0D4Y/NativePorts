#include "HangtimeCore.h"
#include <stdio.h>
#include <string.h>
bool ht_validate_rom(const char *path,char *report,size_t capacity) {
 FILE *f=fopen(path,"rb");if(!f){snprintf(report,capacity,"Cannot open game file.");return false;}
 unsigned char h[16]={0};size_t n=fread(h,1,sizeof h,f);fseek(f,0,SEEK_END);long size=ftell(f);
 char iso[5]={0};fseek(f,16*2352+25,SEEK_SET);size_t ni=fread(iso,1,5,f);fclose(f);
 static const unsigned char sync[12]={0,255,255,255,255,255,255,255,255,255,255,0};
 if(n==16 && size>=32*2352 && size%2352==0 && ni==5 && !memcmp(iso,"CD001",5) && !memcmp(h,sync,12) && h[15]==2)return true;
 snprintf(report,capacity,"Select the extracted PlayStation nbashowt.bin disc image, not the ZIP/7z archive or an arcade ROM.");return false;
}
