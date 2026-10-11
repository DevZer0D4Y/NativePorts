#include "HangtimeCore.h"
#include <stdio.h>
#include <string.h>
bool ht_validate_rom(const char *path,char *report,size_t capacity) {
 FILE *f=fopen(path,"rb");if(!f){snprintf(report,capacity,"Cannot open game file.");return false;}
 unsigned char h[16]={0};size_t n=fread(h,1,sizeof h,f);fclose(f);
 if(n<8){snprintf(report,capacity,"Game file is empty or truncated.");return false;}
 if(h[0]=='P'&&h[1]=='K')return true;
 snprintf(report,capacity,"Select nbashowt.zip; put nbashowt.chd in the nbashowt folder beside it.");return false;
}
