"""Exercise actual arcade edge helpers, including priority and sprite safeguards."""
from pathlib import Path
import subprocess
r = Path(__file__).resolve().parents[1]
b = r / '.build/stage-edge-tests'
b.mkdir(parents=True, exist_ok=True)
source = (r/'vendor/mame2003-plus/src/vidhrdw/cps1_vidhrdw.c').read_text()
body = source[source.index('static void ssf2t_render_layer('):]
body = body[:body.index('#endif')]
fixture = r'''
#include <assert.h>
#include <stdio.h>
struct rectangle { int min_x,max_x,min_y,max_y; };
struct mame_bitmap { unsigned pixels[2][512]; };
static struct mame_bitmap priority, *priority_bitmap=&priority;
static int left=64,right=447;
#define read_pixel(b,x,y) ((b)->pixels[y][x])
#define plot_pixel(b,x,y,p) ((b)->pixels[y][x]=(p))
void ssf2t_layer_clip(int l,const struct rectangle *r,struct rectangle *c) {
 *c=*r;c->min_x=left;c->max_x=right;
}
void cps1_render_layer(struct mame_bitmap *b,const struct rectangle *r,int l,int p) {
 for(int y=r->min_y;y<=r->max_y;y++) for(int x=r->min_x;x<=r->max_x;x++) {
  plot_pixel(b,x,y,100+x);plot_pixel(priority_bitmap,x,y,x%4);
 }
}
'''
main = r'''
int main(void) {
 struct mame_bitmap b={0}; struct rectangle r={0,511,0,1};
 ssf2t_render_layer(&b,&r,2,1);
 for(int y=0;y<2;y++) for(int x=0;x<512;x++) {
  int src=x<64?127-x:x>447?895-x:x;
  assert(read_pixel(&b,x,y)==100+src);
  assert(read_pixel(priority_bitmap,x,y)==src%4);
 }
 left=0;right=511;ssf2t_render_layer(&b,&r,2,1);
 for(int x=0;x<512;x++) assert(read_pixel(&b,x,0)==100+x);
 puts("PASS: finite stage extension fills both edges, preserves original pixels and priority.");
}
'''
p=b/'sf.c';p.write_text(fixture+body+main)
subprocess.run(['clang',str(p),'-o',str(b/'sf')],check=True)
subprocess.run([str(b/'sf')],check=True)
source = (r/'vendor/mame2003-plus/src/vidhrdw/midtunit_vidhrdw.c').read_text()
declarations=source[source.index('#define U3_TILES'):]
declarations=declarations[:declarations.index('#endif')]
body=source[source.index('static void umk3_extend_tiles('):]
body=body[:body.index('#endif')]
fixture=r'''
#include <assert.h>
#include <stdio.h>
typedef unsigned UINT32; typedef unsigned short UINT16;
#define U3_PAD 56
#define XPOSMASK 1023
static struct {int xpos,width,height,xstep,ystep,ypos,palette,preskip,postskip,bpp;UINT32 offset;} dma_state;
static unsigned char midyunit_gfx_rom[8192];
static unsigned extensions;
void dma_draw_current(int command) { if(dma_state.xpos==456) extensions++; }
'''
main=r'''
void tile(int x,unsigned image,int palette) {
 dma_state.xpos=x;dma_state.offset=image;dma_state.palette=palette;
 umk3_extend_tiles(0x8000);
}
int main(void) {
 dma_state.width=64;dma_state.height=32;dma_state.xstep=dma_state.ystep=256;
 tile(328,10,2);tile(392,20,2);
 assert(extensions==0); /* Two distinct props/fighters are not a band. */
 umk3_tile_count=extensions=0;
 tile(264,10,2);tile(328,20,2);tile(392,30,2);
 assert(extensions>0);
 umk3_tile_count=extensions=0;
 tile(264,10,2);tile(328,20,3);tile(392,30,2);
 assert(extensions==0);
 puts("PASS: mixed background bands extend; unrelated pairs and palettes do not.");
}
'''
p=b/'umk.c';p.write_text(fixture+declarations+body+main)
subprocess.run(['clang',str(p),'-o',str(b/'umk')],check=True)
subprocess.run([str(b/'umk')],check=True)
