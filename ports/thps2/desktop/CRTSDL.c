#define GL_SILENCE_DEPRECATION
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "Shaders.h"
#define GL_FUNCTIONS(X) \
 X(PFNGLCREATESHADERPROC,CreateShader) X(PFNGLSHADERSOURCEPROC,ShaderSource) \
 X(PFNGLCOMPILESHADERPROC,CompileShader) X(PFNGLGETSHADERIVPROC,GetShaderiv) \
 X(PFNGLGETSHADERINFOLOGPROC,GetShaderInfoLog) X(PFNGLCREATEPROGRAMPROC,CreateProgram) \
 X(PFNGLATTACHSHADERPROC,AttachShader) X(PFNGLLINKPROGRAMPROC,LinkProgram) \
 X(PFNGLGETPROGRAMIVPROC,GetProgramiv) X(PFNGLGETPROGRAMINFOLOGPROC,GetProgramInfoLog) \
 X(PFNGLUSEPROGRAMPROC,UseProgram) X(PFNGLGETUNIFORMLOCATIONPROC,GetUniformLocation) \
 X(PFNGLUNIFORM1IPROC,Uniform1i) X(PFNGLUNIFORM1FPROC,Uniform1f) \
 X(PFNGLUNIFORM2FPROC,Uniform2f) X(PFNGLACTIVETEXTUREPROC,ActiveTexture) \
 X(PFNGLGENFRAMEBUFFERSEXTPROC,GenFramebuffersEXT) X(PFNGLBINDFRAMEBUFFEREXTPROC,BindFramebufferEXT) \
 X(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC,FramebufferTexture2DEXT) X(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC,CheckFramebufferStatusEXT)
#define DECLARE(type,name) static type g##name;
GL_FUNCTIONS(DECLARE)
#undef DECLARE

static GLuint program,textures[5],fbo;
static unsigned tex_w,tex_h,feedback_index,frame_number;
static GLint u_mode,u_frame,u_source,u_output;
static void error(const char *message) { SDL_Log("CRT: %s",message); }
static GLuint compile(GLenum type,const char *source) {
    GLuint s=gCreateShader(type);gShaderSource(s,1,&source,NULL);gCompileShader(s);GLint ok;
    gGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096];gGetShaderInfoLog(s,sizeof log,NULL,log);error(log);return 0;}return s;
}
static bool initialize_gl(void) {
#define LOAD(type,name) g##name=(type)SDL_GL_GetProcAddress("gl"#name);if(!g##name){error("OpenGL 2.1 with framebuffer support is required.");return false;}
    GL_FUNCTIONS(LOAD)
#undef LOAD
    program=gCreateProgram();gAttachShader(program,compile(GL_VERTEX_SHADER,crt_vertex));gAttachShader(program,compile(GL_FRAGMENT_SHADER,crt_fragment));gLinkProgram(program);
    GLint ok;gGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){char log[4096];gGetProgramInfoLog(program,sizeof log,NULL,log);error(log);return false;}
    gUseProgram(program);
    gUniform1i(gGetUniformLocation(program,"sourceTex"),0);gUniform1i(gGetUniformLocation(program,"feedbackTex"),1);gUniform1i(gGetUniformLocation(program,"blurTex"),2);
    u_mode=gGetUniformLocation(program,"mode");u_frame=gGetUniformLocation(program,"frame");u_source=gGetUniformLocation(program,"sourceSize");u_output=gGetUniformLocation(program,"outputSize");
    glGenTextures(5,textures);gGenFramebuffersEXT(1,&fbo);return true;
}
static void quad(bool offscreen) {
    float top=offscreen?-1:1,bottom=-top;glBegin(GL_QUADS);
    glTexCoord2f(0,0);glVertex2f(-1,top);glTexCoord2f(1,0);glVertex2f(1,top);
    glTexCoord2f(1,1);glVertex2f(1,bottom);glTexCoord2f(0,1);glVertex2f(-1,bottom);glEnd();
}
static void bind_texture(unsigned unit,GLuint tex) {gActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,tex);}
static void shader_pass(int mode,GLuint source,GLuint target,unsigned w,unsigned h) {
    gBindFramebufferEXT(GL_FRAMEBUFFER_EXT,target?fbo:0);
    if(target){gFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,target,0);
        if(gCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)!=GL_FRAMEBUFFER_COMPLETE_EXT){error("Incomplete CRT framebuffer.");return;}glViewport(0,0,w,h);}
    gUseProgram(program);gUniform1i(u_mode,mode);gUniform1f(u_frame,(float)frame_number);
    gUniform2f(u_source,(float)tex_w,(float)tex_h);gUniform2f(u_output,(float)w,(float)h);
    bind_texture(0,source);bind_texture(1,target==textures[1+feedback_index]?textures[0]:textures[1+feedback_index]);
    bind_texture(2,target==textures[4]?textures[0]:textures[4]);gActiveTexture(GL_TEXTURE0);quad(target!=0);
}

bool crt_draw(SDL_Renderer *renderer,SDL_Texture *binding,const uint8_t *pixels,unsigned w,unsigned h,SDL_Rect rect,bool enabled) {
 static bool ready=false;
 if(!w || !h || !binding || !pixels)return false;
 SDL_RenderFlush(renderer);
 if(SDL_GL_BindTexture(binding,NULL,NULL)<0)return false;
 GLint oldProgram=0,oldViewport[4],oldFbo=0;
 glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram);glGetIntegerv(GL_VIEWPORT,oldViewport);glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&oldFbo);
 GLboolean blend=glIsEnabled(GL_BLEND);glDisable(GL_BLEND);
 glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
 if(!ready){ready=initialize_gl();}
 if(!ready)goto restore;
 if(w!=tex_w||h!=tex_h){tex_w=w;tex_h=h;feedback_index=0;uint8_t *blank=calloc(w*h,4);
  for(unsigned i=0;i<5;i++){bind_texture(0,textures[i]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
   glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,blank);}free(blank);}
 bind_texture(0,textures[0]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels);frame_number++;
 if(enabled){unsigned next=1-feedback_index;shader_pass(1,textures[0],textures[1+next],w,h);feedback_index=next;
  shader_pass(2,textures[1+next],textures[3],w,h);shader_pass(3,textures[3],textures[4],w,h);}
 gBindFramebufferEXT(GL_FRAMEBUFFER_EXT,oldFbo);
 int dw,dh;SDL_GetRendererOutputSize(renderer,&dw,&dh);glViewport(rect.x,dh-rect.y-rect.h,rect.w,rect.h);
 shader_pass(enabled?4:0,enabled?textures[1+feedback_index]:textures[0],0,rect.w,rect.h);
restore:
 if(gBindFramebufferEXT)gBindFramebufferEXT(GL_FRAMEBUFFER_EXT,oldFbo);if(gUseProgram)gUseProgram(oldProgram);if(gActiveTexture)gActiveTexture(GL_TEXTURE0);
 glViewport(oldViewport[0],oldViewport[1],oldViewport[2],oldViewport[3]);
 glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
 if(blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);SDL_GL_UnbindTexture(binding);
 return ready;
}
