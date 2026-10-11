#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES3/gl.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include "CoreGraphics.h"
static EAGLContext *context,*previous;
static GLuint framebuffer,texture,depth;
static struct retro_hw_render_callback callback;
static bool pending;
static uintptr_t current_framebuffer(void){return framebuffer;}
static retro_proc_address_t proc(const char *name){return (retro_proc_address_t)dlsym(RTLD_DEFAULT,name);}
bool hw_prepare(struct retro_hw_render_callback *c){
 if(c->context_type!=RETRO_HW_CONTEXT_OPENGLES2 && c->context_type!=RETRO_HW_CONTEXT_OPENGLES3 && c->context_type!=RETRO_HW_CONTEXT_OPENGLES_VERSION)return false;
 context=[[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];if(!context)return false;
 previous=[EAGLContext currentContext];[EAGLContext setCurrentContext:context];
 glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
 glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1024,1024,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
 glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,1024,1024);
 glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth);
 bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
 c->get_current_framebuffer=current_framebuffer;c->get_proc_address=proc;callback=*c;pending=true;
 [EAGLContext setCurrentContext:previous];return ok;
}
void hw_begin(void){previous=[EAGLContext currentContext];[EAGLContext setCurrentContext:context];if(pending){pending=false;if(callback.context_reset)callback.context_reset();}}
void hw_end(void){[EAGLContext setCurrentContext:previous];}
void hw_read(uint8_t *pixels,unsigned w,unsigned h){
 if(w>1024||h>1024)return;glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
 if(callback.bottom_left_origin){uint8_t row[4096];for(unsigned y=0;y<h/2;y++){memcpy(row,pixels+y*w*4,w*4);memcpy(pixels+y*w*4,pixels+(h-1-y)*w*4,w*4);memcpy(pixels+(h-1-y)*w*4,row,w*4);}}
}
void hw_release(void){if(!context)return;hw_begin();if(callback.context_destroy)callback.context_destroy();glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&texture);glDeleteRenderbuffers(1,&depth);hw_end();context=nil;pending=false;}
