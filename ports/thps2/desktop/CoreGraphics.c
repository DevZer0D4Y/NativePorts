#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <string.h>
#include "../ios/Hangtime/CoreGraphics.h"
static SDL_Window *window,*previous_window;static SDL_GLContext context,previous_context;
static GLuint framebuffer,texture,depth;static bool pending;
static struct retro_hw_render_callback callback;
#define FUNCS(X) X(PFNGLGENFRAMEBUFFERSEXTPROC,GenFramebuffersEXT) X(PFNGLBINDFRAMEBUFFEREXTPROC,BindFramebufferEXT) X(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC,FramebufferTexture2DEXT) X(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC,CheckFramebufferStatusEXT) X(PFNGLGENRENDERBUFFERSEXTPROC,GenRenderbuffersEXT) X(PFNGLBINDRENDERBUFFEREXTPROC,BindRenderbufferEXT) X(PFNGLRENDERBUFFERSTORAGEEXTPROC,RenderbufferStorageEXT) X(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC,FramebufferRenderbufferEXT)
#define DECLARE(type,name) static type g##name;
FUNCS(DECLARE)
static uintptr_t current_framebuffer(void){return framebuffer;}
static retro_proc_address_t proc(const char *name){return (retro_proc_address_t)SDL_GL_GetProcAddress(name);}
bool hw_prepare(struct retro_hw_render_callback *c){
 if(c->context_type!=RETRO_HW_CONTEXT_OPENGL)return false;
 previous_window=SDL_GL_GetCurrentWindow();previous_context=SDL_GL_GetCurrentContext();
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,0);
 window=SDL_CreateWindow("Dreamcast renderer",0,0,1024,1024,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);if(!window)return false;
 context=SDL_GL_CreateContext(window);if(!context)return false;
#define LOAD(type,name) g##name=(type)SDL_GL_GetProcAddress("gl"#name);if(!g##name)return false;
 FUNCS(LOAD)
 gGenFramebuffersEXT(1,&framebuffer);gBindFramebufferEXT(GL_FRAMEBUFFER_EXT,framebuffer);
 glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1024,1024,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 gFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,texture,0);
 gGenRenderbuffersEXT(1,&depth);gBindRenderbufferEXT(GL_RENDERBUFFER_EXT,depth);gRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_DEPTH24_STENCIL8_EXT,1024,1024);
 gFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,depth);gFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_STENCIL_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,depth);
 bool ok=gCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT;
 c->get_current_framebuffer=current_framebuffer;c->get_proc_address=proc;callback=*c;pending=true;
 SDL_GL_MakeCurrent(previous_window,previous_context);return ok;
}
void hw_begin(void){previous_window=SDL_GL_GetCurrentWindow();previous_context=SDL_GL_GetCurrentContext();SDL_GL_MakeCurrent(window,context);if(pending){pending=false;if(callback.context_reset)callback.context_reset();}}
void hw_end(void){SDL_GL_MakeCurrent(previous_window,previous_context);}
void hw_read(uint8_t *pixels,unsigned w,unsigned h){if(w>1024||h>1024)return;gBindFramebufferEXT(GL_FRAMEBUFFER_EXT,framebuffer);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
 if(callback.bottom_left_origin){uint8_t row[4096];for(unsigned y=0;y<h/2;y++){memcpy(row,pixels+y*w*4,w*4);memcpy(pixels+y*w*4,pixels+(h-1-y)*w*4,w*4);memcpy(pixels+(h-1-y)*w*4,row,w*4);}}
}
void hw_release(void){if(!context)return;hw_begin();if(callback.context_destroy)callback.context_destroy();hw_end();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);context=NULL;window=NULL;pending=false;}
