/* Private local rendering captures; game pixels are never release assets. */
#include "HangtimeCore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "libretro.h"
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
    extern int multitap1; assert(multitap1==1);
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
    for(int i=0;i<4000;i++) ht_run();ht_set_buttons(0,1);for(int i=0;i<6;i++)ht_run();ht_set_buttons(0,0);for(int i=0;i<180;i++)ht_run();capture(argv[2],"gameplay-off");
    size_t n=retro_serialize_size();void *state=malloc(n);assert(n && state && retro_serialize(state,n));
    for(int i=0;i<30;i++)ht_run();
    capture(argv[2],"frame-reference");unsigned w=ht_width(),h=ht_height();assert(w==368);
    unsigned char *original=malloc(w*h*4);assert(original);memcpy(original,ht_pixels(),w*h*4);
    assert(retro_unserialize(state,n));free(state);
    ht_set_widescreen(true);
    for(int i=0;i<30;i++) ht_run();
    capture(argv[2],"widescreen");
    assert(ht_width()==496 && ht_height()==h);
    unsigned changed=0,max_delta=0;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){int different=0;for(unsigned c=0;c<3;c++){int delta=abs(original[(y*w+x)*4+c]-ht_pixels()[(y*496+x+64)*4+c]);if(delta)different=1;if(delta>max_delta)max_delta=delta;}changed+=different;}
    printf("Comparison: %u changed pixels, max channel delta %u\n",changed,max_delta);assert(changed*20 < w*h);
    /* Score/time are 2D overlays and must match exactly. World rasterization
     * can differ at newly exposed/clipped triangle edges. */
    for(unsigned y=20;y<47;y++)for(unsigned x=125;x<245;x++)
      assert(!memcmp(original+(y*w+x)*4,ht_pixels()+(y*496+x+64)*4,4));
    unsigned edge_content=0;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<496;x++)if(x<64 || x>=432){const unsigned char *p=ht_pixels()+(y*496+x)*4;if(p[0]>16 || p[1]>16 || p[2]>16)edge_content++;}
    assert(edge_content>1024);
    free(original);ht_set_widescreen(false);for(int i=0;i<30;i++)ht_run();assert(ht_width()==368);
    puts("PASS: 4-player multitap; native 368 -> 496 -> 368 toggle; score/time HUD exact; native central geometry retained; extra side scenery drawn.");
    ht_stop();
}
