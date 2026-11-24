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
static int upgradeMenu=0, upgradeCredits=3000;
static float creditProgress=0.0f;

typedef struct { float x,z,v; int color,style,model,direction; } TrafficCar;
/* v is each NPC's own road speed in km/h; direction -1 is oncoming traffic. */
static TrafficCar traffic[MAX_NPC]={
 {-1.25f,18,72,0,0,0, 1},{1.25f,29,94,1,1,1, 1},{0,43,68,2,0,2, 1},
 {-2.5f,57,105,3,1,3,1},{2.5f,71,82,4,0,4, 1},{-1.25f,88,115,5,1,5,1},{1.25f,105,62,6,0,6, 1}
};

static u64 col(u8 r,u8 g,u8 b){ return GS_SETREG_RGBAQ(r,g,b,0x80,0); }
static void quad(float a,float b,float c,float d,float e,float f,float g,float h,u64 color){
 gsKit_prim_quad(gs,a,b,c,d,e,f,g,h,2,color);
}
static int project(float x,float y,float z,float *sx,float *sy){
 /* Chase camera: elevated, slightly downward-looking, centered behind the car. */
 float focal=285.0f, horizon=(float)gs->Height*.315f;
 if(z<1.0f)return 0;
 *sx=(float)gs->Width*.5f+(x-playerX*.10f)*focal/z;
 *sy=horizon+(1.62f-y)*focal/z;
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
static void drawCar(float x,float z,float len,float width,u64 body,int model,int player,int tuned){
 /* Detailed low-poly vehicle assembly. Front points toward +Z. */
 float half=width*.5f, rear=z, front=z+len;
 float floorY=.10f, beltY=.73f, hoodY=.47f;
 float cabRear=z+len*.28f, cabFront=z+len*.69f;
 float roofY=1.28f, wheelR=.25f;
 int hatch=(model==1||model==2||model==7);
 int pickup=(model==9);
 if(hatch){cabRear=z+len*.22f;cabFront=z+len*.70f;roofY=model==7?1.10f:1.19f;}
 if(model==7){half*=.94f;hoodY=.43f;}
 if(pickup){cabRear=z+len*.43f;cabFront=z+len*.78f;roofY=1.22f;beltY=.78f;}
 if(model==4||model==8){roofY=1.34f;half*=1.02f;}
 u64 tire=col(10,11,13),rubber=col(24,25,27),rim=col(145,150,155);
 u64 glass=col(31,62,82),glassHi=col(78,112,130);
 u64 trim=col(25,27,30),chrome=col(170,175,178);
 u64 red=col(220,30,24),amber=col(245,150,35),white=col(250,235,190);
 u64 darkBody=col(48,51,55),bodyHi=col(215,215,210);
 /* Soft ground shadow. */
 face(x-half*1.18f,.015f,rear-.16f,x+half*1.18f,.015f,rear-.16f,
      x+half*1.18f,.015f,front+.18f,x-half*1.18f,.015f,front+.18f,col(28,30,31));
 /* Main body: rocker, doors, hood, rear quarter and bumpers. */
 face(x-half,floorY,rear,x+half,floorY,rear,x+half,beltY,rear,x-half,beltY,rear,body);
 face(x-half,floorY,rear,x-half,beltY,rear,x-half,beltY,front,x-half,floorY,front,body);
 face(x+half,floorY,rear,x+half,beltY,rear,x+half,beltY,front,x+half,floorY,front,body);
 face(x-half,floorY,front,x+half,floorY,front,x+half,hoodY,front,x-half,hoodY,front,body);
 face(x-half,hoodY,front,x+half,hoodY,front,x+half,beltY,front-len*.20f,x-half,beltY,front-len*.20f,body);
 face(x-half*.96f,floorY+.10f,rear+.10f,x+half*.96f,floorY+.10f,rear+.10f,
      x+half*.96f,floorY+.22f,rear+.10f,x-half*.96f,floorY+.22f,rear+.10f,chrome);
 face(x-half*.96f,floorY+.10f,front-.06f,x+half*.96f,floorY+.10f,front-.06f,
      x+half*.96f,floorY+.23f,front-.06f,x-half*.96f,floorY+.23f,front-.06f,trim);
 /* Roof, windshield, rear glass and side windows. */
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
 /* Window highlights and pillars add depth instead of a single glass slab. */
 for(int side=-1;side<=1;side+=2){
  float sx=x+(float)side*half*.80f;
  face(sx,beltY,cabRear+len*.035f,sx,beltY,cabRear+len*.065f,
       sx*.0f+x+(float)side*half*.58f,roofY,cabRear+len*.065f,
       sx*.0f+x+(float)side*half*.58f,roofY,cabRear+len*.035f,trim);
  face(x+(float)side*half*.80f,beltY,cabFront-len*.06f,
       x+(float)side*half*.80f,beltY,cabFront-len*.025f,
       x+(float)side*half*.58f,roofY,cabFront-len*.025f,
       x+(float)side*half*.58f,roofY,cabFront-len*.06f,trim);
 }
 /* Model-specific rear profile and pickup cargo bed. */
 if(pickup){
  face(x-half*.96f,beltY*.70f,rear+len*.04f,x-half*.96f,beltY*.70f,cabRear,
       x-half*.96f,beltY+.22f,cabRear,x-half*.96f,beltY+.22f,rear+len*.04f,body);
  face(x+half*.96f,beltY*.70f,rear+len*.04f,x+half*.96f,beltY*.70f,cabRear,
       x+half*.96f,beltY+.22f,cabRear,x+half*.96f,beltY+.22f,rear+len*.04f,body);
  face(x-half*.96f,beltY+.22f,rear+len*.04f,x+half*.96f,beltY+.22f,rear+len*.04f,
       x+half*.96f,beltY+.22f,cabRear,x-half*.96f,beltY+.22f,cabRear,trim);
  face(x-half*.82f,beltY*.72f,rear+len*.10f,x+half*.82f,beltY*.72f,rear+len*.10f,
       x+half*.82f,beltY*.72f,cabRear-len*.03f,x-half*.82f,beltY*.72f,cabRear-len*.03f,darkBody);
 }else if(hatch){
  face(x-half*.76f,beltY,cabRear,x+half*.76f,beltY,cabRear,
       x+half*.58f,roofY,cabRear+len*.10f,x-half*.58f,roofY,cabRear+len*.10f,body);
 }else{
  face(x-half*.78f,beltY,rear+len*.04f,x+half*.78f,beltY,rear+len*.04f,
       x+half*.72f,beltY+.08f,cabRear,x-half*.72f,beltY+.08f,cabRear,body);
 }
 /* Hood creases, front grille and lower air intake. */
 face(x-half*.63f,hoodY+.015f,front-.015f,x-half*.56f,hoodY+.015f,front-.015f,
      x-half*.48f,hoodY+.035f,front-len*.13f,x-half*.55f,hoodY+.035f,front-len*.13f,bodyHi);
 face(x+half*.56f,hoodY+.015f,front-.015f,x+half*.63f,hoodY+.015f,front-.015f,
      x+half*.55f,hoodY+.035f,front-len*.13f,x+half*.48f,hoodY+.035f,front-len*.13f,bodyHi);
 face(x-half*.43f,floorY+.30f,front+.045f,x+half*.43f,floorY+.30f,front+.045f,
      x+half*.43f,floorY+.49f,front+.045f,x-half*.43f,floorY+.49f,front+.045f,trim);
 for(int k=0;k<5;k++){
  float gy=floorY+.325f+(float)k*.032f;
  face(x-half*.37f,gy,front+.052f,x+half*.37f,gy,front+.052f,
       x+half*.37f,gy+.012f,front+.052f,x-half*.37f,gy+.012f,front+.052f,chrome);
 }
 face(x-half*.24f,floorY+.16f,front+.05f,x+half*.24f,floorY+.16f,front+.05f,
      x+half*.24f,floorY+.22f,front+.05f,x-half*.24f,floorY+.22f,front+.05f,darkBody);
 /* Headlights, indicators, fog lamps and rear light clusters. */
 for(int side=-1;side<=1;side+=2){
  float sx=x+(float)side;
  face(sx-half*.88f,floorY+.28f,front+.055f,sx-half*.48f,floorY+.28f,front+.055f,
       sx-half*.48f,floorY+.43f,front+.055f,sx-half*.88f,floorY+.43f,front+.055f,white);
  face(sx-half*.88f,floorY+.25f,front+.058f,sx-half*.79f,floorY+.25f,front+.058f,
       sx-half*.79f,floorY+.31f,front+.058f,sx-half*.88f,floorY+.31f,front+.058f,amber);
  face(sx-half*.91f,floorY+.30f,rear-.055f,sx-half*.53f,floorY+.30f,rear-.055f,
       sx-half*.53f,floorY+.57f,rear-.055f,sx-half*.91f,floorY+.57f,rear-.055f,red);
  face(sx-half*.89f,floorY+.12f,rear-.058f,sx-half*.76f,floorY+.12f,rear-.058f,
       sx-half*.76f,floorY+.24f,rear-.058f,sx-half*.89f,floorY+.24f,rear-.058f,amber);
  face(sx-half*.30f,floorY+.13f,front+.06f,sx-half*.18f,floorY+.13f,front+.06f,
       sx-half*.18f,floorY+.22f,front+.06f,sx-half*.30f,floorY+.22f,front+.06f,white);
 }
 /* Four tires with sidewalls, hub rings and center caps. */
 for(int side=-1;side<=1;side+=2){
  float wx=x+(float)side*(half+.045f);
  for(int axle=0;axle<2;axle++){
   float wz=axle?front-len*.22f:rear+len*.23f;
   face(wx-.14f,floorY-.015f,wz-wheelR,wx+.14f,floorY-.015f,wz-wheelR,
        wx+.14f,floorY+wheelR,wz-wheelR,wx-.14f,floorY+wheelR,wz-wheelR,tire);
   face(wx-.105f,floorY+.035f,wz-wheelR-.018f,wx+.105f,floorY+.035f,wz-wheelR-.018f,
        wx+.105f,floorY+.22f,wz-wheelR-.018f,wx-.105f,floorY+.22f,wz-wheelR-.018f,rubber);
   face(wx-.075f,floorY+.07f,wz-wheelR-.03f,wx+.075f,floorY+.07f,wz-wheelR-.03f,
        wx+.075f,floorY+.19f,wz-wheelR-.03f,wx-.075f,floorY+.19f,wz-wheelR-.03f,rim);
   face(wx-.025f,floorY+.105f,wz-wheelR-.04f,wx+.025f,floorY+.105f,wz-wheelR-.04f,
        wx+.025f,floorY+.155f,wz-wheelR-.04f,wx-.025f,floorY+.155f,wz-wheelR-.04f,chrome);
  }
  /* Side skirt, door shut lines, handles, mirror and mud flap. */
  float sx=x+(float)side*half;
  face(sx,floorY+.12f,rear+len*.28f,sx,floorY+.12f,front-len*.18f,
       sx,floorY+.20f,front-len*.18f,sx,floorY+.20f,rear+len*.28f,trim);
  face(sx,beltY*.40f,rear+len*.43f,sx,beltY*.40f,rear+len*.46f,
       sx,beltY*.88f,rear+len*.46f,sx,beltY*.88f,rear+len*.43f,trim);
  face(sx,beltY*.77f,rear+len*.50f,sx,beltY*.77f,rear+len*.57f,
       sx,beltY*.82f,rear+len*.57f,sx,beltY*.82f,rear+len*.50f,chrome);
  face(sx,beltY-.03f,cabFront-.08f,sx+(float)side*.11f,beltY-.03f,cabFront-.08f,
       sx+(float)side*.11f,beltY+.07f,cabFront-.08f,sx,beltY+.07f,cabFront-.08f,trim);
  face(sx,floorY+.03f,rear+len*.16f,sx,floorY+.03f,rear+len*.24f,
       sx,floorY+.15f,rear+len*.24f,sx,floorY+.15f,rear+len*.16f,darkBody);
 }
 /* Front and rear registration plates, exhaust, spoiler/roof antenna. */
 face(x-half*.22f,floorY+.25f,front+.065f,x+half*.22f,floorY+.25f,front+.065f,
      x+half*.22f,floorY+.31f,front+.065f,x-half*.22f,floorY+.31f,front+.065f,col(225,225,215));
 face(x-half*.22f,floorY+.24f,rear-.065f,x+half*.22f,floorY+.24f,rear-.065f,
      x+half*.22f,floorY+.30f,rear-.065f,x-half*.22f,floorY+.30f,rear-.065f,col(225,225,215));
 if(hatch||tuned){
  face(x-half*.70f,roofY+.015f,cabRear+len*.06f,x+half*.70f,roofY+.015f,cabRear+len*.06f,
       x+half*.70f,roofY+.09f,cabRear+len*.06f,x-half*.70f,roofY+.09f,cabRear+len*.06f,trim);
 }
 if(tuned){
  face(x-half*.92f,beltY+.04f,rear+.08f,x+half*.92f,beltY+.04f,rear+.08f,
       x+half*.92f,beltY+.12f,rear+.08f,x-half*.92f,beltY+.12f,rear+.08f,chrome);
  face(x-half*.10f,floorY+.12f,rear-.09f,x+half*.10f,floorY+.12f,rear-.09f,
       x+half*.10f,floorY+.17f,rear-.09f,x-half*.10f,floorY+.17f,rear-.09f,trim);
 }
 (void)player;
}
static void drawRoad(void){
 float horizon=(float)gs->Height*.39f;
 u64 sky=nightMode?col(8,15,30):(weather==2?col(145,155,160):(weather==1?col(80,105,125):col(105,170,205)));
 gsKit_prim_sprite(gs,0,0,(float)gs->Width,horizon,1,sky);
 gsKit_prim_sprite(gs,0,horizon,(float)gs->Width,(float)gs->Height,1,nightMode?col(16,25,19):col(58,105,48));
 /* Broad highway asphalt and gravel shoulders. */
 face(-9.0f,-1.62f,3,9.0f,-1.62f,3,9.0f,-1.62f,150,-9.0f,-1.62f,150,col(57,59,62));
 face(-9.7f,-1.62f,3,-9.0f,-1.62f,3,-9.0f,-1.62f,150,-9.7f,-1.62f,150,col(112,108,94));
 face(9.0f,-1.62f,3,9.7f,-1.62f,3,9.7f,-1.62f,150,9.0f,-1.62f,150,col(112,108,94));
 face(-8.35f,-1.57f,3,-8.22f,-1.57f,3,-8.22f,-1.57f,150,-8.35f,-1.57f,150,col(235,230,210));
 face(8.22f,-1.57f,3,8.35f,-1.57f,3,8.35f,-1.57f,150,8.22f,-1.57f,150,col(235,230,210));
 for(int i=0;i<20;i++){
  float z=4.0f+(float)i*6.3f-roadScroll;
  while(z<3.5f)z+=126.0f;
  while(z>130.0f)z-=126.0f;
  for(int lane=-1;lane<=1;lane+=2){
   float lx=(float)lane*2.75f;
   face(lx-.075f,-1.565f,z,lx+.075f,-1.565f,z,lx+.075f,-1.565f,z+4.25f,lx-.075f,-1.565f,z+4.25f,col(238,237,222));
  }
  for(int side=-1;side<=1;side+=2){
   float lx=(float)side*.12f;
   face(lx-.045f,-1.56f,z,lx+.045f,-1.56f,z,lx+.045f,-1.56f,z+4.25f,lx-.045f,-1.56f,z+4.25f,col(238,190,48));
   float gx=(float)side*9.35f;
   face(gx-.035f,-1.48f,z,gx+.035f,-1.48f,z,gx+.035f,-.45f,z,gx-.035f,-.45f,z,col(125,130,132));
   face(gx-.06f,-.62f,z,gx+.06f,-.62f,z,gx+.06f,-.38f,z+6.0f,gx-.06f,-.38f,z+6.0f,col(165,170,172));
  }
 }
 if(weather==1)for(int r=0;r<24;r++){
  float rx=(float)((r*37+(int)(roadScroll*90))%620);
  float ry=(float)((r*53+(int)(roadScroll*170))%430);
  gsKit_prim_sprite(gs,rx,ry,rx+1.0f,ry+9.0f,1,col(170,195,215));
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
 if(upgradeMenu){
  gsKit_prim_sprite(gs,w*.27f,h*.24f,w*.73f,h*.72f,1,col(12,15,20));
  gsKit_prim_sprite(gs,w*.30f,h*.30f,w*.70f,h*.34f,1,col(48,54,62));
  for(int n=0;n<ENGINE_UPGRADE_STAGES;n++){
   float x=w*.31f+(float)n*w*.076f;
   gsKit_prim_sprite(gs,x,h*.40f,x+w*.064f,h*.49f,1,n<engineStage[currentVehicle]?col(240,170,35):col(75,82,92));
  }
  gsKit_prim_sprite(gs,w*.31f,h*.56f,w*.69f,h*.60f,1,col(42,45,50));
  gsKit_prim_sprite(gs,w*.31f,h*.56f,w*.31f+(w*.38f)*((float)upgradeCredits/6000.0f),h*.60f,1,col(55,155,95));
  gsKit_prim_sprite(gs,w*.31f,h*.65f,w*.69f,h*.68f,1,col(195,135,35));
 }
}
static void initPad(void){ padInit(0); padPortOpen(0,0,padBuf); }
static void controls(void){
 if(!padRead(0,0,&pad)){ brakeActive=0; throttle=0.0f; return; }
 /* R2 accelerates, L2 brakes; left analog stick steers. */
 brakeActive=(!(pad.btns&PAD_L2));
 u16 pressed=(u16)((~pad.btns)&previousButtons);
 previousButtons=pad.btns;
 int stick=(int)pad.ljoy_h-128;
 if(stick>-12 && stick<12)stick=0;
 playerX+=((float)stick/127.0f)*.075f;
 if(playerX< -6.0f)playerX=-6.0f;
 if(playerX>6.0f)playerX=6.0f;
 if(!(pad.btns&PAD_R2))throttle+=.018f;else throttle-=.004f;
 if(brakeActive)throttle=0.0f;
 if(throttle<0)throttle=0;if(throttle>1)throttle=1;
 if(pressed&PAD_SELECT){ currentVehicle=(currentVehicle+1)%VEHICLE_COUNT; speedKmh=0.0f; throttle=0.0f; }
 if(pressed&PAD_START)upgradeMenu=!upgradeMenu;
 if(upgradeMenu){
  throttle=0.0f;
  if(pressed&PAD_CROSS && engineStage[currentVehicle]<ENGINE_UPGRADE_STAGES){
   int cost=500*(engineStage[currentVehicle]+1);
   if(upgradeCredits>=cost){upgradeCredits-=cost;engineStage[currentVehicle]++;}
  }
 }
 if(pressed&PAD_L1)weather=(weather+1)%3;
 if(pressed&PAD_CIRCLE && !upgradeMenu){
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
 if(pressed&PAD_R1)headlights=!headlights;
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
   /* Progressive road-car braking: decelerates firmly without snapping to zero. */
   speedKmh-=0.42f+(speedKmh*.0045f);
   if(speedKmh<0)speedKmh=0;
  }else{
   speedKmh+=(throttle*(2.05f+vehicle_acceleration(currentVehicle,engineStage[currentVehicle])*1.45f))-.045f;
   if(speedKmh<0)speedKmh=0;
  }
  if(speedKmh>vehicle_top_speed(currentVehicle,engineStage[currentVehicle]))speedKmh=vehicle_top_speed(currentVehicle,engineStage[currentVehicle]);
  creditProgress+=speedKmh*.000006f;
  if(creditProgress>=1.0f){upgradeCredits+=100;creditProgress-=1.0f;if(upgradeCredits>6000)upgradeCredits=6000;}
  roadScroll+=speedKmh*.00145f;if(roadScroll>126.0f)roadScroll-=126.0f;
  for(int i=0;i<MAX_NPC;i++){
   /* Relative motion: same-direction cars can pull away or be overtaken.
      Oncoming cars approach faster; NPCs are never frozen with player braking. */
   float npcSignedSpeed=(float)traffic[i].direction*traffic[i].v;
   traffic[i].z+=(npcSignedSpeed-speedKmh)*.00145f;
   if(traffic[i].z<7.0f){
    traffic[i].z=112.0f+(float)(i*4);
    traffic[i].x=(float)((i%5)-2)*1.25f;
   }else if(traffic[i].z>132.0f){
    traffic[i].z=12.0f+(float)(i*3);
    traffic[i].x=(float)((i%5)-2)*1.25f;
   }
   if(traffic[i].style&&(i%2)==0)
    traffic[i].x+=(float)sin((double)(roadScroll+i))*.002f;
  }
  gsKit_clear(gs,nightMode?col(7,12,22):col(90,145,175));
  drawRoad();
  for(int i=0;i<MAX_NPC;i++)drawCar(traffic[i].x,traffic[i].z,VEHICLES[traffic[i].model].length*.72f,VEHICLES[traffic[i].model].width*.82f,trafficColor(traffic[i].color),traffic[i].model,0,traffic[i].style);
  drawCar(playerX,5.6f,VEHICLES[currentVehicle].length,VEHICLES[currentVehicle].width,col(PAINTS[paintIndex][0],PAINTS[paintIndex][1],PAINTS[paintIndex][2]),currentVehicle,1,engineStage[currentVehicle]>0);
  drawHud();
  gsKit_queue_exec(gs);gsKit_sync_flip(gs);gsKit_queue_reset(gs->Per_Queue);
 }
 return 0;
}
