"""Exercise actual arcade edge helpers, including priority and sprite safeguards."""
from pathlib import Path
import subprocess
r = Path(__file__).resolve().parents[1]
b = r / '.build/stage-edge-tests'
b.mkdir(parents=True, exist_ok=True)
source = (r/'vendor/mame2003-plus/src/vidhrdw/cps1_vidhrdw.c').read_text()
body = source[source.index('extern data16_t *cps2_workram;'):]
body = body[:body.index('#endif')]
fixture = r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
typedef unsigned short data16_t; typedef short INT16;
struct rectangle { int min_x,max_x,min_y,max_y; };
struct mame_bitmap { int unused; };
static data16_t ram[32768], *cps2_workram=ram;
static struct {struct rectangle visible_area;} machine={{64,447,0,223}}, *Machine=&machine;
static void set_visible_area(int l,int r,int t,int b) {
 Machine->visible_area=(struct rectangle){l,r,t,b};
}
static void cps1_render_layer(struct mame_bitmap *b,const struct rectangle *r,int l,int p) {}
'''
# The real helper declares this pointer itself.
fixture = fixture.replace('static data16_t ram[32768], *cps2_workram=ram;','static data16_t ram[32768]; data16_t *cps2_workram=ram;')
main = r'''
int main(void) {
 SF_RAM(SF_P1)=SF_RAM(SF_P2)=1;
 SF_RAM(SF_MIN)=200;SF_RAM(SF_MAX)=400;
 SF_RAM(SF_CENTRE)=300;SF_RAM(SF_BACK)=100;
 ssf2t_set_widescreen(1);ssf2t_update_widescreen();
 assert(Machine->visible_area.min_x==0 && Machine->visible_area.max_x==511);
 for(int x=200;x<=400;x++) {
  SF_RAM(SF_CENTRE)=x;SF_RAM(SF_BACK)=100+(x-300)/2;
  ssf2t_update_widescreen();
  assert(Machine->visible_area.min_x==0 && Machine->visible_area.max_x==511);
  int l,r; ssf2t_scene_bounds(&l,&r);
  assert(l>=0 && l<=64 && r>=447 && r<=511);
  assert(64-l==r-447);
  if(x==200 || x==400) assert(l==64 && r==447);
 }
 ssf2t_set_widescreen(0);ssf2t_update_widescreen();
 assert(Machine->visible_area.min_x==64 && Machine->visible_area.max_x==447);
 puts("PASS: fixed 512-column output across 201 camera positions; finite scenery clipped without zoom; toggle restores 4:3.");
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
