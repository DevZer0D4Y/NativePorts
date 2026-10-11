#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "CRTSDL.h"
int main(void){
 SDL_SetMainReady();SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengl");SDL_Init(SDL_INIT_VIDEO);
 SDL_Window *w=SDL_CreateWindow("NewPixie OpenGL diagnostic — no game loaded",100,100,640,480,0);
 SDL_Renderer *r=SDL_CreateRenderer(w,-1,SDL_RENDERER_ACCELERATED);
 uint8_t *pixels=malloc(256*192*4);for(int y=0;y<192;y++)for(int x=0;x<256;x++){
  int i=(y*256+x)*4;pixels[i]=y<96?220:20;pixels[i+1]=(x/32)%2?60:120;pixels[i+2]=y<96?20:220;pixels[i+3]=255;
 }
 SDL_Texture *t=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,256,192);SDL_UpdateTexture(t,NULL,pixels,256*4);
 int dw,dh;SDL_GetRendererOutputSize(r,&dw,&dh);SDL_Rect rect={0,0,dw,dh};bool ok=false;
 for(int i=0;i<3;i++){SDL_SetRenderDrawColor(r,10,20,40,255);SDL_RenderClear(r);ok=crt_draw(r,t,pixels,256,192,rect,true);}
 SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,dw,dh,32,SDL_PIXELFORMAT_RGBA32);SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_RGBA32,s->pixels,s->pitch);
 uint8_t *top=(uint8_t*)s->pixels+(dh/4)*s->pitch+(dw/2)*4;uint8_t *bottom=(uint8_t*)s->pixels+(dh*3/4)*s->pitch+(dw/2)*4;
 bool orientation=top[0]>top[2] && bottom[2]>bottom[0];
 FILE *f=fopen(REPORT_FILE,"w");fprintf(f,"%s: GLSL compiled, four NewPixie passes rendered. Orientation %s. Top RGB %d %d %d; bottom RGB %d %d %d.\n",ok&&orientation?"PASS":"FAIL",orientation?"correct":"incorrect",top[0],top[1],top[2],bottom[0],bottom[1],bottom[2]);fclose(f);
 SDL_SaveBMP(s,IMAGE_FILE);SDL_RenderPresent(r);
 bool quit=false;while(!quit){SDL_Event e;while(SDL_PollEvent(&e))if(e.type==SDL_QUIT || (e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_ESCAPE))quit=true;SDL_Delay(10);}
 SDL_Quit();return 0;
}
