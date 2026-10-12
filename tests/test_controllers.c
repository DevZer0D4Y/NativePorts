#include <stdbool.h>
#ifdef PORT_THPS2
void ht_run_frame(bool present);
#define PORT_ACTION_END A_START
#else
#define PORT_ACTION_END A_COIN
#endif
#define main native_frontend_main
#include PORT_MAIN
#undef main
#ifdef PORT_THPS2
/* This input test links the small arcade bridge; it never runs a game. */
void ht_run_frame(bool present) { ht_run(); }
#endif
#include <assert.h>
int main(void){
 SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
 assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)==0);
 window=SDL_CreateWindow("Input check",0,0,64,64,SDL_WINDOW_HIDDEN);assert(window);
 memcpy(keys,default_keys,sizeof keys);memcpy(pads,default_pads,sizeof pads);reset_players(false);
 SDL_Joystick *virtual[PORT_PLAYERS];
 for(unsigned i=0;i<PORT_PLAYERS;i++){
  SDL_VirtualJoystickDesc d={0};d.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;d.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;
  d.naxes=SDL_CONTROLLER_AXIS_MAX;d.nbuttons=SDL_CONTROLLER_BUTTON_MAX;d.vendor_id=0x045e;d.product_id=0x02ea;d.name="Virtual Xbox";
  d.button_mask=(1u<<SDL_CONTROLLER_BUTTON_MAX)-1;d.axis_mask=(1u<<SDL_CONTROLLER_AXIS_MAX)-1;
  int id=SDL_JoystickAttachVirtualEx(&d);assert(id>=0);virtual[i]=SDL_JoystickOpen(id);assert(virtual[i]);
  update_controllers();assert(controllers[i]);assert(player_port[i]==PORT_PLAYER_ORDER[i]);
  SDL_JoystickSetVirtualAxis(virtual[i],SDL_CONTROLLER_AXIS_TRIGGERLEFT,-32768);SDL_JoystickSetVirtualAxis(virtual[i],SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
 }
 const int button[P_COUNT]={SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_X,SDL_CONTROLLER_BUTTON_Y,SDL_CONTROLLER_BUTTON_LEFTSHOULDER,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,-1,-1,SDL_CONTROLLER_BUTTON_LEFTSTICK,SDL_CONTROLLER_BUTTON_RIGHTSTICK,SDL_CONTROLLER_BUTTON_BACK};
 for(unsigned i=0;i<PORT_PLAYERS;i++)for(int a=A_SHOOT;a<PORT_ACTION_END;a++){
  unsigned binding=default_pads[a];int b=0;while(!(binding&(1u<<b)))b++;assert(b<P_COUNT);
  if(b==P_LT||b==P_RT)SDL_JoystickSetVirtualAxis(virtual[i],b==P_LT?SDL_CONTROLLER_AXIS_TRIGGERLEFT:SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);
  else SDL_JoystickSetVirtualButton(virtual[i],button[b],1);
  SDL_JoystickSetVirtualAxis(virtual[i],SDL_CONTROLLER_AXIS_LEFTX,-32768);
  SDL_PumpEvents();input_poll();uint16_t mask=next_frame(&players[i]);
  if (PORT_TIMED_COMBOS && (a==A_DUNK || a==A_SHOVE)) mask=next_frame(&players[i]);assert((mask&(action_bits[a]|PAD_LEFT))==(action_bits[a]|PAD_LEFT));
  for(unsigned j=0;j<PORT_PLAYERS;j++)if(j!=i)assert(next_frame(&players[j])==0);
  if(b==P_LT||b==P_RT)SDL_JoystickSetVirtualAxis(virtual[i],b==P_LT?SDL_CONTROLLER_AXIS_TRIGGERLEFT:SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);
  else SDL_JoystickSetVirtualButton(virtual[i],button[b],0);
  SDL_JoystickSetVirtualAxis(virtual[i],SDL_CONTROLLER_AXIS_LEFTX,0);SDL_PumpEvents();reset_players(false);
 }
 for(unsigned i=0;i<PORT_PLAYERS;i++){
  SDL_JoystickSetVirtualButton(virtual[i],SDL_CONTROLLER_BUTTON_START,1);SDL_PumpEvents();input_poll();
  SDL_JoystickSetVirtualButton(virtual[i],SDL_CONTROLLER_BUTTON_START,0);SDL_PumpEvents();input_poll();assert(next_frame(&players[i])&PAD_START);reset_players(false);
 }
 assert(SDL_JoystickDetachVirtual(0)==0);SDL_PumpEvents();update_controllers();assert(!controllers[0]);
 for(unsigned i=1;i<PORT_PLAYERS;i++)assert(controllers[i]);
 if (PORT_TIMED_COMBOS) {
  struct player p={0}; p.spin=-1; p.held[A_DUNK]=true;
  assert(next_frame(&p)==PAD_TURBO); assert(next_frame(&p)==(PAD_TURBO|PAD_SHOOT));
  p.held[A_DUNK]=false; assert(next_frame(&p)==0);
  p.held[A_SHOVE]=true; assert(next_frame(&p)==PAD_TURBO); assert(next_frame(&p)==(PAD_TURBO|PAD_PASS));
  memset(&p,0,sizeof p); p.spin=0;
  for(int frame=0;frame<10;frame++) {
   bool down=frame==2 || frame==3 || frame==4 || frame==8 || frame==9;
   assert(next_frame(&p)==(down?PAD_TURBO:0));
  }
  assert(next_frame(&p)==0);
 }
 for(unsigned i=0;i<PORT_PLAYERS;i++){if(controllers[i])SDL_GameControllerClose(controllers[i]);SDL_JoystickClose(virtual[i]);}
 SDL_DestroyWindow(window);SDL_Quit();puts("Virtual pads: all six actions, directions, isolated player slots, Start and disconnect passed.");return 0;
}
