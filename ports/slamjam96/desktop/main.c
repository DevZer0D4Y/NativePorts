#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "HangtimeCore.h"
#include "CRTSDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <xinput.h>
#endif

/* RetroPad bits read by the core's classic layout: B = Turbo, Y = Pass/Steal, A = Shoot/Block. */
enum { PAD_TURBO = 1 << 0, PAD_PASS = 1 << 1, PAD_COIN = 1 << 2, PAD_START = 1 << 3,
       PAD_UP = 1 << 4, PAD_DOWN = 1 << 5, PAD_LEFT = 1 << 6, PAD_RIGHT = 1 << 7, PAD_SHOOT = 1 << 8 };
enum { A_UP, A_DOWN, A_LEFT, A_RIGHT, A_SHOOT, A_PASS, A_TURBO, A_SPIN, A_DUNK, A_SHOVE, A_COIN, A_START, A_COUNT };
static const char *action_title[A_COUNT] = {"UP","DOWN","LEFT","RIGHT","SHOOT / JUMP","PASS / CHANGE","TURBO","STEAL","PICK","ISOLATE","SELECT","START"};
static const char *action_id[A_COUNT] = {"up","down","left","right","shoot","pass","turbo","spin","dunk","shove","coin","start"};
static const uint16_t action_bits[A_COUNT] = {PAD_UP,PAD_DOWN,PAD_LEFT,PAD_RIGHT,256,1,4096,2,1024,2048,4,8};
/* Share and the paddles exist on Xbox Series and Elite controllers. pad_name is what settings.cfg stores,
   pad_title what the controls screen shows (Xbox names). */
enum { P_A, P_B, P_X, P_Y, P_LB, P_RB, P_LT, P_RT, P_L3, P_R3, P_BACK, P_SHARE, P_PADDLE1, P_PADDLE2, P_PADDLE3, P_PADDLE4, P_COUNT };
#include "XboxInput.h"
static const char *pad_name[P_COUNT] = {"A","B","X","Y","LB","RB","LT","RT","L3","R3","BACK","SHARE","P1","P2","P3","P4"};
static const char *pad_title[P_COUNT] = {"A","B","X","Y","LB","RB","LT","RT","LS","RS","VIEW","SHARE","P1","P2","P3","P4"};
/* Slam uses four PS1 multitap slots in connection order. */
static const unsigned player_port[4] = {0, 1, 2, 3};
static const SDL_Scancode default_keys[A_COUNT] = {SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_X,SDL_SCANCODE_C,SDL_SCANCODE_Z,SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D,SDL_SCANCODE_5,SDL_SCANCODE_1};
/* NBA 2K style pad: X shoots, A passes, triggers are Turbo. Start is fixed: tap = START, hold = pause. */
static const unsigned default_pads[A_COUNT] = {0,0,0,0,1u<<P_X,1u<<P_A,1u<<P_RT|1u<<P_LT|1u<<P_RB,1u<<P_B,1u<<P_Y,1u<<P_LB,1u<<P_BACK,0};
static SDL_Scancode keys[A_COUNT];
static unsigned pads[A_COUNT];

enum { SCREEN_FULL, SCREEN_SQUARE, SCREEN_ARCADE };
static const char *screen_name[3] = {"FULL","SQUARE","ARCADE"};
static int screen_mode = SCREEN_ARCADE;
static bool fullscreen = true;
/* Color unused margins while preserving the selected image proportions. */
static bool colorSides = true;
static bool game_wide = true;
/* NewPixie CRT processing, independent of game ROM patches. */
static int crtMode = 1;
static const char *crtMode_name[2] = {"OFF","ON"};
enum { MENU_NONE, MENU_PAUSE, MENU_CONTROLS };
enum { ITEM_RESUME, ITEM_CRT, ITEM_CONTROLS, ITEM_WIDE, ITEM_SIDES, ITEM_SCREEN, ITEM_FULLSCREEN, ITEM_RESTART, ITEM_QUIT, ITEM_COUNT };
enum { MENU_TOP = 104, MENU_STEP = 48, MENU_H = 42, ROW_TOP = 136, ROW_STEP = 33 };
static int menu, selection, capture = -1;
static char notice[96];
static Uint32 notice_until;

/* Per-player input; SPIN is a timed TURBO double tap and DUNK/SHOVE lead with one frame of TURBO. */
struct player { bool held[A_COUNT], was[A_COUNT], start_down, start_used; int spin, combo[2], start_pulse; Uint32 start_since; unsigned suppressed; };
static struct player players[4];

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioDeviceID audio_device;
static SDL_GameController *controllers[4];
static struct pad_sample previous_pad[4];
static bool running;
static char *save_dir;

static void audio_callback(void *userdata, Uint8 *stream, int length) {
    (void)userdata;
    float *out = (float *)stream;
    unsigned remaining = (unsigned)length / (sizeof(float) * 2);
    while (remaining) {
        float left[256], right[256];
        unsigned n = remaining < 256 ? remaining : 256;
        ht_audio_read(left,right,n);
        for (unsigned i=0;i<n;i++) { *out++=left[i]; *out++=right[i]; }
        remaining -= n;
    }
}
static void message(const char *text) { SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Slam’n’Jam 96",text,window); }
static void show_notice(const char *text) { snprintf(notice,sizeof notice,"%s",text); notice_until=SDL_GetTicks()+3000; }

static void config_path(char *out,size_t size) { snprintf(out,size,"%ssettings.cfg",save_dir); }
static void save_config(void) {
    char path[4096]; config_path(path,sizeof path);
    FILE *f=fopen(path,"w"); if (!f) return;
    fprintf(f,"screen=%d\nfullscreen=%d\ncolorSides=%d\ncrtMode=%d\nwidescreen=%d\n",screen_mode,fullscreen,colorSides,crtMode,game_wide);
    for (int a=0;a<A_COUNT;a++) {
        fprintf(f,"key.%s=%s\npad.%s=",action_id[a],SDL_GetScancodeName(keys[a]),action_id[a]);
        for (int b=0;b<P_COUNT;b++) if (pads[a] & (1u << b)) fprintf(f,"%s ",pad_name[b]);
        fputc('\n',f);
    }
    fclose(f);
}
static void load_config(void) {
    memcpy(keys,default_keys,sizeof keys); memcpy(pads,default_pads,sizeof pads);
    char path[4096], line[256]; config_path(path,sizeof path);
    FILE *f=fopen(path,"r"); if (!f) return;
    while (fgets(line,sizeof line,f)) {
        line[strcspn(line,"\r\n")]=0;
        char *value=strchr(line,'='); if (!value) continue;
        *value++=0;
        if (!strcmp(line,"screen")) { int m=atoi(value); screen_mode=m>=0 && m<3 ? m : SCREEN_ARCADE; }
        else if (!strcmp(line,"fullscreen")) fullscreen=atoi(value)!=0;
        else if (!strcmp(line,"colorSides")) colorSides=atoi(value)!=0;
        else if (!strcmp(line,"widescreen")) game_wide=atoi(value)!=0;
        else if (!strcmp(line,"crtMode")) { int h=atoi(value); crtMode=h>=0 && h<2 ? h : 0; }
        else for (int a=0;a<A_COUNT;a++) {
            if (!strncmp(line,"key.",4) && !strcmp(line+4,action_id[a])) keys[a]=SDL_GetScancodeFromName(value);
            if (!strncmp(line,"pad.",4) && !strcmp(line+4,action_id[a])) {
                pads[a]=0;
                for (char *t=strtok(value," ");t;t=strtok(NULL," ")) for (int b=0;b<P_COUNT;b++) if (!strcmp(t,pad_name[b])) pads[a] |= 1u << b;
            }
        }
    }
    fclose(f);
}
static void bind_key(int action,SDL_Scancode scancode) {
    /* Swap with whatever action owned the key, so nothing is left with a duplicate. */
    for (int a=0;a<A_COUNT;a++) if (a!=action && keys[a]==scancode) keys[a]=keys[action];
    keys[action]=scancode;
}
static void bind_pad(int action,int input) {
    for (int a=0;a<A_COUNT;a++) pads[a] &= ~(1u << input);
    pads[action]=1u << input;
}
static void restore_defaults(void) {
    memcpy(keys,default_keys,sizeof keys); memcpy(pads,default_pads,sizeof pads);
    save_config(); show_notice("DEFAULT CONTROLS RESTORED");
}

static bool pad_down(SDL_GameController *g,int input) {
    static const SDL_GameControllerButton map[P_COUNT]={SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_INVALID,
        SDL_CONTROLLER_BUTTON_INVALID,SDL_CONTROLLER_BUTTON_LEFTSTICK,SDL_CONTROLLER_BUTTON_RIGHTSTICK,SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_MISC1,SDL_CONTROLLER_BUTTON_PADDLE1,SDL_CONTROLLER_BUTTON_PADDLE2,SDL_CONTROLLER_BUTTON_PADDLE3,
        SDL_CONTROLLER_BUTTON_PADDLE4};
    if (input==P_LT) return SDL_GameControllerGetAxis(g,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>16000;
    if (input==P_RT) return SDL_GameControllerGetAxis(g,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>16000;
    return SDL_GameControllerGetButton(g,map[input]);
}
static struct pad_sample read_pad(SDL_GameController *g) {
    struct pad_sample sample={0};
    if (!g) return sample;
    for (int b=0;b<P_COUNT;b++) if (pad_down(g,b)) sample.down |= 1u << b;
    Sint16 x=SDL_GameControllerGetAxis(g,SDL_CONTROLLER_AXIS_LEFTX), y=SDL_GameControllerGetAxis(g,SDL_CONTROLLER_AXIS_LEFTY);
    if (SDL_GameControllerGetButton(g,SDL_CONTROLLER_BUTTON_DPAD_UP) || y< -10000) sample.directions |= PAD_UP;
    if (SDL_GameControllerGetButton(g,SDL_CONTROLLER_BUTTON_DPAD_DOWN) || y>10000) sample.directions |= PAD_DOWN;
    if (SDL_GameControllerGetButton(g,SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x< -10000) sample.directions |= PAD_LEFT;
    if (SDL_GameControllerGetButton(g,SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x>10000) sample.directions |= PAD_RIGHT;
    sample.start=SDL_GameControllerGetButton(g,SDL_CONTROLLER_BUTTON_START);
    sample.home=SDL_GameControllerGetButton(g,SDL_CONTROLLER_BUTTON_GUIDE);
#ifdef _WIN32
    /* SDL exposes the actual XInput user index in the native device path.
       Read its fixed button layout directly, without a controller mapping or
       packet-number/event dependency. SDL still supplies Share and paddles. */
    const char *path=SDL_JoystickPath(SDL_GameControllerGetJoystick(g));
    unsigned user; char extra;
    if (path && sscanf(path,"XInput#%u%c",&user,&extra)==1 && user<4) {
        typedef DWORD (WINAPI *get_state_fn)(DWORD,XINPUT_STATE *);
        static get_state_fn get_state;
        static bool attempted;
        if (!attempted) {
            const char *dlls[]={"xinput1_4.dll","xinput1_3.dll","xinput9_1_0.dll"};
            attempted=true;
            for (unsigned i=0;i<3 && !get_state;i++) {
                HMODULE module=LoadLibraryExA(dlls[i],NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
                if (module) {
                    get_state=(get_state_fn)GetProcAddress(module,"XInputGetState");
                    if (!get_state) FreeLibrary(module);
                }
            }
        }
        XINPUT_STATE state;
        if (get_state && get_state(user,&state)==ERROR_SUCCESS) {
            struct pad_sample native=xbox_sample(state.Gamepad.wButtons,state.Gamepad.bLeftTrigger,
                state.Gamepad.bRightTrigger,state.Gamepad.sThumbLX,state.Gamepad.sThumbLY);
            native.down |= sample.down & ((1u<<P_SHARE)|(1u<<P_PADDLE1)|(1u<<P_PADDLE2)|(1u<<P_PADDLE3)|(1u<<P_PADDLE4));
            native.home=sample.home;
            return native;
        }
    }
#endif
    return sample;
}
/* Clears every player; with suppress, buttons still held (e.g. A on RESUME) stay ignored until released. */
static void reset_players(bool suppress) {
    for (unsigned i=0;i<4;i++) {
        memset(&players[i],0,sizeof players[i]); players[i].spin=-1;
        if (suppress && controllers[i]) {
            struct pad_sample sample=read_pad(controllers[i]);
            players[i].suppressed=sample.down;
            players[i].start_down=sample.start;
            players[i].start_used=true;
        }
    }
}

static void set_paused(bool value) {
    for (unsigned i=0;i<4;i++) ht_set_buttons(i,0);
    if (value && running) ht_save_data();
    if (audio_device) SDL_PauseAudioDevice(audio_device,value);
    ht_audio_clear();
}
static void open_menu(int which) {
    if (!running) return;
    if (menu==MENU_NONE) set_paused(true);
    menu=which; selection=0; capture=-1;
}
static void close_menu(void) {
    menu=MENU_NONE; capture=-1;
    reset_players(true);
    set_paused(false);
}
static void toggle_fullscreen(void) {
    fullscreen=!fullscreen;
    SDL_SetWindowFullscreen(window,fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    save_config();
}
static void toggle_widescreen(void) {
    game_wide=!game_wide; ht_set_widescreen(game_wide); save_config();
    show_notice(game_wide ? "WIDESCREEN ON" : "WIDESCREEN OFF");
}
static void toggle_colorSides(void) {
    colorSides=!colorSides;  save_config();
    show_notice(colorSides ? "COLOR SIDES ON" : "COLOR SIDES OFF");
}
static void cycle_crtMode(int delta) {
    crtMode=(crtMode+2+delta)%2;  save_config();
    char text[32]; snprintf(text,sizeof text,"CRT FILTER: %s",crtMode_name[crtMode]); show_notice(text);
}
static void cycle_screen(int delta) {
    screen_mode=(screen_mode+3+delta)%3; save_config();
    char text[32]; snprintf(text,sizeof text,"SCREEN: %s",screen_name[screen_mode]); show_notice(text);
}

static bool copy_file(const char *source,const char *target) {
    FILE *in=fopen(source,"rb"); if (!in) return false;
    FILE *out=fopen(target,"wb"); if (!out) { fclose(in); return false; }
    char bytes[16384]; size_t n; bool ok=true;
    while ((n=fread(bytes,1,sizeof bytes,in))) if (fwrite(bytes,1,n,out)!=n) { ok=false; break; }
    if (ferror(in)) ok=false;
    if (fclose(out)) ok=false;
    fclose(in); return ok;
}
static bool load_rom(const char *path) {
    if (running) { message("To change the ROM set, close and reopen the app."); return false; }
    char report[8192];
    if (!ht_validate_rom(path,report,sizeof report)) { message(report); return false; }
    size_t capacity=strlen(save_dir)+32;
    char *canonical=malloc(capacity), *staging=malloc(capacity);
    if (!canonical || !staging) { free(canonical); free(staging); message("Out of memory."); return false; }
    snprintf(canonical,capacity,"%sslamjam96.chd",save_dir);
    snprintf(staging,capacity,"%simport.tmp",save_dir);
    bool ready=true;
    if (strcmp(path,canonical)) {
        ready=copy_file(path,staging);
        if (ready) {
#ifdef _WIN32
            ready=MoveFileExA(staging,canonical,MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)!=0;
#else
            ready=rename(staging,canonical)==0;
#endif
        }
    }
    if (!ready) { message("Could not copy the ROM set into the data folder."); free(canonical); free(staging); return false; }
    running=ht_start(canonical,save_dir);
    free(canonical); free(staging);
    if (!running) { message(ht_error()); return false; }
    SDL_AudioSpec spec={0}, actual={0};
    spec.freq=(int)ht_sample_rate(); spec.format=AUDIO_F32SYS; spec.channels=2; spec.samples=1024; spec.callback=audio_callback;
    audio_device=SDL_OpenAudioDevice(NULL,0,&spec,&actual,0);
    if (!audio_device) message(SDL_GetError());
    SDL_SetWindowTitle(window,"Slam’n’Jam 96 - Esc: menu | F1: controls | F8: widescreen | F9: color sides | F10: screen mode | F11: fullscreen");
    reset_players(false); set_paused(false);
    show_notice("PRESS 5 FOR A CREDIT, THEN 1 TO START");
    return true;
}
static void choose_file(void) {
#ifdef _WIN32
    char path[4096]="";
    OPENFILENAMEA dialog={0}; dialog.lStructSize=sizeof dialog;
    dialog.lpstrFile=path; dialog.nMaxFile=sizeof path;
    dialog.lpstrFilter="Disc images\0*.chd\0\0"; dialog.lpstrTitle="Select the complete slamjam96.chd (PlayStation)";
    dialog.Flags=OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&dialog)) load_rom(path);
#else
    message("Drag slamjam96.chd into the window or pass its path on the command line.");
#endif
}
static void update_controllers(void) {
    for (unsigned i=0;i<4;i++) if (controllers[i] && !SDL_GameControllerGetAttached(controllers[i])) {
        SDL_GameControllerClose(controllers[i]); controllers[i]=NULL;
        memset(&previous_pad[i],0,sizeof previous_pad[i]);
        memset(&players[i],0,sizeof players[i]); players[i].spin=-1;
    }
    for (int j=0;j<SDL_NumJoysticks();j++) {
        if (!SDL_IsGameController(j)) continue;
        SDL_JoystickID id=SDL_JoystickGetDeviceInstanceID(j); bool found=false;
        for (unsigned i=0;i<4;i++) if (controllers[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controllers[i]))==id) found=true;
        if (!found) for (unsigned i=0;i<4;i++) if (!controllers[i]) { controllers[i]=SDL_GameControllerOpen(j); break; }
    }
}
/* Slot 0 is the keyboard plus the first controller; slots map to arcade players through player_port. */
static void input_poll(void) {
    const Uint8 *keyboard=SDL_GetKeyboardState(NULL);
    Uint32 now=SDL_GetTicks();
    for (unsigned i=0;i<4;i++) {
        struct player *p=&players[i];
        memcpy(p->was,p->held,sizeof p->held); memset(p->held,0,sizeof p->held);
        if (i==0) for (int a=0;a<A_COUNT;a++) if (keys[a]!=SDL_SCANCODE_UNKNOWN && keyboard[keys[a]]) p->held[a]=true;
        SDL_GameController *g=controllers[i];
        if (g) {
            struct pad_sample sample=read_pad(g);
            unsigned down=sample.down;
            p->suppressed &= down; down &= ~p->suppressed;
            for (int a=0;a<A_COUNT;a++) if (pads[a] & down) p->held[a]=true;
            if (sample.directions & PAD_UP) p->held[A_UP]=true;
            if (sample.directions & PAD_DOWN) p->held[A_DOWN]=true;
            if (sample.directions & PAD_LEFT) p->held[A_LEFT]=true;
            if (sample.directions & PAD_RIGHT) p->held[A_RIGHT]=true;
            bool start=sample.start;
            if (start && !p->start_down) { p->start_since=now; p->start_used=false; }
            if (start && !p->start_used && now-p->start_since>=600) { p->start_used=true; p->start_down=true; open_menu(MENU_PAUSE); return; }
            if (!start && p->start_down && !p->start_used) p->start_pulse=6;
            p->start_down=start;
        }
        if (p->held[A_SPIN] && !p->was[A_SPIN]) p->spin=0;
    }
}
static uint16_t next_frame(struct player *p) {
    uint16_t mask=0;
    for(int a=0;a<A_COUNT;a++)if(p->held[a])mask|=action_bits[a];
    if(p->start_pulse>0){mask|=PAD_START;p->start_pulse--;}
    return mask;
}

/* Small 5x7 bitmap font (ASCII 32-95), with no runtime font dependency. */
static const uint8_t glyphs[64][7]={
 {0,0,0,0,0,0,0},{4,4,4,4,4,0,4},{10,10,10,0,0,0,0},{10,10,31,10,31,10,10},{4,15,20,14,5,30,4},{24,25,2,4,8,19,3},
 {12,18,20,8,21,18,13},{4,4,8,0,0,0,0},{2,4,8,8,8,4,2},{8,4,2,2,2,4,8},{0,4,21,14,21,4,0},{0,4,4,31,4,4,0},
 {0,0,0,0,12,4,8},{0,0,0,31,0,0,0},{0,0,0,0,0,12,12},{1,1,2,4,8,16,16},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{31,2,4,2,1,17,14},{2,6,10,18,31,2,2},
 {31,16,30,1,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,2,12},
 {0,12,12,0,12,12,0},{0,12,12,0,12,4,8},{2,4,8,16,8,4,2},{0,0,31,0,31,0,0},{8,4,2,1,2,4,8},{14,17,1,2,4,0,4},
 {14,17,1,13,21,21,14},
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,14},{17,17,17,31,17,17,17},{31,4,4,4,4,4,31},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,8,8,8,8,8,14},{16,16,8,4,2,1,1},{14,2,2,2,2,2,14},{4,10,17,0,0,0,0},{0,0,0,0,0,0,31}
};
static int text_width(const char *value,int scale) { int n=(int)strlen(value); return n ? n*6*scale-scale : 0; }
static void text_at(const char *value,int x,int y,int scale) {
    for (;*value;value++,x+=6*scale) {
        int c=(unsigned char)*value;
        if (c>='a' && c<='z') c-=32;
        if (c<32 || c>95) c='?';
        for (int row=0;row<7;row++) for (int col=0;col<5;col++) if (glyphs[c-32][row] & (1u << (4-col))) {
            SDL_Rect p={x+col*scale,y+row*scale,scale,scale}; SDL_RenderFillRect(renderer,&p);
        }
    }
}
static void text(const char *value,int y,int scale) { text_at(value,(800-text_width(value,scale))/2,y,scale); }
static void fill(int x,int y,int w,int h,Uint8 r,Uint8 g,Uint8 b,Uint8 a) {
    SDL_SetRenderDrawBlendMode(renderer,a<255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer,r,g,b,a);
    SDL_Rect rect={x,y,w,h}; SDL_RenderFillRect(renderer,&rect);
}
static void ink(bool dark) { if (dark) SDL_SetRenderDrawColor(renderer,28,28,30,255); else SDL_SetRenderDrawColor(renderer,255,255,255,255); }
static void gold(void) { SDL_SetRenderDrawColor(renderer,252,188,41,255); }

/* Full-width light bar at a logical y, drawn in window pixels so it spans letterbox margins too. */
static void band(int y,int h) {
    int width,height; SDL_GetRendererOutputSize(renderer,&width,&height);
    float scale=width/800.0f<height/600.0f ? width/800.0f : height/600.0f;
    int top=(int)((height-600*scale)/2);
    SDL_RenderSetLogicalSize(renderer,0,0);
    fill(0,top+(int)(y*scale),width,(int)(h*scale+0.5f),242,242,242,255);
    SDL_RenderSetLogicalSize(renderer,800,600);
}
static bool notice_active(void) { return notice[0] && !SDL_TICKS_PASSED(SDL_GetTicks(),notice_until); }
static void header(const char *title,const char *back) {
    band(34,62);
    fill(22,44,46,42,28,28,30,255); ink(false); text_at("<",39,55,3);
    ink(true); text_at(back,80,57,3); text(title,48,5);
}
static void footer(const char *hint) { band(550,36); ink(true); text(notice_active() ? notice : hint,561,2); }
static void pad_label(int action,char *out,size_t size) {
    out[0]=0;
    if (action<A_SHOOT) { snprintf(out,size,"D-PAD / STICK"); return; }
    if (action==A_START) { snprintf(out,size,"START (TAP)"); return; }
    for (int b=0;b<P_COUNT;b++) if (pads[action] & (1u << b)) { size_t used=strlen(out); snprintf(out+used,size-used,"%s%s",used ? " " : "",pad_title[b]); }
    if (!out[0]) snprintf(out,size,"-");
}
static void draw_pause(void) {
    header("PAUSED","ESC");
    char labels[ITEM_COUNT][40];
    snprintf(labels[ITEM_RESUME],40,"RESUME"); snprintf(labels[ITEM_CONTROLS],40,"CONTROLS");
    snprintf(labels[ITEM_CRT],40,"CRT FILTER: < %s >",crtMode_name[crtMode]);
    snprintf(labels[ITEM_WIDE],40,"WIDESCREEN: %s",game_wide ? "ON" : "OFF");
    snprintf(labels[ITEM_SIDES],40,"COLOR SIDES: %s",colorSides ? "ON" : "OFF");
    snprintf(labels[ITEM_SCREEN],40,"SCREEN: < %s >",screen_name[screen_mode]);
    snprintf(labels[ITEM_FULLSCREEN],40,"FULLSCREEN: %s",fullscreen ? "ON" : "OFF");
    snprintf(labels[ITEM_RESTART],40,"RESTART ARCADE"); snprintf(labels[ITEM_QUIT],40,"QUIT GAME");
    for (int i=0;i<ITEM_COUNT;i++) {
        int y=MENU_TOP+i*MENU_STEP; bool on=i==selection;
        if (on) fill(200,y,400,MENU_H,252,188,41,255); else fill(200,y,400,MENU_H,0,0,0,150);
        ink(on); text(labels[i],y+12,3);
    }
    footer("ARROWS SELECT - ENTER OK - ESC RESUME - PAD: A OK, B BACK");
}
static void draw_controls(void) {
    header("CONTROLS","ESC");
    gold(); text_at("ACTION",70,114,2); text_at("KEYBOARD",360,114,2); text_at("GAMEPAD",584,114,2);
    for (int a=0;a<A_COUNT;a++) {
        int y=ROW_TOP+a*ROW_STEP; bool on=a==selection;
        if (on) fill(50,y,700,28,252,188,41,255); else fill(50,y,700,28,0,0,0,150);
        ink(on); text_at(action_title[a],70,y+7,2);
        if (capture==a) { text_at(a<A_SHOOT ? "PRESS A KEY..." : "PRESS A KEY OR BUTTON...",360,y+7,2); continue; }
        const char *key=keys[a]==SDL_SCANCODE_UNKNOWN ? "-" : SDL_GetScancodeName(keys[a]);
        char pad[64]; pad_label(a,pad,sizeof pad);
        text_at(key,360,y+7,2); text_at(pad,584,y+7,2);
    }
    footer(capture>=0 ? "PRESS THE NEW KEY OR BUTTON - ESC OR START CANCELS" : "ENTER CHANGE - BACKSPACE DEFAULTS - ESC BACK - PAD: A, Y DEFAULTS");
}
static void draw_launcher(void) {
    fill(0,0,800,600,9,12,19,255);
    gold(); text("SLAMJAM96",140,5);
    SDL_Rect button={190,270,420,70}; SDL_RenderFillRect(renderer,&button);
    SDL_SetRenderDrawColor(renderer,9,12,19,255); text("OPEN DISC IMAGE",292,3);
    SDL_SetRenderDrawColor(renderer,190,199,212,255); text("OR DRAG THE CHD FILE HERE",380,2);
    text("PLAYSTATION DISC IMAGE REQUIRED",420,2);
}
/* FULL stretches to the window, SQUARE keeps square pixels, ARCADE keeps the arcade monitor's pixel shape
   (4:3 until an expanded gameplay patch is implemented). */
static SDL_Rect game_rect(int width,int height) {
    SDL_Rect rect={0,0,width,height};
    if (screen_mode==SCREEN_FULL) return rect;
    double aspect=screen_mode==SCREEN_SQUARE ? (double)ht_width()/ht_height() : ht_aspect_ratio();
    rect.h=(int)(width/aspect+0.5); rect.w=width;
    if (rect.h>height) { rect.h=height; rect.w=(int)(height*aspect+0.5); }
    rect.x=(width-rect.w)/2; rect.y=(height-rect.h)/2;
    return rect;
}
static void draw(void) {
    int width,height; SDL_GetRendererOutputSize(renderer,&width,&height);
    SDL_RenderSetLogicalSize(renderer,0,0);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
    if (running) SDL_SetRenderDrawColor(renderer,colorSides?12:0,colorSides?22:0,colorSides?43:0,255); else SDL_SetRenderDrawColor(renderer,9,12,19,255);
    SDL_RenderClear(renderer);
    if (running && ht_width() && ht_height()) {
        int w=(int)ht_width(), h=(int)ht_height(), tw=0,th=0;
        if (texture) SDL_QueryTexture(texture,NULL,NULL,&tw,&th);
        if (tw!=w || th!=h) { SDL_DestroyTexture(texture); texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,w,h); }
        if (texture) {
            SDL_UpdateTexture(texture,NULL,ht_pixels(),w*4);
            SDL_Rect rect=game_rect(width,height);
            if (!crtMode || !crt_draw(renderer,texture,ht_pixels(),w,h,rect,true)) SDL_RenderCopy(renderer,texture,NULL,&rect);
        }
        if (menu!=MENU_NONE) fill(0,0,width,height,0,0,0,190);
    }
    SDL_RenderSetLogicalSize(renderer,800,600);
    if (!running) draw_launcher();
    else if (menu==MENU_PAUSE) draw_pause();
    else if (menu==MENU_CONTROLS) draw_controls();
    if (menu==MENU_NONE && notice_active()) {
        int w=text_width(notice,2)+40;
        fill((800-w)/2,500,w,32,0,0,0,200); gold(); text(notice,509,2);
    }
}

static void menu_move(int delta) {
    int count=menu==MENU_PAUSE ? ITEM_COUNT : A_COUNT;
    selection=(selection+count+delta)%count;
}
static void menu_adjust(int delta) {
    if (menu!=MENU_PAUSE) return;
    if (selection==ITEM_SCREEN) cycle_screen(delta);
    else if (selection==ITEM_CRT) cycle_crtMode(delta);
    else if (selection==ITEM_WIDE) toggle_widescreen();
    else if (selection==ITEM_SIDES) toggle_colorSides();
    else if (selection==ITEM_FULLSCREEN) toggle_fullscreen();
}
static void menu_accept(bool *quit) {
    if (menu==MENU_CONTROLS) { capture=selection; return; }
    switch (selection) {
    case ITEM_RESUME: close_menu(); break;
    case ITEM_CONTROLS: menu=MENU_CONTROLS; selection=0; break;
    case ITEM_WIDE: toggle_widescreen(); break;
    case ITEM_SIDES: toggle_colorSides(); break;
    case ITEM_CRT: cycle_crtMode(1); break;
    case ITEM_SCREEN: cycle_screen(1); break;
    case ITEM_FULLSCREEN: toggle_fullscreen(); break;
    case ITEM_RESTART: ht_reset(); close_menu(); show_notice("ARCADE RESTARTED"); break;
    case ITEM_QUIT: *quit=true; break;
    }
}
static void menu_back(void) {
    if (menu==MENU_CONTROLS) { menu=MENU_PAUSE; selection=ITEM_CONTROLS; }
    else close_menu();
}
static void key_down(const SDL_KeyboardEvent *event,bool *quit) {
    SDL_Scancode s=event->keysym.scancode;
    if (s==SDL_SCANCODE_F11 || (s==SDL_SCANCODE_RETURN && (event->keysym.mod & KMOD_ALT))) { if (!event->repeat) toggle_fullscreen(); return; }
    if (capture>=0) {
        if (event->repeat) return;
        if (s==SDL_SCANCODE_ESCAPE) { capture=-1; return; }
        if ((s>=SDL_SCANCODE_F1 && s<=SDL_SCANCODE_F12) || s==SDL_SCANCODE_P) return; /* reserved for the menus */
        bind_key(capture,s); capture=-1; save_config(); return;
    }
    if (!running) {
        if (s==SDL_SCANCODE_ESCAPE) *quit=true;
        else if (s==SDL_SCANCODE_O) choose_file();
        return;
    }
    if (s==SDL_SCANCODE_F10) { if (!event->repeat) cycle_screen(1); return; }
    if (s==SDL_SCANCODE_F8) { if (!event->repeat) toggle_widescreen(); return; }
    if (s==SDL_SCANCODE_F9) { if (!event->repeat) toggle_colorSides(); return; }
    if (menu==MENU_NONE) {
        if (event->repeat) return;
        if (s==SDL_SCANCODE_ESCAPE || s==SDL_SCANCODE_P) open_menu(MENU_PAUSE);
        else if (s==SDL_SCANCODE_F1) { open_menu(MENU_PAUSE); menu=MENU_CONTROLS; }
        return;
    }
    if (s==SDL_SCANCODE_UP || s==keys[A_UP]) menu_move(-1);
    else if (s==SDL_SCANCODE_DOWN || s==keys[A_DOWN]) menu_move(1);
    else if (s==SDL_SCANCODE_LEFT || s==keys[A_LEFT]) menu_adjust(-1);
    else if (s==SDL_SCANCODE_RIGHT || s==keys[A_RIGHT]) menu_adjust(1);
    else if (event->repeat) return;
    else if (s==SDL_SCANCODE_RETURN || s==SDL_SCANCODE_KP_ENTER || s==SDL_SCANCODE_SPACE) menu_accept(quit);
    else if (s==SDL_SCANCODE_F1 && menu==MENU_PAUSE) { menu=MENU_CONTROLS; selection=0; }
    else if (s==SDL_SCANCODE_ESCAPE || s==SDL_SCANCODE_P || s==SDL_SCANCODE_F1) menu_back();
    else if ((s==SDL_SCANCODE_BACKSPACE || s==SDL_SCANCODE_DELETE) && menu==MENU_CONTROLS) restore_defaults();
}
static void pad_input(int input) {
    if (capture<A_SHOOT) { show_notice("DIRECTIONS USE THE D-PAD AND LEFT STICK"); capture=-1; return; }
    bind_pad(capture,input); capture=-1; save_config();
}
static void pad_button(Uint8 button,bool *quit) {
    if (capture>=0) {
        static const struct { Uint8 button; int input; } map[]={{SDL_CONTROLLER_BUTTON_A,P_A},{SDL_CONTROLLER_BUTTON_B,P_B},
            {SDL_CONTROLLER_BUTTON_X,P_X},{SDL_CONTROLLER_BUTTON_Y,P_Y},{SDL_CONTROLLER_BUTTON_LEFTSHOULDER,P_LB},
            {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,P_RB},{SDL_CONTROLLER_BUTTON_LEFTSTICK,P_L3},{SDL_CONTROLLER_BUTTON_RIGHTSTICK,P_R3},
            {SDL_CONTROLLER_BUTTON_BACK,P_BACK},{SDL_CONTROLLER_BUTTON_MISC1,P_SHARE},{SDL_CONTROLLER_BUTTON_PADDLE1,P_PADDLE1},
            {SDL_CONTROLLER_BUTTON_PADDLE2,P_PADDLE2},{SDL_CONTROLLER_BUTTON_PADDLE3,P_PADDLE3},{SDL_CONTROLLER_BUTTON_PADDLE4,P_PADDLE4}};
        if (button==SDL_CONTROLLER_BUTTON_START || button==SDL_CONTROLLER_BUTTON_GUIDE) { capture=-1; return; }
        for (size_t i=0;i<sizeof map/sizeof map[0];i++) if (map[i].button==button) pad_input(map[i].input);
        return;
    }
    if (!running) return;
    if (menu==MENU_NONE) { if (button==SDL_CONTROLLER_BUTTON_GUIDE) open_menu(MENU_PAUSE); return; }
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: menu_move(-1); break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: menu_move(1); break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: menu_adjust(-1); break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: menu_adjust(1); break;
    case SDL_CONTROLLER_BUTTON_A: menu_accept(quit); break;
    case SDL_CONTROLLER_BUTTON_B: menu_back(); break;
    case SDL_CONTROLLER_BUTTON_Y: if (menu==MENU_CONTROLS) restore_defaults(); break;
    case SDL_CONTROLLER_BUTTON_START: case SDL_CONTROLLER_BUTTON_GUIDE: close_menu(); break;
    default: break;
    }
}
static void poll_pad_menus(bool *quit) {
    for (unsigned i=0;i<4;i++) {
        struct pad_sample sample=read_pad(controllers[i]), before=previous_pad[i];
        previous_pad[i]=sample;
        unsigned edges=sample.down & ~before.down;
        uint16_t directions=sample.directions & ~before.directions;
        if (capture>=0) {
            if ((sample.start && !before.start) || (sample.home && !before.home)) capture=-1;
            else for (int b=0;b<P_COUNT;b++) if (edges & (1u<<b)) { pad_input(b); break; }
            continue;
        }
        if (sample.home && !before.home) { pad_button(SDL_CONTROLLER_BUTTON_GUIDE,quit); continue; }
        if (menu==MENU_NONE) continue;
        /* Handle one menu action per controller sample: opening a remap must
           not immediately bind another button held in that same sample. */
        if (sample.start && !before.start) pad_button(SDL_CONTROLLER_BUTTON_START,quit);
        else if (edges & (1u<<P_A)) pad_button(SDL_CONTROLLER_BUTTON_A,quit);
        else if (edges & (1u<<P_B)) pad_button(SDL_CONTROLLER_BUTTON_B,quit);
        else if (edges & (1u<<P_Y)) pad_button(SDL_CONTROLLER_BUTTON_Y,quit);
        else if (directions & PAD_UP) pad_button(SDL_CONTROLLER_BUTTON_DPAD_UP,quit);
        else if (directions & PAD_DOWN) pad_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN,quit);
        else if (directions & PAD_LEFT) pad_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT,quit);
        else if (directions & PAD_RIGHT) pad_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT,quit);
    }
}
static void mouse_down(const SDL_MouseButtonEvent *event,bool *quit) {
    float x,y; SDL_RenderWindowToLogical(renderer,event->x,event->y,&x,&y);
    if (!running) { if (x>=190 && x<=610 && y>=270 && y<=340) choose_file(); return; }
    if (menu==MENU_PAUSE && x>=200 && x<=600 && y>=MENU_TOP) {
        int item=(int)((y-MENU_TOP)/MENU_STEP);
        if (item<ITEM_COUNT && y-MENU_TOP-item*MENU_STEP<MENU_H) { selection=item; menu_accept(quit); }
    } else if (menu==MENU_CONTROLS && x>=50 && x<=750 && y>=ROW_TOP) {
        int row=(int)((y-ROW_TOP)/ROW_STEP);
        if (row<A_COUNT) { selection=row; capture=row; }
    } else if (menu!=MENU_NONE && x>=22 && x<=200 && y>=34 && y<=96) menu_back();
}
static void handle_event(const SDL_Event *event,bool *quit) {
    switch (event->type) {
    case SDL_QUIT: *quit=true; break;
    case SDL_DROPFILE: load_rom(event->drop.file); SDL_free(event->drop.file); break;
    case SDL_WINDOWEVENT: if (event->window.event==SDL_WINDOWEVENT_FOCUS_LOST && running && menu==MENU_NONE) open_menu(MENU_PAUSE); break;
    case SDL_CONTROLLERDEVICEADDED: {
        const char *name=SDL_GameControllerNameForIndex(event->cdevice.which);
        char text[64]; snprintf(text,sizeof text,"%.36s CONNECTED",name ? name : "CONTROLLER"); show_notice(text);
        break;
    }
    case SDL_CONTROLLERDEVICEREMOVED: show_notice("CONTROLLER DISCONNECTED"); if (running && menu==MENU_NONE) open_menu(MENU_PAUSE); break;
    case SDL_MOUSEBUTTONDOWN: if (event->button.button==SDL_BUTTON_LEFT) mouse_down(&event->button,quit); break;
    case SDL_KEYDOWN: key_down(&event->key,quit); break;
    default: break;
    }
}
/* --snapshot DIR [ROM]: renders the main screens to BMP files without showing a window (UI check). */
static void snapshot(const char *directory,const char *name) {
    draw(); SDL_RenderSetLogicalSize(renderer,0,0);
    int w,h; SDL_GetRendererOutputSize(renderer,&w,&h);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
    if (surface && !SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_ARGB8888,surface->pixels,surface->pitch)) {
        char path[4096]; snprintf(path,sizeof path,"%s/%s.bmp",directory,name); SDL_SaveBMP(surface,path); printf("Saved %s\n",path);
    }
    SDL_FreeSurface(surface); SDL_RenderPresent(renderer);
}
static void run_snapshots(const char *directory) {
    if (!running) { snapshot(directory,"launcher"); return; }
    for (int i=0;i<3000;i++) ht_run();
    const char *modes[3]={"game-full","game-square","game-arcade"};
    for (int m=0;m<3;m++) { screen_mode=m; snapshot(directory,modes[m]); }
    screen_mode=SCREEN_ARCADE; menu=MENU_PAUSE; selection=ITEM_SIDES; snapshot(directory,"menu-pause");
    menu=MENU_CONTROLS; selection=A_TURBO; snapshot(directory,"menu-controls");
    capture=A_SHOOT; selection=A_SHOOT; snapshot(directory,"menu-capture");
    menu=MENU_NONE; capture=-1; show_notice("PRESS 5 FOR A CREDIT, THEN 1 TO START"); snapshot(directory,"game-notice");
    running=false; snapshot(directory,"launcher"); running=true;
    /* the attract demo reaches the court a little later */
    colorSides=true;  notice[0]=0;
    for (int i=0;i<4000;i++) ht_run();
    const char *wide_modes[3]={"wide-full","wide-square","wide-arcade"};
    for (int m=0;m<3;m++) { screen_mode=m; snapshot(directory,wide_modes[m]); }
}

int main(int argc,char **argv) {
    SDL_SetMainReady(); SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengl");
#ifdef _WIN32
    /* GUI build: no console window, but --audit/--smoke-test still print when started from a terminal. */
    if (AttachConsole(ATTACH_PARENT_PROCESS)) { freopen("CONOUT$","w",stdout); freopen("CONOUT$","w",stderr); }
#endif
    bool smoke=argc>1 && !strcmp(argv[1],"--smoke-test");
    bool snapshots=argc>2 && !strcmp(argv[1],"--snapshot");
    if (argc>2 && !strcmp(argv[1],"--audit")) {
        char report[8192]; bool ok=ht_validate_rom(argv[2],report,sizeof report);
        printf("%s\n",ok ? "PlayStation ROM set complete and valid." : report); return ok ? 0 : 2;
    }
#ifdef _WIN32
    /* Use the native XInput backend for Xbox pads instead of opening the
       same controller through the optional HIDAPI Xbox driver. */
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX,"0");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT,"0");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER)) { fprintf(stderr,"%s\n",SDL_GetError()); return 1; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    bool hidden=smoke || snapshots;
    window=SDL_CreateWindow("Slam’n’Jam 96 - Open or drop slamjam96.chd",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                            snapshots ? 1280 : 960,720,SDL_WINDOW_RESIZABLE | (hidden ? SDL_WINDOW_HIDDEN : 0));
    renderer=window ? SDL_CreateRenderer(window,-1,hidden ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED) : NULL;
    if (!renderer && window) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if (!window || !renderer) { fprintf(stderr,"%s\n",SDL_GetError()); SDL_Quit(); return 1; }
    if (snapshots) {
        save_dir=malloc(strlen(argv[2])+2);
        if (save_dir) sprintf(save_dir,"%s/",argv[2]);
    } else save_dir=SDL_GetPrefPath("NativePorts","slamjam96");
    if (!save_dir) { message("Could not create the data folder."); SDL_Quit(); return 1; }
    load_config(); ht_set_widescreen(game_wide); reset_players(false);
    
    
    if (fullscreen && !hidden) SDL_SetWindowFullscreen(window,SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (snapshots) {
        if (argc>3) load_rom(argv[3]);
        run_snapshots(argv[2]);
    } else if (argc>1 && !smoke) load_rom(argv[1]);
    else if (!smoke) {
        char *path=malloc(strlen(save_dir)+32);
        if (path) { sprintf(path,"%sslamjam96.chd",save_dir); FILE *f=fopen(path,"rb"); if (f) { fclose(f); load_rom(path); } free(path); }
    }
    Uint64 frequency=SDL_GetPerformanceFrequency(), previous=SDL_GetPerformanceCounter(), last_save=previous;
    double accumulated=0; bool quit=snapshots; unsigned rendered=0;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) handle_event(&event,&quit);
        Uint64 now=SDL_GetPerformanceCounter(); double elapsed=(double)(now-previous)/frequency; previous=now;
        update_controllers();
        poll_pad_menus(&quit);
        if (running && menu==MENU_NONE) input_poll();
        if (running && menu==MENU_NONE) {
            accumulated+=elapsed<0.1 ? elapsed : 0.1;
            double interval=1.0/ht_fps(); unsigned steps=0;
            while (accumulated>=interval && steps<4) {
                for (unsigned i=0;i<4;i++) ht_set_buttons(player_port[i],next_frame(&players[i]));
                ht_run(); accumulated-=interval; steps++;
            }
            if (steps==4) accumulated=0;
            if ((double)(now-last_save)/frequency>=5) { ht_save_data(); last_save=now; }
        } else accumulated=0;
        draw(); SDL_RenderPresent(renderer);
        if (smoke && ++rendered==3) quit=true;
        SDL_Delay(2);
    }
    if (audio_device) SDL_CloseAudioDevice(audio_device);
    ht_stop(); for (unsigned i=0;i<4;i++) if (controllers[i]) SDL_GameControllerClose(controllers[i]);
    SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    if (snapshots) free(save_dir); else SDL_free(save_dir);
    SDL_Quit();
    if (smoke) puts("Desktop launcher smoke test passed."); return 0;
}
