#include <tamtypes.h>
#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <libpad.h>
#include <math.h>
#include "vehicles.h"

#define MAX_NPC 7
static GSGLOBAL *gs;
static char padBuf[256] __attribute__((aligned(64)));
static struct padButtonStatus pad;
static float playerX=0.0f, speedKmh=0.0f, roadScroll=0.0f, throttle=0.0f;
static int headlights=0, weather=0, nightMode=0, brakeActive=0;
static u16 previousButtons=0xFFFF;
static int currentVehicle=0, engineStage[VEHICLE_COUNT]={0}, paintIndex=0;
static int hornCount=0, hornWindow=0, raceChallenge=0, raceTarget=-1;

typedef struct { float x,z,v; int color,style,model; } TrafficCar;
static TrafficCar traffic[MAX_NPC]={
 {-1.25f,18,.48f,0,0,0},{1.25f,29,.65f,1,1,1},{0,43,.42f,2,0,2},
 {-2.5f,57,.72f,3,1,3},{2.5f,71,.55f,4,0,4},{-1.25f,88,.82f,5,1,5},{1.25f,105,.38f,6,0,6}
};

static u64 col(u8 r,u8 g,u8 b){ return GS_SETREG_RGBAQ(r,g,b,0x80,0); }
static void quad(float a,float b,float c,float d,float e,float f,float g,float h,u64 color){
 gsKit_prim_quad(gs,a,b,c,d,e,f,g,h,2,color);
}
static int project(float x,float y,float z,float *sx,float *sy){
 /* Chase camera: elevated, slightly downward-looking, centered behind the car. */
 float focal=300.0f, horizon=(float)gs->Height*.345f;
 if(z<1.0f)return 0;
 *sx=(float)gs->Width*.5f+(x-playerX*.13f)*focal/z;
 *sy=horizon+(1.48f-y)*focal/z;
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
 /* Multi-part low-poly sedan/pickup silhouette, viewed from behind. */
 float half=width*.5f, rear=z, front=z+len;
 float floorY=.10f, beltY=.73f, hoodY=.47f;
 float cabRear=z+len*.28f, cabFront=z+len*.69f;
 float roofY=1.28f, wheelR=.25f;
 u64 tire=col(12,13,15), rim=col(105,110,115), glass=col(35,68,88);
 u64 trim=col(28,30,33), chrome=col(155,160,162);
 u64 red=col(220,35,28), white=col(245,225,170);

 /* Main lower body, rear panel and side panels. */
 face(x-half,floorY,rear,x+half,floorY,rear,x+half,beltY,rear,x-half,beltY,rear,body);
 face(x-half,floorY,rear,x-half,beltY,rear,x-half,beltY,front,x-half,floorY,front,body);
 face(x+half,floorY,rear,x+half,beltY,rear,x+half,beltY,front,x+half,floorY,front,body);
 face(x-half,floorY,front,x+half,floorY,front,x+half,hoodY,front,x-half,hoodY,front,body);
 face(x-half,hoodY,front,x+half,hoodY,front,x+half,beltY,front-len*.20f,x-half,beltY,front-len*.20f,body);

 /* Rear window, roof, windshield and side glass. */
 face(x-half*.78f,beltY,cabRear,x+half*.78f,beltY,cabRear,
      x+half*.58f,roofY,cabFront,x-half*.58f,roofY,cabFront,glass);
 face(x-half*.58f,roofY,cabFront,x+half*.58f,roofY,cabFront,
      x+half*.52f,roofY,cabRear,x-half*.52f,roofY,cabRear,body);
 face(x-half*.58f,roofY,cabRear,x+half*.52f,roofY,cabRear,
      x+half*.52f,roofY,cabFront,x-half*.58f,roofY,cabFront,glass);
 face(x-half*.80f,beltY,cabRear,x-half*.58f,roofY,cabRear,
      x-half*.58f,roofY,cabFront,x-half*.80f,beltY,cabFront,glass);
 face(x+half*.80f,beltY,cabRear,x+half*.58f,roofY,cabRear,
      x+half*.58f,roofY,cabFront,x+half*.80f,beltY,cabFront,glass);

 /* Hood and trunk/deck planes make the body shape readable. */
 face(x-half*.92f,beltY,front-len*.20f,x+half*.92f,beltY,front-len*.20f,
      x+half*.88f,hoodY,front,x-half*.88f,hoodY,front,body);
 face(x-half*.88f,hoodY,front,x+half*.88f,hoodY,front,
      x+half*.78f,hoodY+.06f,front-len*.10f,x-half*.78f,hoodY+.06f,front-len*.10f,body);

 /* Rear bumper, trunk line, license plate, tail lamps and front lamps. */
 face(x-half*.94f,floorY+.08f,rear-.035f,x+half*.94f,floorY+.08f,rear-.035f,
      x+half*.94f,floorY+.22f,rear-.035f,x-half*.94f,floorY+.22f,rear-.035f,chrome);
 face(x-half*.88f,beltY*.70f,rear-.012f,x+half*.88f,beltY*.70f,rear-.012f,
      x+half*.88f,beltY*.70f,rear+.025f,x-half*.88f,beltY*.70f,rear+.025f,trim);
 face(x-half*.25f,floorY+.20f,rear-.045f,x+half*.25f,floorY+.20f,rear-.045f,
      x+half*.25f,floorY+.34f,rear-.045f,x-half*.25f,floorY+.34f,rear-.045f,col(185,190,190));
 face(x-half*.91f,floorY+.30f,rear-.05f,x-half*.52f,floorY+.30f,rear-.05f,
      x-half*.52f,floorY+.58f,rear-.05f,x-half*.91f,floorY+.58f,rear-.05f,red);
 face(x+half*.52f,floorY+.30f,rear-.05f,x+half*.91f,floorY+.30f,rear-.05f,
      x+half*.91f,floorY+.58f,rear-.05f,x+half*.52f,floorY+.58f,rear-.05f,red);
 face(x-half*.30f,floorY+.04f,front+.025f,x+half*.30f,floorY+.04f,front+.025f,
      x+half*.30f,floorY+.15f,front+.025f,x-half*.30f,floorY+.15f,front+.025f,trim);
 face(x-half*.88f,floorY+.24f,front+.035f,x-half*.42f,floorY+.24f,front+.035f,
      x-half*.42f,floorY+.43f,front+.035f,x-half*.88f,floorY+.43f,front+.035f,white);
 face(x+half*.42f,floorY+.24f,front+.035f,x+half*.88f,floorY+.24f,front+.035f,
      x+half*.88f,floorY+.43f,front+.035f,x+half*.42f,floorY+.43f,front+.035f,white);

 /* Four chunky tires and metallic hubs, visibly outside the body. */
 for(int side=-1;side<=1;side+=2){
  float wx=x+(float)side*(half+.035f);
  for(int axle=0;axle<2;axle++){
   float wz=axle?front-len*.22f:rear+len*.23f;
   face(wx-.12f,floorY-.01f,wz-wheelR,wx+.12f,floorY-.01f,wz-wheelR,
        wx+.12f,floorY+wheelR,wz-wheelR,wx-.12f,floorY+wheelR,wz-wheelR,tire);
   face(wx-.075f,floorY+.055f,wz-wheelR-.012f,wx+.075f,floorY+.055f,wz-wheelR-.012f,
        wx+.075f,floorY+.19f,wz-wheelR-.012f,wx-.075f,floorY+.19f,wz-wheelR-.012f,rim);
  }
 }
 /* Side mirrors and door seams. */
 for(int side=-1;side<=1;side+=2){
  float sx=x+(float)side*half*1.04f;
  face(sx-.055f,beltY-.03f,cabFront-.08f,sx+.055f,beltY-.03f,cabFront-.08f,
       sx+.055f,beltY+.07f,cabFront-.08f,sx-.055f,beltY+.07f,cabFront-.08f,trim);
  face(sx,beltY-.02f,cabRear+.03f,sx,beltY-.02f,cabFront-.04f,
       sx,beltY+.01f,cabFront-.04f,sx,beltY+.01f,cabRear+.03f,trim);
 }
 if(tuned){
  face(x-half*.92f,beltY+.04f,rear+.08f,x+half*.92f,beltY+.04f,rear+.08f,
       x+half*.92f,beltY+.12f,rear+.08f,x-half*.92f,beltY+.12f,rear+.08f,trim);
 }
 (void)player;
}
static void drawRoad(void){
 float horizon=(float)gs->Height*.39f;
 u64 sky=nightMode?col(8,15,30):(weather==2?col(145,155,160):(weather==1?col(80,105,125):col(105,170,205)));
 gsKit_prim_sprite(gs,0,0,(float)gs->Width,horizon,1,sky);
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
 if(weather==1){
  for(int r=0;r<24;r++){
   float rx=(float)((r*37+(int)(roadScroll*90))%620);
   float ry=(float)((r*53+(int)(roadScroll*170))%430);
   gsKit_prim_sprite(gs,rx,ry,rx+1.0f,ry+9.0f,1,col(170,195,215));
  }
 }
 if(weather==2)gsKit_prim_sprite(gs,0,horizon+55,(float)gs->Width,horizon+125,1,col(190,195,195));
}
static void drawHud(void){
 float w=(float)gs->Width,h=(float)gs->Height;
 gsKit_prim_sprite(gs,12,h-32,w*.34f,h-13,1,col(18,20,24));
 gsKit_prim_sprite(gs,15,h-29,15+(w*.31f)*(speedKmh/350.0f),h-16,1,speedKmh>250?col(220,65,35):col(235,185,45));
 gsKit_prim_sprite(gs,w-105,12,w-14,35,1,col(18,20,24));
 gsKit_prim_sprite(gs,w-99,18,w-20,29,1,nightMode?col(55,105,180):col(60,155,90));
 if(headlights)gsKit_prim_sprite(gs,w-125,42,w-14,49,1,col(245,225,130));
 if(weather)gsKit_prim_sprite(gs,14,14,60,21,1,col(95,145,205));
 if(raceChallenge)gsKit_prim_sprite(gs,w-125,55,w-14,64,1,raceChallenge==1?col(45,190,90):col(190,65,45));
}
static void initPad(void){ padInit(0); padPortOpen(0,0,padBuf); }
static void controls(void){
 if(!padRead(0,0,&pad)){ brakeActive=0; return; }
 brakeActive=(!(pad.btns&PAD_SQUARE));
 u16 pressed=(u16)((~pad.btns)&previousButtons);
 previousButtons=pad.btns;
 if(!(pad.btns&PAD_LEFT))playerX-=.055f;
 if(!(pad.btns&PAD_RIGHT))playerX+=.055f;
 if(playerX< -3.1f)playerX=-3.1f;
 if(playerX>3.1f)playerX=3.1f;
 if(!(pad.btns&PAD_CROSS))throttle+=.006f;else throttle-=.003f;
 if(!(pad.btns&PAD_SQUARE))throttle-=.012f;
 if(throttle<0)throttle=0;if(throttle>1)throttle=1;
 if(pressed&PAD_SELECT){ currentVehicle=(currentVehicle+1)%VEHICLE_COUNT; speedKmh=0.0f; throttle=0.0f; }
 if(pressed&PAD_START && engineStage[currentVehicle]<ENGINE_UPGRADE_STAGES)engineStage[currentVehicle]++;
 if(pressed&PAD_R2)weather=(weather+1)%3;
 if(pressed&PAD_CIRCLE){
  if(hornWindow<=0)hornCount=0;
  hornCount++; hornWindow=55;
  if(hornCount>=3){
   float nearest=10000.0f; raceTarget=-1;
   for(int n=0;n<MAX_NPC;n++){
    if(traffic[n].z>6.0f && traffic[n].z<nearest){nearest=traffic[n].z;raceTarget=n;}
   }
   raceChallenge=(raceTarget>=0 && (traffic[raceTarget].style || (raceTarget%3==0)))?1:2;
   hornCount=0; hornWindow=0;
  }
 }
 if(pressed&PAD_R1)headlights=1;
 if(pressed&PAD_L1)headlights=0;
 if(pressed&PAD_TRIANGLE)nightMode=!nightMode;

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
  if(hornWindow>0)hornWindow--;
  if(brakeActive){
   /* Strong, progressive service brake instead of the old barely-visible drag. */
   speedKmh-=2.2f+(speedKmh*.035f);
   if(speedKmh<0)speedKmh=0;
  }else{
   speedKmh+=(throttle*(1.25f+vehicle_acceleration(currentVehicle,engineStage[currentVehicle])))-.12f;
   if(speedKmh<0)speedKmh=0;
  }
  if(speedKmh>vehicle_top_speed(currentVehicle,engineStage[currentVehicle]))speedKmh=vehicle_top_speed(currentVehicle,engineStage[currentVehicle]);
  roadScroll+=speedKmh*.0009f;if(roadScroll>7)roadScroll-=7;
  for(int i=0;i<MAX_NPC;i++){
   traffic[i].z-=.025f+speedKmh*.0014f-traffic[i].v*.025f;
   if(traffic[i].z<7){traffic[i].z=105+(float)(i*5);traffic[i].x=(float)((i%5)-2)*1.25f;}
   if(traffic[i].style&&(i%2)==0)traffic[i].x+=(float)sin((double)(roadScroll+i))*.002f;
  }
  gsKit_clear(gs,nightMode?col(7,12,22):col(90,145,175));
  drawRoad();
  for(int i=0;i<MAX_NPC;i++)drawCar(traffic[i].x,traffic[i].z,VEHICLES[traffic[i].model].length*.72f,VEHICLES[traffic[i].model].width*.82f,trafficColor(traffic[i].color),0,traffic[i].style);
  drawCar(playerX,4.2f,VEHICLES[currentVehicle].length,VEHICLES[currentVehicle].width,col(PAINTS[paintIndex][0],PAINTS[paintIndex][1],PAINTS[paintIndex][2]),1,engineStage[currentVehicle]>0);
  drawHud();
  gsKit_queue_exec(gs);gsKit_sync_flip(gs);gsKit_queue_reset(gs->Per_Queue);
 }
 return 0;
}
