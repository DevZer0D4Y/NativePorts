"""Exercise the actual native canvas layout, including double-buffer isolation."""
from pathlib import Path
import subprocess

r = Path(__file__).resolve().parents[1]
b = r / '.build/slam-canvas-test'
b.mkdir(parents=True, exist_ok=True)
s = (r / 'vendor/pcsx/plugins/gpu_neon/psx_gpu/psx_gpu_parse.c').read_text()
body = s[s.index('static void port_draw_window('):s.index('u32 gpu_parse(')]
fixture = r'''
#include <assert.h>
#include <stdio.h>
typedef unsigned u32;
static int gpu_widescreen_native_width=320,gpu_widescreen_second_x=320;

typedef struct { int offset_x,viewport_start_x,viewport_end_x; } psx_gpu_struct;
static int sign_extend_11bit(unsigned v) { return ((int)(v<<21))>>21; }
int main(void);
'''
main = r'''
int main(void) {
  psx_gpu_struct g = {0}; u32 regs[8]={0};
  regs[4]=319; port_draw_window(&g,regs,1);
  assert(g.offset_x==53 && g.viewport_start_x==0 && g.viewport_end_x==425);
  regs[3]=320;regs[4]=639;regs[5]=320;port_draw_window(&g,regs,1);
  assert(g.offset_x==565 && g.viewport_start_x==512 && g.viewport_end_x==937);
  assert(regs[3]==320 && regs[4]==639 && regs[5]==320);
  port_draw_window(&g,regs,0);
  assert(g.offset_x==320 && g.viewport_start_x==320 && g.viewport_end_x==639);
  regs[4]=511;port_draw_window(&g,regs,1);
  assert(g.offset_x==320 && g.viewport_end_x==511);
  gpu_widescreen_native_width=368;regs[3]=0;regs[4]=383;regs[5]=0;port_draw_window(&g,regs,1);
  assert(g.offset_x==64 && g.viewport_end_x==511);
  regs[3]=384;regs[4]=767;regs[5]=384;port_draw_window(&g,regs,1);
  assert(g.offset_x==576 && g.viewport_start_x==512 && g.viewport_end_x==1023);
  assert(gpu_widescreen_second_x==384);
  puts("PASS: native scale, centered HUD, isolated double buffers, unchanged guest registers, toggle restoration.");
}
'''
p = b / 'canvas.c'
pad = s[s.index('int gpu_widescreen_pad('):].splitlines()[0]
p.write_text(fixture + pad + body + main)
subprocess.run(['clang', str(p), '-o', str(b / 'canvas')], check=True)
subprocess.run([str(b / 'canvas')], check=True)
