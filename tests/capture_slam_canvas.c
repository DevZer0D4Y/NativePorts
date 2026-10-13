/* Private local rendering captures; game pixels are never release assets. */
#include "HangtimeCore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"
#include <assert.h>
static void capture(const char *directory, const char *name) {
    char path[4096]; snprintf(path,sizeof path,"%s/%s.ppm",directory,name);
    unsigned w=ht_width(),h=ht_height(); const unsigned char *p=ht_pixels();
    if (!w || !h || !p) { fprintf(stderr,"No frame\n"); exit(2); }
    FILE *f=fopen(path,"wb"); if(!f) exit(3);
    fprintf(f,"P6\n%u %u\n255\n",w,h);
    for (unsigned i=0;i<w*h;i++) fwrite(p+i*4,1,3,f);
    fclose(f); printf("%s: %ux%u aspect %.4f\n",name,w,h,ht_aspect_ratio());
}
int main(int argc,char **argv) {
    if(argc!=4) return 1;
    if(!ht_start(argv[1],argv[2])) { puts(ht_error()); return 2; }
    ht_set_widescreen(false);
    int frames=atoi(argv[3]);
    for(int i=0;i<frames;i++) ht_run();
    capture(argv[2],"original");
    for (int step=0; step<8; ++step) {
        ht_set_buttons(0,step==0?8:1);
        for (int i=0;i<6;i++) ht_run();
        ht_set_buttons(0,0);
        for (int i=0;i<500;i++) ht_run();
        char name[32]; snprintf(name,sizeof name,"step-%d",step); capture(argv[2],name);
    }
    size_t n=retro_serialize_size(); void *state=malloc(n);
    assert(n && state && retro_serialize(state,n));
    ht_set_widescreen(false); for(int i=0;i<30;i++) ht_run(); capture(argv[2],"comparison-off");
    assert(ht_width()==320); unsigned height=ht_height();
    unsigned char *original=malloc(320*height*4); memcpy(original,ht_pixels(),320*height*4);
    assert(retro_unserialize(state,n)); free(state);
    ht_set_widescreen(true);
    for(int i=0;i<30;i++) ht_run();
    capture(argv[2],"widescreen");
    assert(ht_width()==426 && ht_height()==height);
    for(unsigned y=0;y<height;y++)
      assert(!memcmp(original+y*320*4,ht_pixels()+(y*426+53)*4,320*4));
    printf("PASS: expanded native court; all %u central pixels match original, including HUD and sprites.\n",320*height);
    ht_set_widescreen(false); for(int i=0;i<30;i++)ht_run();assert(ht_width()==320);
    free(original);
    ht_stop();
}
