#include <tamtypes.h>
#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <libpad.h>
#include <math.h>

#define MAX_NPC 7
static GSGLOBAL *gs;
static char padBuf[256] __attribute__((aligned(64)));
static padButtonStatus pad;
static float playerX=0.0f, speedKmh=0.0f, roadScroll=0.0f, throttle=0.0f;
static int headlights=0, weather=0, nightMode=0;
static u16 previousButtons=0xFFFF;

typedef struct { float x,z,v; int color,style; } TrafficCar;
static TrafficCar traffic[MAX_NPC]={
 {-1.25f,18,.48f,0,0},{1.25f,29,.65f,1,1},{0,43,.42f,2,0},
 {-2.5f,57,.72f,3,1},{2.5f,71,.55f,4,0},{-1.25f,88,.82f,5,1},{1.25f,105,.38f,6,0}
};

static u64 col(u8 r,u8 g,u8 b){ return GS_SETREG_RGBAQ(r,g,b,0x80,0); }
static void quad(float a,float b,float c,float d,float e,float f,float g,float h,u64 color){
 gsKit_prim_quad(gs,a,b,c,d,e,f,g,h,2,color);
}
static int project(float x,float y,float z,float *sx,float *sy){
 float focal=265.0f, horizon=(float)gs->Height*.39f;
 if(z<1.0f)return 0;
 *sx=(float)gs->Width*.5f+(x-playerX*.20f)*focal/z;
 *sy=horizon+(1.15f-y)*focal/z;
 return 1;
}
static void face(float ax,float ay,float az,float bx,float by,float bz,
 float cx,float cy,float cz,float dx,float dy,float dz,u64 color){
 float x1,y1,x2,y2,x3,y3,x4,y4;
 if(!project(ax,ay,az,&x1,&y1)||!project(bx,by,bz,&x2,&y2)||
 !project(cx,cy,cz,&x3,&y3)||!project(dx,dy,dz,&x4,&y4))return;
 quad(x1,y1,x2,y2,x3,y3,x4,y4,color);
}
static u64 trafficColor(int c){
 static const u8 colors[7][3]={{210,210,205},{35,85,160},{175,35,30},{30,30,32},{210,175,35},{115,115,120},{20,125,85}};
 return col(colors[c%7][0],colors[c%7][1],colors[c%7][2]);
}
static void drawCar(float x,float z,float len,float width,u64 body,int player,int tuned){
 float half=width*.5f, rear=z, front=z+len, y0=.12f, y1=player?.78f:.68f;
 float cb=z+len*.30f, cf=z+len*.73f, roof=y1+.52f;
 u64 dark=col(22,24,27),glass=col(45,78,100),lamp=col(245,225,160);
 face(x-half-.08f,y0,rear+len*.18f,x-half-.08f,y0+.30f,rear+len*.18f,x-half-.08f,y0+.30f,front-len*.18f,x-half-.08f,y0,front-len*.18f,dark);
 face(x+half+.08f,y0,rear+len*.18f,x+half+.08f,y0+.30f,rear+len*.18f,x+half+.08f,y0+.30f,front-len*.18f,x+half+.08f,y0,front-len*.18f,dark);
 face(x-half,y0,rear,x-half,y1,rear,x-half,y1,front,x-half,y0,front,body);
 face(x+half,y0,rear,x+half,y1,rear,x+half,y1,front,x+half,y0,front,body);
 face(x-half,y1,front,x+half,y1,front,x+half,y1,front-len*.28f,x-half,y1,front-len*.28f,body);
 face(x-half*.76f,y1,cb,x+half*.76f,y1,cb,x+half*.58f,roof,cf,x-half*.58f,roof,cf,body);
 face(x-half*.58f,roof,cf,x+half*.58f,roof,cf,x+half*.48f,roof,cb,x-half*.48f,roof,cb,glass);
 face(x-half*.76f,y1,cb,x-half*.58f,roof,cb,x-half*.58f,roof,cf,x-half*.76f,y1,cf,glass);
 face(x+half*.76f,y1,cb,x+half*.58f,roof,cb,x+half*.58f,roof,cf,x+half*.76f,y1,cf,glass);
 face(x-half*.82f,y0+.24f,front+.01f,x-half*.82f,y0+.43f,front+.01f,x-half*.35f,y0+.43f,front+.01f,x-half*.35f,y0+.24f,front+.01f,headlights||player?lamp:col(95,85,60));
 face(x+half*.35f,y0+.24f,front+.01f,x+half*.35f,y0+.43f,front+.01f,x+half*.82f,y0+.43f,front+.01f,x+half*.82f,y0+.24f,front+.01f,headlights||player?lamp:col(95,85,60));
 if(tuned)face(x-half*.9f,y1+.04f,rear+.10f,x+half*.9f,y1+.04f,rear+.10f,x+half*.9f,y1+.12f,rear+.10f,x-half*.9f,y1+.12f,rear+.10f,col(25,25,28));
}
static void drawRoad(void){
 float horizon=(float)gs->Height*.39f;
 gsKit_prim_sprite(gs,0,0,(float)gs->Width,horizon,1,nightMode?col(8,15,30):col(105,170,205));
 gsKit_prim_sprite(gs,0,horizon,(float)gs->Width,(float)gs->Height,1,nightMode?col(18,28,20):col(55,112,55));
 face(-8,0,3,8,0,3,8,0,150,-8,0,150,col(75,77,80));
 face(-8.35f,0,3,-8,0,3,-8,0,150,-8.35f,0,150,col(220,210,180));
 face(8,0,3,8.35f,0,3,8.35f,0,150,8,0,150,col(220,210,180));
 for(int i=0;i<18;i++){
  float z=5.0f+(float)i*7.0f+roadScroll;
  while(z>130)z-=126;
  for(int lane=-1;lane<=1;lane++){
   float lx=(float)lane*2.0f;
   face(lx-.035f,.015f,z,lx+.035f,.015f,z,lx+.035f,.015f,z+2.5f,lx-.035f,.015f,z+2.5f,col(225,225,205));
  }
 }
}
static void drawHud(void){
 float w=(float)gs->Width,h=(float)gs->Height;
 gsKit_prim_sprite(gs,12,h-32,w*.34f,h-13,1,col(18,20,24));
 gsKit_prim_sprite(gs,15,h-29,15+(w*.31f)*(speedKmh/350.0f),h-16,1,speedKmh>250?col(220,65,35):col(235,185,45));
 gsKit_prim_sprite(gs,w-105,12,w-14,35,1,col(18,20,24));
 gsKit_prim_sprite(gs,w-99,18,w-20,29,1,nightMode?col(55,105,180):col(60,155,90));
 if(headlights)gsKit_prim_sprite(gs,w-125,42,w-14,49,1,col(245,225,130));
 if(weather)gsKit_prim_sprite(gs,14,14,60,21,1,col(95,145,205));
}
static void initPad(void){ padInit(0); padPortOpen(0,0,padBuf); }
static void controls(void){
 if(!padRead(0,0,&pad))return;
 u16 pressed=(u16)((~pad.btns)&previousButtons);
 previousButtons=pad.btns;
 if(!(pad.btns&PAD_LEFT))playerX-=.055f;
 if(!(pad.btns&PAD_RIGHT))playerX+=.055f;
 if(playerX< -3.1f)playerX=-3.1f;
 if(playerX>3.1f)playerX=3.1f;
 if(!(pad.btns&PAD_CROSS))throttle+=.006f;else throttle-=.003f;
 if(!(pad.btns&PAD_SQUARE))throttle-=.012f;
 if(throttle<0)throttle=0;if(throttle>1)throttle=1;
 if(pressed&PAD_R1)headlights=1;
 if(pressed&PAD_L1)headlights=0;
 if(pressed&PAD_TRIANGLE)nightMode=!nightMode;
 if(pressed&PAD_CIRCLE)weather=(weather+1)%3;
}
int main(int argc,char *argv[]){
 (void)argc;(void)argv;
 dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
 dmaKit_chan_init(DMA_CHANNEL_GIF);
 gs=gsKit_init_global();
 gs->Mode=GS_MODE_NTSC;gs->Interlace=GS_INTERLACED;gs->Field=GS_FIELD;
 gs->Width=640;gs->Height=448;gs->PSM=GS_PSM_CT16;gs->PSMZ=GS_PSMZ_16;
 gsKit_init_screen(gs);gsKit_mode_switch(gs,GS_PERSISTENT);gsKit_set_test(gs,GS_ZTEST_OFF);
 initPad();
 for(;;){
  controls();
  speedKmh+=(throttle*2.2f)-.22f;if(speedKmh<0)speedKmh=0;if(speedKmh>350)speedKmh=350;
  roadScroll+=speedKmh*.0009f;if(roadScroll>7)roadScroll-=7;
  for(int i=0;i<MAX_NPC;i++){
   traffic[i].z-=.025f+speedKmh*.0014f-traffic[i].v*.025f;
   if(traffic[i].z<7){traffic[i].z=105+(float)(i*5);traffic[i].x=(float)((i%5)-2)*1.25f;}
   if(traffic[i].style&&(i%2)==0)traffic[i].x+=(float)sin((double)(roadScroll+i))*.002f;
  }
  gsKit_clear(gs,nightMode?col(7,12,22):col(90,145,175));
  drawRoad();
  for(int i=0;i<MAX_NPC;i++)drawCar(traffic[i].x,traffic[i].z,3.2f,1.45f,trafficColor(traffic[i].color),0,traffic[i].style);
  drawCar(playerX,5.8f,4.0f,1.85f,col(190,35,30),1,1);
  drawHud();
  gsKit_queue_exec(gs);gsKit_sync_flip(gs);gsKit_queue_reset(gs->Per_Queue);
 }
 return 0;
}
