#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <pspiofilemgr.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

PSP_MODULE_INFO("ShadowFightLite", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
#define SW 480
#define SH 272
#define BW 512
#define GY 220
static unsigned int __attribute__((aligned(64))) fb[BW * SH];
#define RGB(r,g,b) (0xFF000000u|((unsigned)(r)<<16)|((unsigned)(g)<<8)|(unsigned)(b))

/* ============ ПРИМИТИВЫ ============ */
static void fillRect(int x,int y,int w,int h,unsigned int c){
    int x0=x,y0=y,x1=x+w,y1=y+h;
    if(x0<0)x0=0;if(y0<0)y0=0;if(x1>SW)x1=SW;if(y1>SH)y1=SH;
    if(x1<=x0||y1<=y0)return;
    for(int yy=y0;yy<y1;yy++){unsigned int*r=&fb[yy*BW+x0];int n=x1-x0;for(int i=0;i<n;i++)r[i]=c;}
}
static void px(int x,int y,unsigned int c){if(x<0||x>=SW||y<0||y>=SH)return;fb[y*BW+x]=c;}
static void line(int x0,int y0,int x1,int y1,unsigned int c,int t){
    int dx=abs(x1-x0),dy=-abs(y1-y0);int sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx+dy,r=t/2;
    while(1){for(int oy=-r;oy<=r;oy++)for(int ox=-r;ox<=r;ox++)px(x0+ox,y0+oy,c);
        if(x0==x1&&y0==y1)break;int e2=2*err;
        if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}
}
static void circle(int cx,int cy,int r,unsigned int c){
    for(int y=-r;y<=r;y++){int w=(int)sqrtf((float)(r*r-y*y));for(int x=-w;x<=w;x++)px(cx+x,cy+y,c);}
}

/* ============ ШРИФТ ============ */
typedef struct{unsigned char ch;unsigned char d[5];}Glyph;
static const Glyph FONT[]={
    {' ',{0,0,0,0,0}},{'.',{0,0x60,0x60,0,0}},{'-',{0x08,0x08,0x08,0x08,0x08}},
    {':',{0,0x36,0x36,0,0}},{'/',{0x20,0x10,0x08,0x04,0x02}},{'!',{0,0,0x5F,0,0}},
    {'0',{0x3E,0x51,0x49,0x45,0x3E}},{'1',{0,0x42,0x7F,0x40,0}},{'2',{0x42,0x61,0x51,0x49,0x46}},
    {'3',{0x21,0x41,0x45,0x4B,0x31}},{'4',{0x18,0x14,0x12,0x7F,0x10}},{'5',{0x27,0x45,0x45,0x45,0x39}},
    {'6',{0x3C,0x4A,0x49,0x49,0x30}},{'7',{0x01,0x71,0x09,0x05,0x03}},{'8',{0x36,0x49,0x49,0x49,0x36}},
    {'9',{0x06,0x49,0x49,0x29,0x1E}},{',',{0,0x50,0x30,0,0}},{'%',{0x63,0x13,0x08,0x64,0x63}},
    {'A',{0x7E,0x11,0x11,0x11,0x7E}},{'B',{0x7F,0x49,0x49,0x49,0x36}},{'C',{0x3E,0x41,0x41,0x41,0x22}},
    {'D',{0x7F,0x41,0x41,0x22,0x1C}},{'E',{0x7F,0x49,0x49,0x49,0x41}},{'F',{0x7F,0x09,0x09,0x09,0x01}},
    {'G',{0x3E,0x41,0x49,0x49,0x7A}},{'H',{0x7F,0x08,0x08,0x08,0x7F}},{'I',{0,0x41,0x7F,0x41,0}},
    {'J',{0x20,0x40,0x41,0x3F,0x01}},{'K',{0x7F,0x08,0x14,0x22,0x41}},{'L',{0x7F,0x40,0x40,0x40,0x40}},
    {'M',{0x7F,0x02,0x0C,0x02,0x7F}},{'N',{0x7F,0x04,0x08,0x10,0x7F}},{'O',{0x3E,0x41,0x41,0x41,0x3E}},
    {'P',{0x7F,0x09,0x09,0x09,0x06}},{'Q',{0x3E,0x41,0x51,0x21,0x5E}},{'R',{0x7F,0x09,0x19,0x29,0x46}},
    {'S',{0x46,0x49,0x49,0x49,0x31}},{'T',{0x01,0x01,0x7F,0x01,0x01}},{'U',{0x3F,0x40,0x40,0x40,0x3F}},
    {'V',{0x1F,0x20,0x40,0x20,0x1F}},{'W',{0x7F,0x20,0x18,0x20,0x7F}},{'X',{0x63,0x14,0x08,0x14,0x63}},
    {'Y',{0x03,0x04,0x78,0x04,0x03}},{'Z',{0x61,0x51,0x49,0x45,0x43}},
};
typedef struct{unsigned char b1,b2;unsigned char d[5];}CyrGlyph;
static const CyrGlyph CYR[]={
    {0xD0,0x90,{0x7E,0x11,0x11,0x11,0x7E}},{0xD0,0x91,{0x7F,0x49,0x49,0x49,0x31}},{0xD0,0x92,{0x7F,0x49,0x49,0x49,0x36}},
    {0xD0,0x93,{0x7F,0x01,0x01,0x01,0x01}},{0xD0,0x94,{0x7E,0x21,0x21,0x21,0x7F}},{0xD0,0x95,{0x7F,0x49,0x49,0x49,0x41}},
    {0xD0,0x81,{0x7F,0x4B,0x49,0x4B,0x41}},{0xD0,0x96,{0x63,0x14,0x7F,0x14,0x63}},{0xD0,0x97,{0x22,0x41,0x49,0x49,0x36}},
    {0xD0,0x98,{0x7F,0x10,0x08,0x04,0x7F}},{0xD0,0x99,{0x7F,0x10,0x0A,0x05,0x7F}},{0xD0,0x9A,{0x7F,0x08,0x14,0x22,0x41}},
    {0xD0,0x9B,{0x3F,0x40,0x40,0x40,0x7F}},{0xD0,0x9C,{0x7F,0x02,0x0C,0x02,0x7F}},{0xD0,0x9D,{0x7F,0x08,0x08,0x08,0x7F}},
    {0xD0,0x9E,{0x3E,0x41,0x41,0x41,0x3E}},{0xD0,0x9F,{0x7F,0x01,0x01,0x01,0x7F}},{0xD0,0xA0,{0x7F,0x09,0x09,0x09,0x06}},
    {0xD0,0xA1,{0x3E,0x41,0x41,0x41,0x22}},{0xD0,0xA2,{0x01,0x01,0x7F,0x01,0x01}},{0xD0,0xA3,{0x27,0x48,0x48,0x48,0x3F}},
    {0xD0,0xA4,{0x1C,0x22,0x7F,0x22,0x1C}},{0xD0,0xA5,{0x63,0x14,0x08,0x14,0x63}},{0xD0,0xA6,{0x7F,0x40,0x40,0x40,0x6F}},
    {0xD0,0xA7,{0x07,0x08,0x08,0x08,0x7F}},{0xD0,0xA8,{0x7F,0x40,0x7F,0x40,0x7F}},{0xD0,0xA9,{0x7F,0x40,0x7F,0x40,0x6F}},
    {0xD0,0xAA,{0x01,0x3F,0x48,0x48,0x30}},{0xD0,0xAB,{0x7F,0x48,0x30,0,0x7F}},{0xD0,0xAC,{0x7F,0x48,0x48,0x48,0x30}},
    {0xD0,0xAD,{0x22,0x41,0x49,0x49,0x3E}},{0xD0,0xAE,{0x7F,0x08,0x3E,0x41,0x3E}},{0xD0,0xAF,{0x46,0x29,0x19,0x09,0x7F}},
    {0xD0,0xB0,{0x20,0x54,0x54,0x54,0x78}},{0xD0,0xB1,{0x7F,0x48,0x44,0x44,0x38}},{0xD0,0xB2,{0x38,0x54,0x54,0x54,0x28}},
    {0xD0,0xB3,{0x7C,0x04,0x04,0x04,0x04}},{0xD0,0xB4,{0x38,0x44,0x44,0x44,0x7C}},{0xD0,0xB5,{0x38,0x54,0x54,0x54,0x18}},
    {0xD1,0x91,{0x38,0x56,0x54,0x56,0x18}},{0xD0,0xB6,{0x44,0x28,0x7C,0x28,0x44}},{0xD0,0xB7,{0x48,0x54,0x54,0x54,0x24}},
    {0xD0,0xB8,{0x7C,0x20,0x10,0x08,0x7C}},{0xD0,0xB9,{0x7C,0x20,0x14,0x0A,0x7C}},{0xD0,0xBA,{0x7C,0x10,0x28,0x44,0x00}},
    {0xD0,0xBB,{0x3C,0x40,0x40,0x40,0x7C}},{0xD0,0xBC,{0x7C,0x08,0x10,0x08,0x7C}},{0xD0,0xBD,{0x7C,0x10,0x10,0x10,0x7C}},
    {0xD0,0xBE,{0x38,0x44,0x44,0x44,0x38}},{0xD0,0xBF,{0x7C,0x04,0x04,0x04,0x7C}},{0xD1,0x80,{0x7C,0x14,0x14,0x14,0x08}},
    {0xD1,0x81,{0x38,0x44,0x44,0x44,0x20}},{0xD1,0x82,{0x04,0x04,0x7C,0x04,0x04}},{0xD1,0x83,{0x0C,0x50,0x50,0x50,0x3C}},
    {0xD1,0x84,{0x18,0x24,0x7C,0x24,0x18}},{0xD1,0x85,{0x44,0x28,0x10,0x28,0x44}},{0xD1,0x86,{0x7C,0x40,0x40,0x40,0x7C}},
    {0xD1,0x87,{0x0C,0x10,0x10,0x10,0x7C}},{0xD1,0x88,{0x7C,0x40,0x7C,0x40,0x7C}},{0xD1,0x89,{0x7C,0x40,0x7C,0x40,0x6C}},
    {0xD1,0x8A,{0x04,0x3C,0x50,0x50,0x20}},{0xD1,0x8B,{0x7C,0x50,0x20,0x00,0x7C}},{0xD1,0x8C,{0x7C,0x50,0x50,0x50,0x20}},
    {0xD1,0x8D,{0x28,0x44,0x54,0x54,0x38}},{0xD1,0x8E,{0x7C,0x10,0x38,0x44,0x38}},{0xD1,0x8F,{0x4C,0x34,0x14,0x14,0x7C}},
};
static const unsigned char* lookupGlyph(const char* t,int* bytes){
    unsigned char b1=(unsigned char)t[0];
    if(b1<0x80){*bytes=1;for(unsigned int i=0;i<sizeof(FONT)/sizeof(FONT[0]);i++)if(FONT[i].ch==b1)return FONT[i].d;return FONT[0].d;}
    if(b1==0xD0||b1==0xD1){unsigned char b2=(unsigned char)t[1];*bytes=2;
        for(unsigned int i=0;i<sizeof(CYR)/sizeof(CYR[0]);i++)if(CYR[i].b1==b1&&CYR[i].b2==b2)return CYR[i].d;return FONT[0].d;}
    *bytes=1;return FONT[0].d;
}
static void drawText(int x,int y,const char* t,unsigned int col,int s){
    int cx=x;
    while(*t){if(*t=='\n'){cx=x;y+=8*s;t++;continue;}
        int bytes;const unsigned char* g=lookupGlyph(t,&bytes);
        for(int c=0;c<5;c++){unsigned char b=g[c];for(int r=0;r<7;r++)if(b&(1<<r))fillRect(cx+c*s,y+r*s,s,s,col);}
        cx+=6*s;t+=bytes;}
}
static int textW(const char* t,int s){
    int w=0,mx=0;
    while(*t){if(*t=='\n'){if(w>mx)mx=w;w=0;t++;continue;}int bytes;lookupGlyph(t,&bytes);w+=6*s;t+=bytes;}
    return w>mx?w:mx;
}
static void drawTextC(int y,const char* t,unsigned int col,int s){drawText((SW-textW(t,s))/2,y,t,col,s);}

/* ============ ЗВУК ============ */
#define AUD_SAMPLES 1024
static short gAudioBuf[AUD_SAMPLES*2];
static int gAudioReady=0;
static int gVolume=70;
typedef struct{int freq;int duration;int maxDuration;int type;int vol;}SoundSrc;
static SoundSrc gSnd[8];
static int gMusicTimer=0,gMusicNote=0,gMusicType=0;
static void audioInit(void){if(sceAudioSRCChReserve(AUD_SAMPLES,44100,2)<0)return;gAudioReady=1;}
static void sndPlay(int freq,int dur,int type,int vol){
    if(!gAudioReady)return;vol=vol*gVolume/100;
    for(int i=0;i<8;i++){if(gSnd[i].duration<=0){gSnd[i].freq=freq;gSnd[i].duration=dur;gSnd[i].maxDuration=dur;gSnd[i].type=type;gSnd[i].vol=vol;return;}}
}
static void audioTick(void){
    if(!gAudioReady)return;
    for(int i=0;i<AUD_SAMPLES;i++){
        int sample=0;
        for(int s=0;s<8;s++){
            if(gSnd[s].duration<=0)continue;
            float env=(float)gSnd[s].duration/(float)gSnd[s].maxDuration;int sVal=0;
            if(gSnd[s].type==2)sVal=(rand()&0xFFFF)-0x8000;
            else{int period=44100/(gSnd[s].freq>0?gSnd[s].freq:1);if(period<2)period=2;int phase=(i+s*77)%period;
                sVal=(gSnd[s].type==0)?((phase<period/2)?0x4000:-0x4000):((phase<period/2)?0x5000:-0x5000);}
            sample+=(int)(sVal*env*gSnd[s].vol/100.0f/2);
        }
        if(sample>32767)sample=32767;if(sample<-32768)sample=-32768;
        gAudioBuf[i*2]=sample;gAudioBuf[i*2+1]=sample;
    }
    sceAudioSRCOutputBlocking(0x8000,gAudioBuf);
    for(int s=0;s<8;s++)if(gSnd[s].duration>0)gSnd[s].duration-=(AUD_SAMPLES*1000/44100);
}
static void sfxHit(void){sndPlay(180,90,2,70);sndPlay(80,140,0,50);}
static void sfxHeavy(void){sndPlay(120,180,2,80);sndPlay(55,220,0,60);}
static void sfxBlock(void){sndPlay(900,70,0,50);}
static void sfxWhoosh(void){sndPlay(300,60,2,25);}
static void sfxSelect(void){sndPlay(660,60,1,40);sndPlay(880,80,1,35);}
static void sfxWin(void){sndPlay(440,140,1,50);sndPlay(554,140,1,50);sndPlay(659,280,1,55);}
static void sfxLose(void){sndPlay(330,180,1,50);sndPlay(220,320,1,50);}
static void sfxMagic(void){sndPlay(1200,200,0,60);sndPlay(900,180,0,40);}
static void sfxCrit(void){sndPlay(200,300,2,90);sndPlay(100,300,0,70);}
static void sfxCoin(void){sndPlay(1400,60,1,50);sndPlay(1800,100,1,40);}
static void sfxAch(void){sndPlay(660,120,1,50);sndPlay(880,120,1,50);sndPlay(1320,300,1,55);}
static void sfxGem(void){sndPlay(1800,80,1,55);sndPlay(2400,120,1,50);sndPlay(3000,180,1,50);}
static void sfxLevel(void){sndPlay(880,120,1,55);sndPlay(1100,120,1,55);sndPlay(1320,180,1,55);sndPlay(1760,300,1,60);}
static void musicTick(int ms){
    if(!gAudioReady)return;gMusicTimer-=ms;if(gMusicTimer>0)return;
    if(gMusicType==1){int sc[]={110,130,146,164,196,220,261};sndPlay(sc[gMusicNote%7],1500,0,25);
        if(gMusicNote%3==0)sndPlay(55,2200,0,20);gMusicTimer=1400;gMusicNote++;}
    else if(gMusicType==2){int b=gMusicNote%8;if(b%2==0)sndPlay(60,120,0,60);
        if(b==2||b==6)sndPlay(0,80,2,40);if(b%4==0)sndPlay(220,150,0,15);
        if(b==0||b==5)sndPlay(330,200,0,12);gMusicTimer=240;gMusicNote++;}
    else gMusicTimer=100;
}
static void musicSet(int t){gMusicType=t;gMusicTimer=0;gMusicNote=0;}

/* ============ ДАННЫЕ ============ */
typedef struct{const char*name;int type,dmg,range,price;float dur,hs,he,cd;}WeaponDef;
static const WeaponDef WEAPONS[17]={
    {"НОЖИ",0,6,64,0,0.24f,0.05f,0.14f,0.24f},{"КАСТЕТЫ",1,5,54,0,0.20f,0.04f,0.11f,0.19f},
    {"САИ",2,7,66,150,0.28f,0.06f,0.16f,0.30f},{"СТ.ДУБИНКИ",3,9,70,200,0.36f,0.09f,0.20f,0.42f},
    {"МЕЧ НИНДЗЯ",4,10,80,300,0.32f,0.08f,0.18f,0.38f},{"МАЧЕТЕ",5,12,74,400,0.40f,0.11f,0.22f,0.48f},
    {"КИНЖАЛЫ",6,8,60,250,0.22f,0.05f,0.13f,0.24f},{"ЖАЛО",7,9,72,350,0.26f,0.06f,0.15f,0.28f},
    {"КРОВ.ЖНЕЦ",8,14,88,600,0.45f,0.12f,0.26f,0.55f},{"ЯР.ТОНФЫ",9,8,66,450,0.26f,0.06f,0.15f,0.28f},
    {"НУНЧАКИ",10,7,74,300,0.30f,0.08f,0.17f,0.32f},{"ПОЛУМЕСЯЦЫ",11,11,78,700,0.35f,0.09f,0.19f,0.40f},
    {"ТОНФЫ",9,9,70,500,0.28f,0.07f,0.16f,0.32f},{"НАГИНАТА",12,13,95,800,0.48f,0.13f,0.28f,0.58f},
    {"ПОСОХ",13,10,90,650,0.40f,0.11f,0.24f,0.48f},{"ШИПЫ ПУСТЫНИ",14,12,72,750,0.38f,0.10f,0.22f,0.44f},
    {"МРАЧНАЯ КОСА",8,15,92,1000,0.50f,0.14f,0.30f,0.62f},
};
typedef struct{const char*name;int hp,dmg,range,wt;float speed,cd;unsigned int color;int boss;int act;}EnemyDef;
static const EnemyDef ENEMIES[13]={
    {"ШИН",60,6,64,0,130.0f,0.95f,RGB(124,124,144),0,1},{"КИРПИЧ",85,9,70,3,140.0f,0.90f,RGB(141,122,104),0,1},
    {"ИГЛА",80,8,66,2,170.0f,0.75f,RGB(160,110,200),0,1},{"ПРИЗРАК",100,10,80,4,175.0f,0.72f,RGB(108,168,216),0,1},
    {"ЩЕГОЛЬ",120,11,74,5,190.0f,0.65f,RGB(216,168,80),0,1},{"РЫСЬ",180,14,88,8,210.0f,0.58f,RGB(217,144,60),1,1},
    {"САБЛЯ",90,9,68,0,145.0f,0.90f,RGB(140,100,90),0,2},{"ПУСТЫННИК",110,11,74,11,165.0f,0.80f,RGB(180,160,120),0,2},
    {"КРИС",95,10,72,6,185.0f,0.72f,RGB(120,160,180),0,2},{"МРАЧНЫЙ",130,12,80,13,190.0f,0.68f,RGB(100,80,140),0,2},
    {"ПЕСЧАНЫЙ",140,13,78,14,200.0f,0.62f,RGB(200,160,80),0,2},{"ЯРИ",150,14,92,12,205.0f,0.60f,RGB(220,140,60),0,2},
    {"ОТШЕЛЬНИК",220,16,96,15,220.0f,0.52f,RGB(180,60,60),1,2},
};
typedef struct{const char*name;int hp,dmg;float speed;unsigned int color;int wt;}TourFighter;
static const TourFighter TOUR[24]={
    {"СТАЖЁР",40,4,110,RGB(140,140,160),0},{"НОВИЧОК",50,5,115,RGB(130,150,140),1},
    {"БОЕЦ",60,6,120,RGB(150,130,110),0},{"СТРЕЛОК",65,6,130,RGB(120,140,180),6},
    {"БАНДИТ",70,7,135,RGB(160,120,100),5},{"НАЁМНИК",75,7,140,RGB(130,130,130),2},
    {"ТЕНЬ",80,8,150,RGB(80,80,120),7},{"КЛИНОК",85,8,155,RGB(140,140,180),4},
    {"ВЕТЕРАН",90,9,160,RGB(120,100,80),3},{"МАСТЕР",95,9,165,RGB(100,140,120),2},
    {"СТРАЖ",100,10,170,RGB(90,120,90),3},{"УБИЙЦА",105,10,175,RGB(140,80,80),0},
    {"ЛАЗУТЧИК",110,11,180,RGB(100,100,140),6},{"БОЕВОЙ МАГ",115,11,185,RGB(140,80,180),13},
    {"КАПИТАН",120,12,190,RGB(160,140,80),4},{"ГЕНЕРАЛ",130,12,195,RGB(180,120,60),3},
    {"ЛЕГИОНЕР",140,13,200,RGB(140,140,100),11},{"БЕРСЕРК",150,14,205,RGB(200,60,60),5},
    {"ХРАНИТЕЛЬ",160,15,210,RGB(60,140,140),12},{"ВЕЛИКИЙ",170,16,215,RGB(140,60,180),8},
    {"ЦЕНТУРИОН",180,17,220,RGB(200,180,60),12},{"ЧЕМПИОН",190,18,225,RGB(220,140,60),4},
    {"ГРАНД-МАСТЕР",210,20,230,RGB(255,100,50),8},{"КОРОЛЬ ТУРНИРА",250,24,240,RGB(255,60,60),8},
};
typedef struct{const char*name;float hpMul,dmgMul,spdMul;}DiffDef;
static const DiffDef DIFFS[4]={
    {"ЛЕГКО",0.7f,0.7f,0.85f},{"НОРМАЛЬНО",1.0f,1.0f,1.0f},
    {"СЛОЖНО",1.3f,1.3f,1.1f},{"НЕВОЗМОЖНО",1.6f,1.6f,1.25f},
};
typedef struct{const char*name;int defense;int gems;unsigned int color;}ArmorDef;
static const ArmorDef ARMORS[6]={
    {"БЕЗ БРОНИ",0,0,RGB(0,0,0)},{"КОЖАНАЯ",3,2,RGB(120,80,40)},{"КЕВЛАР",6,5,RGB(60,60,80)},
    {"ПЛАСТИНА",10,10,RGB(140,140,160)},{"ТЕНЕВАЯ",15,20,RGB(60,30,90)},{"ЛЕГЕНДА",22,40,RGB(220,180,60)},
};
typedef struct{const char*name;int defense;int gems;unsigned int color;}HelmetDef;
static const HelmetDef HELMETS[6]={
    {"БЕЗ ШЛЕМА",0,0,RGB(0,0,0)},{"ПОВЯЗКА",1,2,RGB(140,100,60)},{"КОЖАНЫЙ",3,5,RGB(120,80,40)},
    {"МЕТАЛЛ",6,10,RGB(160,160,180)},{"ТЁМНЫЙ",10,20,RGB(60,30,90)},{"ЗОЛОТОЙ",15,40,RGB(220,180,60)},
};
typedef struct{const char*name;int reward;int target;}MissionDef;
static const MissionDef MISSIONS[8]={
    {"ПОБЕДИТЬ 3 ВРАГА",1,3},{"ПОБЕДИТЬ РЫСЯ",3,1},{"ПОБЕДИТЬ ОТШЕЛЬНИКА",5,1},
    {"ПОБЕДИТЬ 10 ВРАГОВ",2,10},{"ПОБЕДИТЬ 25 ВРАГОВ",4,25},{"ИЗУЧИТЬ ВСЕ ПРИЁМЫ",3,1},
    {"КУПИТЬ 5 ОРУЖИЙ",2,5},{"СОБРАТЬ 3 ДОСТИЖЕНИЯ",3,3},
};
typedef struct{const char*name;const char*desc;int unlockLevel;}MoveUnlock;
static const MoveUnlock MOVE_UNLOCKS[8]={
    {"БЛОК","КВАДРАТ",1},{"ПИНОК","ВНИЗ + X",2},{"ДВОЙНОЙ УДАР","X X быстро",3},
    {"УДАР В ПРЫЖКЕ","Прыжок + X",4},{"ТЯЖЁЛЫЙ УДАР","ВПЕРЁД + X",5},
    {"МАГИЯ","ТРЕУГОЛЬНИК",6},{"ЯРОСТЬ","НАЗАД + X",7},{"УЛЬТА","X X X",8},
};
typedef struct{const char*name;unsigned int skyTop,skyMid,skyBot,wall,ground,accent;}LocDef;
static const LocDef LOCS[3]={
    {"ДВОР",RGB(10,13,26),RGB(26,16,48),RGB(44,16,56),RGB(20,12,34),RGB(12,7,20),RGB(123,63,255)},
    {"КРЫША",RGB(5,8,21),RGB(15,26,53),RGB(30,20,64),RGB(10,10,24),RGB(6,8,16),RGB(79,163,255)},
    {"ПЕЩЕРА",RGB(21,8,8),RGB(42,16,8),RGB(51,26,8),RGB(26,10,5),RGB(16,7,5),RGB(255,139,58)},
};
typedef struct{const char*speaker;const char*lines[3];int count;}Dialog;
static const Dialog DIALOGS[13]={
    {"ШИН",{"Кто ты такой? Я не трачу время на червей.","Рысь поручил охранять этот зал."},2},
    {"КИРПИЧ",{"Ха! Ещё один глупец!","Я раздавлю тебя!"},2},
    {"ИГЛА",{"Ты не должен был приходить сюда.","Орден превыше всего!"},2},
    {"ПРИЗРАК",{"Ты не первый, кто бросил вызов Рыси,","и не последний, кто падёт."},2},
    {"ЩЕГОЛЬ",{"Наконец-то достойный противник!","Покажи, на что способен!"},2},
    {"РЫСЬ",{"Кто ты такой? Жалкий червь!","Я не буду тратить время.","Тогда, возможно, ты заслужишь право."},3},
    {"САБЛЯ",{"Отшельник ждёт тебя.","Ты не пройдёшь дальше!"},2},
    {"ПУСТЫННИК",{"Пустыня не прощает слабых.","Докажи свою силу!"},2},
    {"КРИС",{"Мой клинок знает твоё имя.","Готовься!"},2},
    {"МРАЧНЫЙ",{"Тьма поглотит тебя.","Ты уже мёртв."},2},
    {"ПЕСЧАНЫЙ",{"Песок скроет твой прах.","Прощай."},2},
    {"ЯРИ",{"Я — страж Отшельника.","Пройдёшь только через мой труп!"},2},
    {"ОТШЕЛЬНИК",{"Ты дошёл до меня.","Впечатляет. Но это ничего не меняет."},2},
};
typedef struct{const char*name;int unlocked;}Achievement;
static Achievement ACHS[8]={
    {"ПЕРВАЯ КРОВЬ",0},{"ПАЛАЧ",0},{"ГРОЗА РЫСЯ",0},{"АКТ I ПРОЙДЕН",0},
    {"ПУСТЫННЫЙ ВОИН",0},{"УБИЙЦА ОТШЕЛЬНИКА",0},{"МАСТЕР",0},{"ЛЕГЕНДА",0},
};

/* ============ СПРАЙТЫ ============ */
static const char* SPR_PLAYER[] = {
    ".....3333.....","....333333....","....344443....","....333333....",".....1111.....",
    "...11111111...","..1111111111..","..1122222211..","..1172222711..","..1172222711..",
    "..1166666611..","...11111111...","...11111111...","....11..11....","....11..11....",
    "....11..11....","....11..11....","...111..111...","...222..222...","...333..333...",
};
static const char* SPR_ENEMY[] = {
    ".....1111.....","....111111....","....144441....","....111111....",".....1111.....",
    "...11111111...","..1111111111..","..1122222211..","..1122222211..","..1111111111..",
    "...11111111...","...11111111...","....111111....","....11..11....","....11..11....",
    "....11..11....","....11..11....","...111..111...","...222..222...","...333..333...",
};
static const char* SPR_BOSS[] = {
    "....111111....","...11111111...","...14444441...","...11111111...","....111111....",
    "..1111111111..",".111111111111.",".112222222211.",".112222222211.",".111111111111.",
    ".111111111111.","..1111111111..","..1111111111..","...111..111...","...11....11...",
    "...11....11...","...11....11...","..111....111..","..222....222..","..333....333..",
};
static const char* SPR_TOUR[] = {
    ".....3333.....","....333333....","....344443....","....333333....",".....1111.....",
    "...11111111...","..1111111111..","..1166666611..","..1166666611..","..1166666611..",
    "..1111111111..","...11111111...","...11111111...","....11..11....","....11..11....",
    "....11..11....","....11..11....","...111..111...","...222..222...","...333..333...",
};
static const char* SPR_KNIFE[] = {"......555555..",".....55555555.","....555555555.","...555555555..","...55555555...","..11111.......","..11111.......","..11111.......","...111........"};
static const char* SPR_FIST[] = {"..55555555..",".5555555555.","55555445555..","5555555555..",".1111111111.",".1111111111."};
static const char* SPR_SCYTHE[] = {"......5555555.","....555555555.","..5555555555..",".55555555.....","5555555.......","55555.........","111...........","1111..........",".1111.........","..1111........"};
static const char* SPR_SWORD[] = {"....555555....","...55555555...","..5555555555..",".55555555555..",".5555555555...",".5555555......","1111111.......","1111111.......","..111.........","..111........."};
static const char* SPR_BATON[] = {"55555555555...","55555555555...","55555555555...","11111.........","11111.........","11111........."};
static const char* SPR_SAI[] = {"....5555......","...5555.......","..5555........","..555555......","..5555........","..5555........","..5555........",".11111........",".11111........"};
static const char* SPR_HAMMER[] = {"....555555....","...55555555...","...55555555...","...55555555...","...111111.....","....1111......","....1111......","....1111......","....1111......","....1111......"};
static const char* SPR_NUNCHAKU[] = {"..5555........","..5555........","...5555.......","....5555......","......4444....","......4444....","....5555......","...5555.......","..5555........","..5555........"};
static const char* SPR_KATANA[] = {"...5555555....","..55555555....",".55555555.....","55555555......","5555555.......","555555........","11111.........","11111.........","..11.........."};
static const char* SPR_TONFA[] = {"....5555......","...5555.......","..5555........","..5555........","..55555555....","..5555....5...","..5555....5...","..5555........",".1111.........",".1111........."};

static unsigned int spriteColor(char c, unsigned int baseColor, int isPlayer, int isBoss){
    if(c=='.') return 0;
    if(c=='1') return baseColor;
    if(c=='2') return RGB(60,30,80);
    if(c=='3') return RGB(220,180,140);
    if(c=='4'){if(isBoss)return RGB(255,204,51);if(isPlayer)return RGB(185,140,255);return RGB(255,136,153);}
    if(c=='5') return RGB(220,220,220);
    if(c=='6') return RGB(180,180,200);
    if(c=='7') return RGB(80,40,120);
    return baseColor;
}
static void drawSprite(const char** sprite,int w,int h,int x,int y,int scale,
                       unsigned int baseColor,int isPlayer,int isBoss,int facing){
    for(int row=0;row<h;row++)for(int col=0;col<w;col++){
        char c=sprite[row][col];if(c=='.')continue;
        unsigned int col2=spriteColor(c,baseColor,isPlayer,isBoss);if(col2==0)continue;
        int drawCol=(facing>0)?col:(w-1-col);
        for(int sy=0;sy<scale;sy++)for(int sx=0;sx<scale;sx++)
            px(x+(drawCol-w/2)*scale+sx,y+(row-h)*scale+sy,col2);
    }
}
static void drawWeaponSprite(int wt,int x,int y,int facing){
    const char** s;int w,h;
    if(wt==0||wt==6){s=SPR_KNIFE;w=14;h=9;}
    else if(wt==1){s=SPR_FIST;w=12;h=6;}
    else if(wt==8||wt==15||wt==5){s=SPR_SCYTHE;w=14;h=10;}
    else if(wt==3||wt==9){s=SPR_BATON;w=14;h=6;}
    else if(wt==2||wt==7){s=SPR_SAI;w=14;h=9;}
    else if(wt==10){s=SPR_NUNCHAKU;w=14;h=10;}
    else if(wt==4){s=SPR_KATANA;w=14;h=9;}
    else if(wt==11||wt==13){s=SPR_HAMMER;w=14;h=10;}
    else if(wt==14){s=SPR_TONFA;w=14;h=10;}
    else {s=SPR_SWORD;w=14;h=10;}
    for(int row=0;row<h;row++)for(int cl=0;cl<w;cl++){
        char c=s[row][cl];if(c=='.')continue;
        unsigned int cc=(c=='5')?RGB(220,220,220):((c=='4')?RGB(255,200,80):RGB(90,74,58));
        int drawCol=(facing>0)?cl:(w-1-cl);
        for(int sy=0;sy<2;sy++)for(int sx=0;sx<2;sx++)
            px(x+(drawCol-w)*2+sx,y+(row-h/2)*2+sy,cc);
    }
}

/* ============ БОЙЦЫ ============ */
typedef struct{
    float x,y,vx,vy;int hp,maxHp;int facing,onGround,blocking,hitDone;
    float attackT,cdT,stunT,hurtT;unsigned int color;int isPlayer,isBoss,isTour;
    int wt,dmg,range;float dur,hs,he,maxCd,speed;float aiT;int aiMode;int cfgIdx;
    int magicCD;
}Fighter;
typedef struct{float x,y;float swing,swingV;float hitFlash;}Bag;
typedef struct{float x,y,vx,vy;float life,maxLife;unsigned int color;int size;}Particle;
static Particle gParts[100];
static int gPartIdx=0;
static void spawnParticle(float x,float y,float vx,float vy,float life,unsigned int col,int size){
    Particle*p=&gParts[gPartIdx];gPartIdx=(gPartIdx+1)%100;
    p->x=x;p->y=y;p->vx=vx;p->vy=vy;p->life=life;p->maxLife=life;p->color=col;p->size=size;
}
typedef struct{float x,y,vx,vy;float life;int dmg;int fromPlayer;unsigned int color;}Projectile;
static Projectile gProj[12];

typedef struct{
    int version;
    int unlockedEnemiesA1, unlockedEnemiesA2;
    int coins,gems;
    int ownedWeapons[17];
    int ownedArmor[6];
    int selectedArmor;
    int ownedHelmets[6];
    int selectedHelmet;
    int unlockedMoves[8];
    int achievements[8];
    int missions[8];
    int missionProgress[8];
    int totalWins;
    int selectedWeapon;
    int volume;
    int difficulty;
    int level;
    int xp;
    int tourProgress;
}SaveData;
static SaveData gSave;

#define ST_MAP        0
#define ST_RADIAL     1
#define ST_CAMPAIGN   2
#define ST_TRAINING   3
#define ST_LEARN      4
#define ST_FIGHT      5
#define ST_END        6
#define ST_INVENTORY  7
#define ST_DIALOG     8
#define ST_CHARACTER  9
#define ST_ACHIEVE    10
#define ST_SETTINGS   11
#define ST_MISSIONS   12
#define ST_TOURNAMENT 13

static int gState=ST_MAP,gMode=0,gAct=1;
static int gWeapon=0,gEnemyIdx=0,gLocIdx=0;
static Fighter gPlayer,gEnemy;
static Bag gBag;static int gBagActive=0;
static float gMsgT=0;static char gMsg[128];
static float gShakeT=0;
static int gTransition=0,gPlayerWon=0;
static int gComboCount=0;static float gComboT=0;
static int gDialogIdx=0,gDialogLine=0;
static int gReturnTimer=0;
static int gInvFromState=ST_MAP;
static int gSettingsIdx=0;
static int gMapCursorX=0,gMapCursorY=0;
static int gRadialCursor=0;
static int gShopMode=0;
static int gInvTab=0;
static int gAchPopup=-1;static float gAchPopupT=0;
static int gTourCursor=0;
static int gMenuOpen=0;
static int gMenuCursor=0;
static int gWasCross=0,gWasLeft=0,gWasRight=0,gWasUp=0,gWasDown=0;
static int gWasCircle=0,gWasSquare=0,gWasTriangle=0,gWasSelect=0,gWasL=0,gWasR=0,gWasStart=0;
static float gAttackChainT=0;
static int gAttackChain=0;
static unsigned int gLastUs=0;

static float dt(void){
    unsigned int now=sceKernelGetSystemTimeLow();
    float d=(now-gLastUs)/1000000.0f;gLastUs=now;
    if(d>0.05f)d=0.05f;if(d<0)d=0;return d;
}
static void setMsg(const char*t,float time){strncpy(gMsg,t,sizeof(gMsg)-1);gMsg[sizeof(gMsg)-1]=0;gMsgT=time;}

static int xpForLevel(int lvl){return 100+lvl*80;}
static void saveGame(void);
static void addXP(int amount){
    gSave.xp+=amount;
    while(gSave.xp>=xpForLevel(gSave.level)){
        gSave.xp-=xpForLevel(gSave.level);
        gSave.level++;
        if(gSave.level<=8)gSave.unlockedMoves[gSave.level-1]=1;
        sfxLevel();
        saveGame();
    }
}
static void saveDefaults(void){
    memset(&gSave,0,sizeof(gSave));
    gSave.version=5;
    gSave.unlockedEnemiesA1=1;
    gSave.ownedWeapons[0]=1;gSave.ownedWeapons[1]=1;
    gSave.ownedArmor[0]=1;
    gSave.ownedHelmets[0]=1;
    gSave.unlockedMoves[0]=1;
    gSave.volume=70;
    gSave.difficulty=1;
    gSave.level=1;
}
static void saveGame(void){
    int fd=sceIoOpen("ms0:/PSP/SAVEDATA/ShadowFightLite/save.dat",PSP_O_CREAT|PSP_O_WRONLY|PSP_O_TRUNC,0777);
    if(fd<0)return;sceIoWrite(fd,&gSave,sizeof(gSave));sceIoClose(fd);
}
static void loadGame(void){
    saveDefaults();
    int fd=sceIoOpen("ms0:/PSP/SAVEDATA/ShadowFightLite/save.dat",PSP_O_RDONLY,0777);
    if(fd<0)return;
    int r=sceIoRead(fd,&gSave,sizeof(gSave));
    if(r!=(int)sizeof(gSave)||gSave.version!=5)saveDefaults();
    sceIoClose(fd);
}
static void unlockAch(int i){
    if(i<0||i>=8)return;if(gSave.achievements[i])return;
    gSave.achievements[i]=1;gAchPopup=i;gAchPopupT=2.2f;
    sfxAch();saveGame();
}
static void addMissionProgress(int idx,int amount){
    if(idx<0||idx>=8)return;if(gSave.missions[idx]==2)return;
    gSave.missions[idx]=1;gSave.missionProgress[idx]+=amount;
    if(gSave.missionProgress[idx]>=MISSIONS[idx].target){
        gSave.missionProgress[idx]=MISSIONS[idx].target;
        gSave.missions[idx]=2;gSave.gems+=MISSIONS[idx].reward;
        setMsg("МИССИЯ ВЫПОЛНЕНА!",2.0f);sfxGem();
    }
    saveGame();
}
static const DiffDef* curDiff(void){return &DIFFS[gSave.difficulty];}
static int getArmorDefense(void){
    return ARMORS[gSave.selectedArmor].defense + HELMETS[gSave.selectedHelmet].defense;
}
static void initPlayer(void){
    memset(&gPlayer,0,sizeof(gPlayer));
    const WeaponDef*w=&WEAPONS[gWeapon];
    gPlayer.isPlayer=1;gPlayer.hp=gPlayer.maxHp=100;
    gPlayer.x=SW*0.25f;gPlayer.y=GY;gPlayer.facing=1;gPlayer.onGround=1;
    gPlayer.color=RGB(26,10,42);
    gPlayer.wt=w->type;gPlayer.dmg=w->dmg;gPlayer.range=w->range;
    gPlayer.dur=w->dur;gPlayer.hs=w->hs;gPlayer.he=w->he;gPlayer.maxCd=w->cd;
    gPlayer.magicCD=180;
}
static void initEnemy(int idx){
    memset(&gEnemy,0,sizeof(gEnemy));
    const EnemyDef*e=&ENEMIES[idx];const DiffDef*d=curDiff();
    gEnemy.hp=gEnemy.maxHp=(int)(e->hp*d->hpMul);
    gEnemy.x=SW*0.75f;gEnemy.y=GY;gEnemy.facing=-1;gEnemy.onGround=1;
    gEnemy.color=e->color;gEnemy.isBoss=e->boss;
    gEnemy.wt=e->wt;gEnemy.dmg=(int)(e->dmg*d->dmgMul);gEnemy.range=e->range;
    gEnemy.dur=0.32f;gEnemy.hs=0.09f;gEnemy.he=0.20f;gEnemy.maxCd=e->cd;
    gEnemy.speed=e->speed*d->spdMul;gEnemy.cfgIdx=idx;gEnemy.magicCD=200;
}
static void initTourFighter(int idx){
    memset(&gEnemy,0,sizeof(gEnemy));
    const TourFighter*t=&TOUR[idx];const DiffDef*d=curDiff();
    gEnemy.hp=gEnemy.maxHp=(int)(t->hp*d->hpMul);
    gEnemy.x=SW*0.75f;gEnemy.y=GY;gEnemy.facing=-1;gEnemy.onGround=1;
    gEnemy.color=t->color;gEnemy.isTour=1;
    gEnemy.wt=t->wt;gEnemy.dmg=(int)(t->dmg*d->dmgMul);
    const WeaponDef*w=&WEAPONS[0];
    for(int i=0;i<17;i++)if(WEAPONS[i].type==t->wt){w=&WEAPONS[i];break;}
    gEnemy.range=w->range;gEnemy.dur=w->dur;gEnemy.hs=w->hs;gEnemy.he=w->he;
    gEnemy.maxCd=w->cd*1.1f;gEnemy.speed=t->speed*d->spdMul;gEnemy.cfgIdx=idx;gEnemy.magicCD=200;
}
static void initBag(void){memset(&gBag,0,sizeof(gBag));gBag.x=SW*0.7f;gBag.y=GY;gBagActive=1;}
static void startBattle(int idx){
    gEnemyIdx=idx;initPlayer();initEnemy(idx);gBagActive=0;
    gTransition=0;gComboCount=0;gComboT=0;
    gState=ST_FIGHT;gMode=0;
    char b[64];snprintf(b,sizeof(b),"РАУНД\n%s",ENEMIES[idx].name);
    setMsg(b,1.5f);musicSet(2);sndPlay(440,150,1,50);
}
static void startTraining(int loc){
    gLocIdx=loc;initPlayer();initBag();
    gTransition=0;gComboCount=0;gComboT=0;gState=ST_FIGHT;gMode=1;
    setMsg(LOCS[loc].name,1.4f);musicSet(2);
}
static void startLearn(int id){
    gLocIdx=id;initPlayer();initBag();
    gTransition=0;gComboCount=0;gComboT=0;gState=ST_FIGHT;gMode=2;
    setMsg(MOVE_UNLOCKS[id].name,1.4f);musicSet(2);
}
static void startTournament(int idx){
    gEnemyIdx=idx;initPlayer();initTourFighter(idx);gBagActive=0;
    gTransition=0;gComboCount=0;gComboT=0;gState=ST_FIGHT;gMode=5;
    char b[64];snprintf(b,sizeof(b),"ТУРНИР %d/24\n%s",idx+1,TOUR[idx].name);
    setMsg(b,1.6f);musicSet(2);sndPlay(440,150,1,50);
}
static void startDialog(int idx){
    gDialogIdx=idx;gDialogLine=0;
    if(DIALOGS[idx].count==0){startBattle(idx);return;}
    gState=ST_DIALOG;musicSet(1);
}
static void physics(Fighter*f,float d){
    f->vy+=1800.0f*d;f->x+=f->vx*d;f->y+=f->vy*d;
    if(f->y>=GY){f->y=GY;f->vy=0;f->onGround=1;}else f->onGround=0;
    if(f->x<28){f->x=28;if(f->vx<0)f->vx=0;}
    if(f->x>SW-28){f->x=SW-28;if(f->vx>0)f->vx=0;}
    if(f->onGround&&f->stunT<=0)f->vx*=0.85f;
}
static void spawnHitParticles(float x,float y,unsigned int col,int count){
    for(int i=0;i<count;i++){
        float a=(rand()%360)*3.14159f/180.0f;
        float sp=60+rand()%180;
        spawnParticle(x,y,cosf(a)*sp,sinf(a)*sp-60,0.5f,col,2+rand()%2);
    }
}
static void applyHit(Fighter*a,Fighter*b,int dmgOverride,int isHeavy,int isUlt){
    float dmg=(float)(dmgOverride>0?dmgOverride:a->dmg);
    if(isHeavy)dmg*=1.4f;if(isUlt)dmg*=2.0f;
    if(!a->isPlayer&&b->isPlayer){dmg-=getArmorDefense();if(dmg<1)dmg=1;}
    int crit=0;if((rand()%100)<15){dmg*=1.8f;crit=1;}
    if(isUlt)crit=1;
    if(b->blocking){dmg*=0.22f;b->vx=a->facing*80;b->stunT=0.10f;sfxBlock();
        spawnHitParticles(b->x,b->y-55,RGB(200,200,255),6);}
    else{b->vx=a->facing*190;b->stunT=isUlt?0.5f:0.26f;b->hurtT=0.22f;
        if(crit||isUlt){sfxCrit();spawnHitParticles(b->x,b->y-55,RGB(255,80,80),isUlt?24:14);}
        else if(dmg>=10){sfxHeavy();spawnHitParticles(b->x,b->y-55,RGB(255,200,80),10);}
        else{sfxHit();spawnHitParticles(b->x,b->y-55,RGB(255,255,200),8);}}
    b->hp-=(int)dmg;if(b->hp<0)b->hp=0;
    gShakeT=crit?0.25f:0.14f;gComboCount++;gComboT=1.6f;
}
static void checkHit(Fighter*a,Fighter*b){
    if(a->attackT<=0||a->hitDone)return;
    float el=a->dur-a->attackT;
    if(el<a->hs||el>a->he)return;
    float dx=b->x-a->x;
    if(fabsf(dx)>a->range)return;
    if((dx>0?1:-1)!=a->facing&&fabsf(dx)>8)return;
    if(fabsf(a->y-b->y)>80)return;
    a->hitDone=1;applyHit(a,b,0,0,0);
}
static void checkHitBag(Fighter*a){
    if(!gBagActive||a->attackT<=0||a->hitDone)return;
    float el=a->dur-a->attackT;
    if(el<a->hs||el>a->he)return;
    float dx=gBag.x-a->x;
    if(fabsf(dx)>a->range+10)return;
    if((dx>0?1:-1)!=a->facing&&fabsf(dx)>8)return;
    a->hitDone=1;gBag.swingV+=a->facing*3.5f;gBag.hitFlash=1;
    gComboCount++;gComboT=1.6f;sfxHit();
    spawnHitParticles(gBag.x,gBag.y-100,RGB(255,200,80),6);
}
static void spawnProjectile(float x,float y,int fromPlayer,int type){
    for(int i=0;i<12;i++)if(gProj[i].life<=0){
        gProj[i].x=x;gProj[i].y=y;gProj[i].vx=(fromPlayer?1:-1)*520;gProj[i].vy=0;gProj[i].life=1.2f;
        gProj[i].dmg=(type==0)?6:10;gProj[i].fromPlayer=fromPlayer;
        gProj[i].color=(type==0)?RGB(255,120,80):RGB(180,120,255);return;}
}
static void updateProjectiles(float d){
    for(int i=0;i<12;i++){
        if(gProj[i].life<=0)continue;
        gProj[i].x+=gProj[i].vx*d;gProj[i].y+=gProj[i].vy*d;gProj[i].life-=d;
        if(gProj[i].fromPlayer&&!gBagActive&&gEnemy.hp>0){
            if(fabsf(gProj[i].x-gEnemy.x)<20&&fabsf(gProj[i].y-(gEnemy.y-55))<50){
                applyHit(&gPlayer,&gEnemy,gProj[i].dmg,0,0);
                spawnHitParticles(gProj[i].x,gProj[i].y,gProj[i].color,10);
                gProj[i].life=0;sfxMagic();}}
        else if(!gProj[i].fromPlayer&&gPlayer.hp>0){
            if(fabsf(gProj[i].x-gPlayer.x)<20&&fabsf(gProj[i].y-(gPlayer.y-55))<50){
                applyHit(&gEnemy,&gPlayer,gProj[i].dmg,0,0);
                spawnHitParticles(gProj[i].x,gProj[i].y,gProj[i].color,10);
                gProj[i].life=0;sfxMagic();}}
        if(gProj[i].x<-10||gProj[i].x>SW+10)gProj[i].life=0;
    }
}
static void updateParticles(float d){
    for(int i=0;i<100;i++){
        if(gParts[i].life<=0)continue;
        gParts[i].x+=gParts[i].vx*d;gParts[i].y+=gParts[i].vy*d;
        gParts[i].vy+=400*d;gParts[i].life-=d;
    }
}
static void enemyAI(float d){
    Fighter*e=&gEnemy;Fighter*p=&gPlayer;
    if(e->stunT>0)return;
    float dx=p->x-e->x,dist=fabsf(dx);int dir=dx>0?1:-1;
    e->aiT-=d;
    if(e->isBoss&&dist>140){e->magicCD-=(int)(d*1000);
        if(e->magicCD<=0){e->magicCD=4000;spawnProjectile(e->x+dir*20,e->y-55,0,1);sfxMagic();}}
    if(e->aiT<=0){
        e->aiT=e->isBoss?(0.18f+(rand()%25)/100.0f):(0.28f+(rand()%40)/100.0f);
        if(dist<=e->range*0.92f&&e->cdT<=0&&(rand()%100)<(e->isBoss?88:72))e->aiMode=2;
        else if(dist>e->range*1.05f)e->aiMode=0;
        else{int r=rand()%100;if(r<22)e->aiMode=1;else if(r<45)e->aiMode=3;else e->aiMode=0;}}
    e->blocking=0;
    if(e->aiMode==0)e->vx=dir*e->speed;
    else if(e->aiMode==2){e->vx*=0.7f;
        if(e->cdT<=0&&dist<=e->range){e->attackT=e->dur;e->cdT=e->maxCd;e->hitDone=0;sfxWhoosh();}}
    else if(e->aiMode==1){e->blocking=1;e->vx=-dir*40;}
    else e->vx*=0.85f;
}
static void update(float d){
    if(gState!=ST_FIGHT)return;
    Fighter*p=&gPlayer;
    if(gBagActive)p->facing=(gBag.x>=p->x)?1:-1;
    else if(gEnemy.hp>0){
        if(p->attackT<=0&&p->stunT<=0)p->facing=(gEnemy.x>=p->x)?1:-1;
        if(gEnemy.attackT<=0&&gEnemy.stunT<=0)gEnemy.facing=(p->x>=gEnemy.x)?1:-1;}
    p->cdT-=d;if(p->cdT<0)p->cdT=0;
    p->stunT-=d;if(p->stunT<0)p->stunT=0;
    p->hurtT-=d;if(p->hurtT<0)p->hurtT=0;
    if(p->attackT>0){p->attackT-=d;if(p->attackT<0)p->attackT=0;}
    if(p->stunT>0)p->vx*=0.9f;
    else if(p->attackT>0)p->vx*=0.84f;
    else if(gWasSquare){p->vx*=0.7f;p->blocking=1;}
    else{p->blocking=0;float ax=0;if(gWasLeft)ax-=1;if(gWasRight)ax+=1;p->vx=ax*280;}
    if(!gBagActive){
        Fighter*e=&gEnemy;
        e->cdT-=d;if(e->cdT<0)e->cdT=0;
        e->stunT-=d;if(e->stunT<0)e->stunT=0;
        e->hurtT-=d;if(e->hurtT<0)e->hurtT=0;
        if(e->attackT>0){e->attackT-=d;if(e->attackT<0)e->attackT=0;}
        enemyAI(d);physics(e,d);}
    physics(p,d);
    if(gBagActive){
        gBag.swingV*=0.94f;gBag.swing+=gBag.swingV*d;gBag.swing*=0.985f;
        if(gBag.swing>0.9f)gBag.swing=0.9f;if(gBag.swing<-0.9f)gBag.swing=-0.9f;
        gBag.hitFlash-=d*3;if(gBag.hitFlash<0)gBag.hitFlash=0;
        checkHitBag(p);}
    else{checkHit(p,&gEnemy);checkHit(&gEnemy,p);}
    updateProjectiles(d);updateParticles(d);
    if(gShakeT>0)gShakeT-=d;
    if(gMsgT>0)gMsgT-=d;
    if(gComboT>0){gComboT-=d;if(gComboT<=0)gComboCount=0;}
    if(gAttackChainT>0){gAttackChainT-=d;if(gAttackChainT<=0)gAttackChain=0;}
    if(gMode==2&&gLocIdx>=0&&!gSave.unlockedMoves[gLocIdx]){
        int done=0;
        if(gLocIdx==0&&gWasSquare)done=1;
        if(gLocIdx==1&&gWasDown&&p->attackT>0)done=1;
        if(gLocIdx==2&&gAttackChain>=2)done=1;
        if(gLocIdx==3&&!p->onGround&&p->attackT>0)done=1;
        if(gLocIdx==4&&(gWasRight||gWasLeft)&&p->attackT>0)done=1;
        if(gLocIdx==5&&gProj[0].life>0)done=1;
        if(gLocIdx==6&&gWasLeft&&p->attackT>0)done=1;
        if(gLocIdx==7&&gAttackChain>=3)done=1;
        if(done){gSave.unlockedMoves[gLocIdx]=1;
            setMsg("ИЗУЧЕНО!",1.2f);sfxWin();saveGame();gReturnTimer=1500;}}
    if(!gTransition&&!gBagActive){
        if(gEnemy.hp<=0){
            gTransition=1;int xpGain=0,coinGain=0;
            if(gMode==5){xpGain=30+gEnemyIdx*5;coinGain=30+gEnemyIdx*10;
                if(gSave.tourProgress==gEnemyIdx)gSave.tourProgress=gEnemyIdx+1;
                setMsg("ПОБЕДА!",1.2f);}
            else{xpGain=20+gEnemyIdx*8;coinGain=20+ENEMIES[gEnemyIdx].hp/2;
                gSave.totalWins++;
                addMissionProgress(0,1);addMissionProgress(3,1);addMissionProgress(4,1);
                if(gSave.totalWins>=1)unlockAch(0);
                if(gSave.totalWins>=3)unlockAch(1);
                if(gSave.totalWins>=20)unlockAch(7);
                if(ENEMIES[gEnemyIdx].boss&&ENEMIES[gEnemyIdx].act==1){unlockAch(2);unlockAch(3);gSave.unlockedEnemiesA2=1;addMissionProgress(1,1);}
                if(ENEMIES[gEnemyIdx].boss&&ENEMIES[gEnemyIdx].act==2){unlockAch(5);addMissionProgress(2,1);}
                if(ENEMIES[gEnemyIdx].act==2)unlockAch(4);
                if(ENEMIES[gEnemyIdx].act==1&&gEnemyIdx<5){if(gSave.unlockedEnemiesA1<gEnemyIdx+2)gSave.unlockedEnemiesA1=gEnemyIdx+2;}
                else if(ENEMIES[gEnemyIdx].act==2&&gEnemyIdx<12){if(gSave.unlockedEnemiesA2<gEnemyIdx-4)gSave.unlockedEnemiesA2=gEnemyIdx-4;}
                if(gEnemyIdx==5)setMsg("АКТ I ПРОЙДЕН!",1.5f);
                else if(gEnemyIdx==12){setMsg("ОТШЕЛЬНИК ПОВЕРЖЕН!",1.5f);gPlayerWon=1;}
                else setMsg("ПОБЕДА!",1.2f);}
            gSave.coins+=coinGain;addXP(xpGain);
            saveGame();sfxWin();sfxCoin();gReturnTimer=1600;}
        else if(p->hp<=0){gTransition=1;setMsg("ПОРАЖЕНИЕ",1.4f);sfxLose();gReturnTimer=1500;}}
    if(gReturnTimer>0){
        gReturnTimer-=(int)(d*1000);
        if(gReturnTimer<=0){
            if(gMode==0)gState=ST_CAMPAIGN;
            else if(gMode==1)gState=ST_TRAINING;
            else if(gMode==2)gState=ST_LEARN;
            else if(gMode==5)gState=ST_TOURNAMENT;
            else gState=ST_MAP;
            musicSet(1);gTransition=0;}}
}

/* ============ ОТРИСОВКА БОЯ ============ */
static void drawFighter(Fighter*f){
    if(!f)return;
    int dir=f->facing;
    unsigned int col=f->isPlayer?RGB(26,10,42):(f->hurtT>0?RGB(255,91,91):f->color);
    float bx=f->x,by=f->y;
    for(int i=-22;i<=22;i++)px((int)bx+i,(int)by+2,RGB(20,20,20));
    if(f->isPlayer){for(int i=-30;i<=30;i++)for(int j=-50;j<=50;j++){
        float dd=sqrtf((float)(i*i+j*j));if(dd<32&&dd>26)px((int)bx+i,(int)(by-60+j),RGB(60,20,100));}}
    if(f->isBoss){for(int i=-30;i<=30;i++)for(int j=-50;j<=50;j++){
        float dd=sqrtf((float)(i*i+j*j));if(dd<32&&dd>26)px((int)bx+i,(int)(by-60+j),RGB(120,50,10));}}
    int lean=0;
    if(f->attackT>0){float prog=1.0f-(f->attackT/f->dur);
        lean=(int)(sinf(prog*3.14159f)*6*dir);}
    const char** sprite;
    if(f->isPlayer)sprite=SPR_PLAYER;
    else if(f->isBoss)sprite=SPR_BOSS;
    else if(f->isTour)sprite=SPR_TOUR;
    else sprite=SPR_ENEMY;
    drawSprite(sprite,14,20,(int)(bx+lean),(int)by,2,col,f->isPlayer,f->isBoss,dir);
    if(f->isPlayer&&gSave.selectedArmor>0){
        unsigned int ac=ARMORS[gSave.selectedArmor].color;
        for(int i=-13;i<=13;i++){px((int)(bx+lean)+i,(int)by-34,ac);px((int)(bx+lean)+i,(int)by-30,ac);px((int)(bx+lean)+i,(int)by-26,ac);}}
    if(f->isPlayer&&gSave.selectedHelmet>0){
        unsigned int hc=HELMETS[gSave.selectedHelmet].color;
        for(int i=-8;i<=8;i++){
            if(i*i+(int)(3*3)<64) px((int)(bx+lean)+i,(int)by-76,hc);
        }
    }
    float handX=bx+12*dir+lean;float handY=by-45;
    if(f->attackT>0){float prog=1.0f-(f->attackT/f->dur);
        float swing=sinf(prog*3.14159f);if(swing<0)swing=0;if(swing>1)swing=1;
        handX=bx+(10+f->range*0.7f*swing)*dir;handY=by-50+6*(1-swing);}
    drawWeaponSprite(f->wt,(int)handX,(int)handY,dir);
    if(f->hurtT>0)circle((int)bx,(int)(by-50),22,RGB(255,255,255));
}
static void drawBag(void){
    if(!gBagActive)return;
    float bx=gBag.x,by=gBag.y;
    for(int i=-20;i<=20;i++)px((int)bx+i,(int)by+2,RGB(20,20,20));
    line((int)bx,(int)(by-160),(int)bx,(int)(by-130+gBag.swing*20),RGB(100,100,100),2);
    float off=gBag.swing*30;float cy=by-100;
    unsigned int body=gBag.hitFlash>0?RGB(255,85,102):RGB(90,42,42);
    unsigned int edge=gBag.hitFlash>0?RGB(255,170,136):RGB(138,58,58);
    for(int j=-70;j<=70;j++){
        int w=(int)(28*(1-fabsf(j)/80.0f));if(w<2)w=2;
        for(int i=-w;i<=w;i++)px((int)(bx+off*(j+70)/140)+i,(int)(cy+j),body);}
    line((int)(bx+off),(int)(cy-70),(int)(bx+off),(int)(cy+70),edge,2);
    line((int)(bx-28),(int)cy,(int)(bx+28),(int)cy,edge,2);
    circle((int)(bx+off),(int)(cy-70),6,RGB(58,26,26));
}
static void drawParticles(void){
    for(int i=0;i<100;i++){
        if(gParts[i].life<=0)continue;
        float a=gParts[i].life/gParts[i].maxLife;
        unsigned int c=gParts[i].color;
        int r=(int)((c>>16&0xFF)*a),g=(int)((c>>8&0xFF)*a),b=(int)((c&0xFF)*a);
        int s=gParts[i].size;
        for(int dy=-s;dy<=s;dy++)for(int dx=-s;dx<=s;dx++)
            px((int)gParts[i].x+dx,(int)gParts[i].y+dy,RGB(r,g,b));}
}
static void drawProjectiles(void){
    for(int i=0;i<12;i++){
        if(gProj[i].life<=0)continue;
        circle((int)gProj[i].x,(int)gProj[i].y,6,gProj[i].color);
        circle((int)gProj[i].x,(int)gProj[i].y,3,RGB(255,255,255));}
}
static void drawBackground(void){
    const LocDef*loc;
    LocDef bLoc={"",RGB(10,13,26),RGB(26,16,48),RGB(44,16,56),RGB(20,12,34),RGB(12,7,20),RGB(123,63,255)};
    if(gMode==1||gMode==2)loc=&LOCS[gLocIdx];else loc=&bLoc;
    for(int y=0;y<GY;y++){
        float t=(float)y/GY;unsigned int c;
        if(t<0.5f){float u=t*2;
            int r=(int)((loc->skyTop>>16&0xFF)*(1-u)+(loc->skyMid>>16&0xFF)*u);
            int g=(int)((loc->skyTop>>8&0xFF)*(1-u)+(loc->skyMid>>8&0xFF)*u);
            int b=(int)((loc->skyTop&0xFF)*(1-u)+(loc->skyMid&0xFF)*u);c=RGB(r,g,b);}
        else{float u=(t-0.5f)*2;
            int r=(int)((loc->skyMid>>16&0xFF)*(1-u)+(loc->skyBot>>16&0xFF)*u);
            int g=(int)((loc->skyMid>>8&0xFF)*(1-u)+(loc->skyBot>>8&0xFF)*u);
            int b=(int)((loc->skyMid&0xFF)*(1-u)+(loc->skyBot&0xFF)*u);c=RGB(r,g,b);}
        unsigned int*row=&fb[y*BW];for(int x=0;x<SW;x++)row[x]=c;}
    int mx=SW-100,my=45;
    for(int y=-40;y<=40;y++)for(int x=-40;x<=40;x++){
        float d2=(float)(x*x+y*y);
        if(d2<1600){float a=1-sqrtf(d2)/40;if(a<0)a=0;
            int base=fb[(my+y)*BW+mx+x]&0xFFFFFF;
            int br=(base>>16)&0xFF,bg=(base>>8)&0xFF,bb=base&0xFF;
            fb[(my+y)*BW+mx+x]=RGB((int)(br+(255-br)*a*0.35f),(int)(bg+(235-bg)*a*0.35f),(int)(bb+(190-bb)*a*0.35f));}}
    circle(mx,my,20,RGB(255,242,205));
    int hts[]={60,40,55,30,50,45,58,38};int step=SW/7;
    for(int i=0;i<8;i++)fillRect(i*step-step/2,GY-hts[i],step+2,hts[i],loc->wall);
    fillRect(0,GY-3,SW,3,loc->accent);
    fillRect(0,GY,SW,SH-GY,loc->ground);
}

/* ============ HUD БОЯ — ПОРТРЕТЫ ============ */
static void drawPortrait(int cx,int cy,int r,int isPlayer,int isBoss,int isTour){
    for(int a=0;a<360;a+=2){float rad=a*3.14159f/180.0f;
        for(int rr=r-2;rr<=r+1;rr++) px(cx+(int)(cosf(rad)*rr),cy+(int)(sinf(rad)*rr),RGB(25,15,10));}
    for(int a=0;a<360;a+=2){float rad=a*3.14159f/180.0f;int rr=r-3;
        px(cx+(int)(cosf(rad)*rr),cy+(int)(sinf(rad)*rr),RGB(200,180,140));
        px(cx+(int)(cosf(rad)*(rr-1)),cy+(int)(sinf(rad)*(rr-1)),RGB(160,140,100));}
    circle(cx,cy,r-4,RGB(170,190,175));
    circle(cx,cy,r-5,RGB(180,200,185));
    circle(cx,cy-r/3+1,r/3,RGB(20,15,25));
    for(int y=cy-r/3+r/3;y<cy+r-3;y++){
        int rel=y-(cy-r/3+r/3);int bw=r/3+1+rel/2;
        if(bw>r-6)bw=r-6;if(bw<3)bw=3;
        for(int x=-bw;x<=bw;x++) px(cx+x,y,RGB(20,15,25));}
    unsigned int eye;
    if(isBoss)eye=RGB(255,204,51);
    else if(isPlayer)eye=RGB(185,140,255);
    else if(isTour)eye=RGB(255,150,80);
    else eye=RGB(255,136,153);
    int ey=cy-r/3-1;
    px(cx-4,ey,eye);px(cx-3,ey,eye);px(cx+4,ey,eye);px(cx+3,ey,eye);
    px(cx-3,ey-1,eye);px(cx+4,ey-1,eye);
}
static void drawHUD(void){
    if(gState!=ST_FIGHT)return;
    int pr=24,ppy=32,ppx=34,epx=SW-34;
    drawPortrait(ppx,ppy,pr,1,0,0);
    if(!gBagActive) drawPortrait(epx,ppy,pr,0,gEnemy.isBoss,gEnemy.isTour);
    int barH=10,barY=ppy-5;
    int leftStart=ppx+pr+6,leftEnd=SW/2-42;
    int rightStart=SW/2+42,rightEnd=epx-pr-6;
    int playerBW=leftEnd-leftStart;
    if(playerBW>0){
        fillRect(leftStart-2,barY-2,playerBW+4,barH+4,RGB(30,15,8));
        fillRect(leftStart-1,barY-1,playerBW+2,barH+2,RGB(90,50,25));
        fillRect(leftStart,barY,playerBW,barH,RGB(40,20,10));
        int pf=(int)(playerBW*(float)gPlayer.hp/gPlayer.maxHp);
        for(int x=0;x<pf;x++){int gr=255-x*70/playerBW;if(gr<160)gr=160;
            fillRect(leftStart+x,barY,1,barH,RGB(255,gr,40));}
        drawText(leftStart+3,barY+2,"ТЕНЬ",RGB(255,255,255),1);
        char hpTxt[8];snprintf(hpTxt,sizeof(hpTxt),"%d",gPlayer.hp);
        int hpW=textW(hpTxt,1);
        drawText(leftEnd-2-hpW,barY+2,hpTxt,RGB(255,255,255),1);
    }
    if(!gBagActive){
        int eBW=rightEnd-rightStart;
        if(eBW>0){
            fillRect(rightStart-2,barY-2,eBW+4,barH+4,RGB(30,15,8));
            fillRect(rightStart-1,barY-1,eBW+2,barH+2,RGB(90,50,25));
            fillRect(rightStart,barY,eBW,barH,RGB(40,20,10));
            int ef=(int)(eBW*(float)gEnemy.hp/gEnemy.maxHp);
            for(int x=0;x<ef;x++){int gr=255-x*70/eBW;if(gr<160)gr=160;
                fillRect(rightEnd-x-1,barY,1,barH,RGB(255,gr,40));}
            const char*en=gEnemy.isTour?TOUR[gEnemyIdx].name:ENEMIES[gEnemyIdx].name;
            int enW=textW(en,1);
            drawText(rightEnd-2-enW,barY+2,en,RGB(255,255,255),1);
            char ehTxt[8];snprintf(ehTxt,sizeof(ehTxt),"%d",gEnemy.hp);
            drawText(rightStart+3,barY+2,ehTxt,RGB(255,255,255),1);
        }
    }
    int ctx=SW/2,cty=ppy,cr=13;
    circle(ctx,cty+2,cr,RGB(20,10,5));
    circle(ctx,cty,cr,RGB(140,110,70));
    circle(ctx,cty,cr-1,RGB(80,60,40));
    circle(ctx,cty,cr-2,RGB(120,95,65));
    circle(ctx,cty,cr-4,RGB(60,45,30));
    fillRect(ctx-3,cty-5,2,10,RGB(230,220,200));
    fillRect(ctx+1,cty-5,2,10,RGB(230,220,200));
    char numTxt[16];snprintf(numTxt,sizeof(numTxt),"%d",gSave.level);
    int nw=textW(numTxt,2);
    drawText(ctx-nw/2,cty+cr+2,numTxt,RGB(255,240,200),2);
    if(gComboCount>1){char cb[16];snprintf(cb,sizeof(cb),"КОМБО x%d",gComboCount);
        int cw=textW(cb,2);drawText((SW-cw)/2,90,cb,RGB(255,200,60),2);}
    if(gMsgT>0){drawTextC(SH*0.42f+2,gMsg,RGB(0,0,0),2);
        drawTextC(SH*0.42f,gMsg,RGB(240,230,255),2);}
}

/* ============ HUD ВЕРХ + МЕНЮ ============ */
static void drawStar(int cx, int cy, int r, unsigned int col){
    int ax[10], ay[10];
    float a0 = -3.14159f / 2.0f;
    for (int i = 0; i < 10; i++){
        float rr = (i % 2 == 0) ? r : (r * 0.45f);
        float a = a0 + i * 3.14159f / 5.0f;
        ax[i] = cx + (int)(cosf(a) * rr);
        ay[i] = cy + (int)(sinf(a) * rr);
    }
    for (int i = 0; i < 10; i++)
        line(ax[i], ay[i], ax[(i+1)%10], ay[(i+1)%10], col, 2);
}
static void drawTopHUD(void){
    int sy = 11;
    fillRect(0, 0, SW, 22, RGB(75, 38, 22));
    fillRect(0, 20, SW, 2, RGB(40, 20, 10));
    fillRect(0, 22, SW, 2, RGB(120, 80, 45));
    drawStar(30, sy, 9, RGB(255, 210, 60));
    drawStar(30, sy, 6, RGB(255, 240, 140));
    char lv[8]; snprintf(lv, sizeof(lv), "%d", gSave.level);
    drawText(46, sy - 4, lv, RGB(255, 255, 255), 1);
    int xpX = 65, xpW = 62;
    fillRect(xpX, sy - 5, xpW, 10, RGB(35, 18, 10));
    int xpFill = (int)(xpW * (float)gSave.xp / xpForLevel(gSave.level));
    if (xpFill < 0) xpFill = 0; if (xpFill > xpW) xpFill = xpW;
    fillRect(xpX, sy - 5, xpFill, 10, RGB(255, 130, 45));
    for (int i = 1; i < 4; i++) fillRect(xpX + xpW * i / 4, sy - 5, 1, 10, RGB(75, 38, 22));
    fillRect(xpX - 1, sy - 6, xpW + 2, 1, RGB(150, 100, 60));
    fillRect(xpX - 1, sy + 5, xpW + 2, 1, RGB(150, 100, 60));
    fillRect(xpX - 1, sy - 6, 1, 12, RGB(150, 100, 60));
    fillRect(xpX + xpW, sy - 6, 1, 12, RGB(150, 100, 60));
    int ex = 150;
    line(ex - 3, sy - 7, ex + 2, sy - 1, RGB(255, 235, 90), 2);
    line(ex + 2, sy - 1, ex - 2, sy + 1, RGB(255, 235, 90), 2);
    line(ex - 2, sy + 1, ex + 3, sy + 7, RGB(255, 235, 90), 2);
    int enX = 168, enW = 62;
    fillRect(enX, sy - 5, enW, 10, RGB(35, 18, 10));
    int enFill = (int)(enW * 0.6f);
    fillRect(enX, sy - 5, enFill, 10, RGB(255, 130, 45));
    for (int i = 1; i < 5; i++) fillRect(enX + enW * i / 5, sy - 5, 1, 10, RGB(75, 38, 22));
    fillRect(enX - 1, sy - 6, enW + 2, 1, RGB(150, 100, 60));
    fillRect(enX - 1, sy + 5, enW + 2, 1, RGB(150, 100, 60));
    fillRect(enX - 1, sy - 6, 1, 12, RGB(150, 100, 60));
    fillRect(enX + enW, sy - 6, 1, 12, RGB(150, 100, 60));
    int cx = 255;
    circle(cx, sy, 8, RGB(200, 170, 60));
    circle(cx, sy, 6, RGB(240, 210, 90));
    circle(cx, sy, 4, RGB(255, 235, 140));
    char cb[16]; snprintf(cb, sizeof(cb), "%d", gSave.coins);
    drawText(cx + 13, sy - 4, cb, RGB(255, 255, 255), 1);
    int gx = 340;
    for (int i = -8; i <= 8; i++){
        int w = 8 - abs(i); if (w < 0) w = 0;
        for (int j = -w; j <= w; j++) px(gx + i, sy + j, RGB(230, 110, 40));
    }
    for (int i = -4; i <= 4; i++){
        int w = 4 - abs(i); if (w < 0) w = 0;
        for (int j = -w; j <= w; j++) px(gx + i, sy + j, RGB(255, 170, 80));
    }
    char gb[16]; snprintf(gb, sizeof(gb), "%d", gSave.gems);
    drawText(gx + 12, sy - 4, gb, RGB(255, 255, 255), 1);
    int pX = SW - 28;
    circle(pX, sy, 10, RGB(50, 160, 50));
    circle(pX, sy, 9, RGB(80, 210, 80));
    fillRect(pX - 6, sy - 1, 12, 3, RGB(255, 255, 255));
    fillRect(pX - 1, sy - 6, 3, 12, RGB(255, 255, 255));
    int mbx = 10, mby = 28, mbw = 92, mbh = 26;
    fillRect(mbx + 2, mby + 2, mbw, mbh, RGB(40, 20, 10));
    fillRect(mbx, mby, mbw, mbh, RGB(70, 35, 20));
    fillRect(mbx + 3, mby + 3, mbw - 6, mbh - 6, RGB(210, 175, 115));
    fillRect(mbx + 1, mby + 3, 2, mbh - 6, RGB(45, 25, 15));
    fillRect(mbx + mbw - 3, mby + 3, 2, mbh - 6, RGB(45, 25, 15));
    fillRect(mbx + 1, mby + 1, mbw - 2, 2, RGB(45, 25, 15));
    fillRect(mbx + 1, mby + mbh - 3, mbw - 2, 2, RGB(45, 25, 15));
    int mtw = textW("МЕНЮ", 2);
    drawText(mbx + mbw/2 - mtw/2, mby + 9, "МЕНЮ", RGB(60, 35, 20), 2);
}
static void drawMenuDropdown(void){
    if (!gMenuOpen) return;
    int mx = 10, my = 56, mw = 200, rowH = 24, mh = 4 * rowH + 6;
    fillRect(mx + 3, my + 3, mw, mh, RGB(30, 15, 10));
    fillRect(mx, my, mw, mh, RGB(60, 32, 18));
    fillRect(mx + 2, my + 2, mw - 4, mh - 4, RGB(190, 155, 100));
    const char* items[4] = {"ТРЕНИРОВКА","ИЗУЧЕНИЕ ПРИЁМОВ","ИНВЕНТАРЬ","БОЙ"};
    for (int i = 0; i < 4; i++){
        int y = my + 3 + i * rowH;
        int sel = (i == gMenuCursor);
        if (sel){
            fillRect(mx + 3, y, mw - 6, rowH - 1, RGB(140, 75, 35));
            fillRect(mx + 3, y, mw - 6, 2, RGB(200, 120, 50));
            fillRect(mx + 3, y + rowH - 3, mw - 6, 2, RGB(80, 40, 20));
        } else {
            fillRect(mx + 3, y, mw - 6, rowH - 1, RGB(90, 52, 30));
        }
        fillRect(mx + 4, y + 6, 2, rowH - 12, RGB(180, 140, 80));
        drawText(mx + 12, y + 8, items[i], RGB(255, 235, 190), 1);
    }
}

/* ============ ЭЛЕМЕНТЫ КАРТ ============ */
static void mapParchment(void){
    for (int y = 0; y < SH; y++){
        float t = (float)y / SH;
        int r = (int)(212 - t * 34);
        int g = (int)(196 - t * 32);
        int b = (int)(158 - t * 30);
        unsigned int c = RGB(r, g, b);
        unsigned int* row = &fb[y * BW];
        for (int x = 0; x < SW; x++) row[x] = c;
    }
    for (int i = 0; i < 380; i++){
        int x = (i * 37) % SW;
        int y = (i * 53) % SH;
        unsigned int c = fb[y*BW+x] & 0xFFFFFF;
        int r = (c>>16)&0xFF, g = (c>>8)&0xFF, b = c&0xFF;
        int d = 8 + (i % 14);
        fb[y*BW+x] = RGB(r>d?r-d:0, g>d?g-d:0, b>d?b-d:0);
    }
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++){
            int dx = (x<SW/2)?(SW/2-x):(x-SW/2);
            int dy = (y<SH/2)?(SH/2-y):(y-SH/2);
            if (dx > SW*0.45f || dy > SH*0.42f){
                unsigned int c = fb[y*BW+x] & 0xFFFFFF;
                int r = ((c>>16)&0xFF)*9/10;
                int g = ((c>>8)&0xFF)*9/10;
                int b = (c&0xFF)*9/10;
                fb[y*BW+x] = RGB(r,g,b);
            }
        }
}
static void mapMountain(int cx, int topY, int baseY, int halfW, unsigned int body, unsigned int shadow){
    for (int y = topY; y <= baseY; y++){
        float t = (float)(y - topY) / (float)(baseY - topY);
        if (t < 0) t = 0; if (t > 1) t = 1;
        int w = (int)(halfW * powf(t, 0.85f));
        for (int x = -w; x <= w; x++){
            int xx = cx + x + 4, yy = y + 4;
            if (xx>=0 && xx<SW && yy>=0 && yy<SH) px(xx, yy, shadow);
        }
    }
    for (int y = topY; y <= baseY; y++){
        float t = (float)(y - topY) / (float)(baseY - topY);
        if (t < 0) t = 0; if (t > 1) t = 1;
        int w = (int)(halfW * powf(t, 0.85f));
        for (int x = -w; x <= w; x++){
            int xx = cx + x;
            if (xx>=0 && xx<SW && y>=0 && y<SH) px(xx, y, body);
        }
    }
}
static void mapTree(int x, int y){
    unsigned int tr = RGB(78, 60, 42);
    unsigned int lA = RGB(82, 102, 70);
    unsigned int lB = RGB(60, 78, 52);
    for (int i = 0; i < 4; i++) px(x, y - i, tr);
    for (int i = 0; i < 7; i++){
        int w = (i<4)?(i/2):(3-(i-4)/2);
        if (w < 0) w = 0;
        for (int dx = -w; dx <= w; dx++)
            px(x + dx, y - 4 - i, (i%2)?lA:lB);
    }
}
static void mapPath(int x0, int y0, int x1, int y1){
    int steps = 28;
    for (int i = 0; i <= steps; i++){
        float t = (float)i / steps;
        int x = x0 + (int)((x1 - x0) * t + sinf(t * 6.28f) * 8);
        int y = y0 + (int)((y1 - y0) * t);
        if ((i % 3) == 0 && x>=0 && x<SW && y>=0 && y<SH)
            px(x, y, RGB(148, 125, 88));
    }
}
static void mapMarker(int mx, int my, int icon, int selected){
    unsigned int ring   = selected ? RGB(200, 90, 50) : RGB(120, 80, 50);
    unsigned int inner  = RGB(242, 224, 180);
    unsigned int dark   = RGB(60, 40, 30);
    unsigned int accent = RGB(160, 110, 60);
    circle(mx, my, 14, ring);
    circle(mx, my, 12, inner);
    if (selected){
        for (int a = 0; a < 360; a += 6){
            float rad = a * 3.14159f / 180.0f;
            px(mx + (int)(cosf(rad)*17), my + (int)(sinf(rad)*17), RGB(240, 180, 100));
        }
    }
    if (icon == 0){ line(mx-6,my+5,mx+6,my-5,dark,2); line(mx-5,my-4,mx-3,my-2,accent,3); }
    else if (icon == 1){ circle(mx,my-1,5,dark); circle(mx-2,my-2,1,inner); circle(mx+2,my-2,1,inner); line(mx-3,my+4,mx+3,my+4,dark,2); }
    else if (icon == 2){ fillRect(mx-4,my-5,8,3,dark); fillRect(mx-6,my-2,12,3,accent); fillRect(mx-2,my+1,4,3,dark); fillRect(mx-5,my+4,10,2,dark); }
    else if (icon == 3){ circle(mx,my,5,dark); circle(mx-2,my,2,accent); circle(mx+2,my,2,accent); }
    else if (icon == 4){ fillRect(mx-6,my-4,12,9,dark); fillRect(mx-6,my-4,12,2,accent); line(mx,my-4,mx,my+5,accent,1); }
    else { circle(mx,my,5,dark); circle(mx,my,3,RGB(220,180,90)); line(mx-3,my,mx+3,my,dark,1); }
}
static void mapPagoda(int x, int y, int w, int h, int tiers){
    unsigned int roofA = RGB(90,68,48), roofB = RGB(65,48,35);
    unsigned int wall  = RGB(160,140,108), wallD = RGB(120,100,75);
    int tierH = h / tiers;
    for (int t = 0; t < tiers; t++){
        int ty = y - (t+1)*tierH;
        int tw = w - t*2; if (tw < 4) tw = 4;
        for (int i = 0; i < 5; i++){
            int rw = tw/2 + 4 - i; if (rw < 1) rw = 1;
            for (int dx = -rw; dx <= rw; dx++)
                px(x+dx, ty+i, i<2?roofA:roofB);
        }
        if (t < tiers-1)
            for (int wy = ty+5; wy < ty+tierH-2; wy++)
                for (int dx = -tw/2+1; dx <= tw/2-1; dx++)
                    px(x+dx, wy, wall);
    }
    for (int by = y; by < y+3; by++)
        for (int dx = -w/2; dx <= w/2; dx++)
            px(x+dx, by, wallD);
}
static void mapTorii(int x, int y){
    unsigned int c = RGB(140,50,40);
    for (int i = 0; i < 12; i++){
        px(x-6, y-i, c); px(x-5, y-i, c);
        px(x+5, y-i, c); px(x+6, y-i, c);
    }
    for (int dx = -9; dx <= 9; dx++){ px(x+dx, y-12, c); px(x+dx, y-11, c); }
    for (int dx = -7; dx <= 7; dx++)  px(x+dx, y-8, c);
}
static void mapHouse(int x, int y, int w, int h){
    unsigned int roof = RGB(85,62,42), roofL = RGB(110,85,60);
    unsigned int wall = RGB(158,138,106);
    for (int i = 0; i < h/3; i++){
        int rw = w/2 + 3 - i; if (rw < 1) rw = 1;
        for (int dx = -rw; dx <= rw; dx++)
            px(x+dx, y-h+i, i<1?roofL:roof);
    }
    for (int wy = y-h+h/3; wy < y; wy++)
        for (int dx = -w/2+1; dx <= w/2-1; dx++)
            px(x+dx, wy, wall);
}
static void mapBridge(int x, int y, int len){
    unsigned int w1 = RGB(110,80,50), w2 = RGB(80,58,36);
    for (int dx = -len/2; dx <= len/2; dx++){
        int ay = y - (int)(sinf((dx+len/2)*3.14f/len)*3);
        px(x+dx, ay, w1); px(x+dx, ay+1, w2);
    }
    for (int dx = -len/2; dx <= len/2; dx += 3){
        int ay = y - (int)(sinf((dx+len/2)*3.14f/len)*3);
        px(x+dx, ay-4, w1); px(x+dx, ay-3, w1);
    }
}

/* ============ КАРТА АКТ I ============ */
static void drawMap(void){
    mapParchment();
    unsigned int farM = RGB(198,182,145), farS = RGB(172,155,120);
    mapMountain(60,85,150,60,farM,farS);
    mapMountain(160,75,150,55,farM,farS);
    mapMountain(360,80,150,65,farM,farS);
    mapMountain(450,90,150,55,farM,farS);
    unsigned int midM = RGB(176,158,120), midS = RGB(148,130,94);
    mapMountain(95,115,180,80,midM,midS);
    mapMountain(400,110,180,90,midM,midS);
    mapMountain(470,135,180,55,midM,midS);
    unsigned int volcM = RGB(163,143,105), volcS = RGB(122,102,72);
    mapMountain(240,45,180,105,volcM,volcS);
    for (int y = 45; y < 85; y++){
        float t = (float)(y-45)/40.0f;
        int w = (int)(105 * powf(t, 0.85f));
        for (int x = -w; x <= w; x++)
            if ((w - fabsf((float)x)) < 12) px(240+x, y, RGB(245,240,225));
    }
    for (int y = 175; y < 260; y++){
        float t = (float)(y-175)/85.0f;
        int left = (int)(60 + sinf(t*3.0f)*20 - t*20);
        int right = (int)(300 + sinf(t*2.5f+1.0f)*40 + t*60);
        if (left < 0) left = 0;
        if (right > SW) right = SW;
        for (int x = left; x < right; x++) px(x, y, RGB(128,145,140));
    }
    for (int i = 0; i < 22; i++){
        int x = 15 + (i*41)%460, y = 155 + (i*17)%60;
        if (y>175 && x>55 && x<305) continue;
        mapTree(x, y);
    }
    for (int i = 0; i < 12; i++)
        mapTree(25 + (i*73)%430, 130 + (i*19)%35);
    mapPath(120,100,240,90); mapPath(240,90,360,100);
    mapPath(120,100,140,200); mapPath(140,200,240,230);
    mapPath(240,230,360,200); mapPath(360,200,360,100);
    int mxs[6]={120,240,360,140,240,360};
    int mys[6]={100, 90,100,200,230,200};
    const char* mn[6]={"СЮЖЕТ","ВЫЖИВАНИЕ","ТУРНИР","ТРЕНИРОВКА","ОБУЧЕНИЕ","МАГАЗИН"};
    int mi[6]={0,1,2,3,4,5};
    for (int i = 0; i < 6; i++){
        int sel = (i == gMapCursorY*3 + gMapCursorX);
        mapMarker(mxs[i], mys[i], mi[i], sel);
        int tw = textW(mn[i], 1);
        drawText(mxs[i]-tw/2+1, mys[i]+19, mn[i], RGB(140,120,85), 1);
        drawText(mxs[i]-tw/2, mys[i]+18, mn[i], RGB(60,40,30), 1);
    }
    drawTextC(30, "АКТ I — ПЕРЕРОЖДЕНИЕ", RGB(70,50,35), 2);
    drawTextC(SH-24, "СТРЕЛКИ ВЫБОР   X ВОЙТИ", RGB(80,60,40), 1);
    drawTextC(SH-12, "L: АКТ 1   R: АКТ 2   КВ: МЕНЮ", RGB(80,60,40), 1);
    drawTopHUD();
    drawMenuDropdown();
}

/* ============ КАРТА АКТ II ============ */
static void drawMapAct2(void){
    mapParchment();
    unsigned int farM = RGB(200,185,150), farS = RGB(170,155,120);
    int p1[][3] = {{40,100,55},{110,90,60},{200,75,70},{300,70,80},{400,95,65},{455,110,45}};
    for (int i = 0; i < 6; i++) mapMountain(p1[i][0], p1[i][1], 160, p1[i][2], farM, farS);
    unsigned int midM = RGB(175,158,120), midS = RGB(145,128,95);
    int p2[][3] = {{70,115,60},{160,105,70},{260,100,65},{360,110,75},{440,125,55}};
    for (int i = 0; i < 5; i++) mapMountain(p2[i][0], p2[i][1], 175, p2[i][2], midM, midS);
    for (int y = 90; y < SH; y++){
        float t = (float)(y-90)/(SH-90);
        int cx = 200 + (int)(sinf(t*5.5f)*40) + (int)(t*60);
        int w = 5 + (int)(t*6);
        for (int dx = -w; dx <= w; dx++){
            int xx = cx+dx;
            if (xx>=0 && xx<SW) px(xx, y, RGB(128,145,140));
        }
    }
    for (int y = 130; y < 175; y++){
        float t = (float)(y-130)/45;
        int cx = 160 - (int)(t*60);
        for (int dx = -3; dx <= 3; dx++){
            int xx = cx+dx;
            if (xx>=0 && xx<SW) px(xx, y, RGB(128,145,140));
        }
    }
    mapBridge(180, 155, 40);
    mapPagoda(80, 145, 14, 24, 2);
    mapHouse(60,150,10,8); mapHouse(105,152,12,9);
    mapHouse(95,165,10,7); mapHouse(120,160,9,6);
    mapHouse(70,165,11,8);
    mapPagoda(320,165,22,40,3); mapPagoda(295,170,18,32,3);
    mapPagoda(345,168,20,36,3);
    mapTorii(280,145); mapTorii(355,148);
    mapHouse(260,175,11,8); mapHouse(285,180,12,9);
    mapHouse(310,185,10,8); mapHouse(335,180,11,7);
    mapHouse(360,185,10,7); mapHouse(375,178,9,7);
    mapHouse(390,172,10,8); mapHouse(305,195,16,12);
    mapHouse(345,198,15,11);
    for (int i = 0; i < 40; i++){
        int x = 10 + (i*47)%460, y = 100 + (i*31)%150;
        if (x>55 && x<135 && y>140 && y<180) continue;
        if (x>255 && x<400 && y>140 && y<215) continue;
        mapTree(x, y);
    }
    mapPath(80,155,180,155); mapPath(180,155,280,150);
    mapPath(280,150,360,150); mapPath(100,130,240,90);
    int mxs[6]={80,240,300,180,380,400};
    int mys[6]={140, 90,155,180,180,210};
    const char* mn[6]={"ВЫЖИВАНИЕ","ТУРНИР","СЮЖЕТ","ТРЕНИРОВКА","ОБУЧЕНИЕ","МАГАЗИН"};
    int mi[6]={1,2,0,3,4,5};
    for (int i = 0; i < 6; i++){
        int sel = (i == gMapCursorY*3 + gMapCursorX);
        mapMarker(mxs[i], mys[i], mi[i], sel);
        int tw = textW(mn[i], 1);
        drawText(mxs[i]-tw/2+1, mys[i]+19, mn[i], RGB(140,120,85), 1);
        drawText(mxs[i]-tw/2, mys[i]+18, mn[i], RGB(60,40,30), 1);
    }
    drawTextC(30, "АКТ II — ОТШЕЛЬНИК", RGB(70,50,35), 2);
    drawTextC(SH-24, "СТРЕЛКИ ВЫБОР   X ВОЙТИ", RGB(80,60,40), 1);
    drawTextC(SH-12, "L: АКТ 1   R: АКТ 2   КВ: МЕНЮ", RGB(80,60,40), 1);
    drawTopHUD();
    drawMenuDropdown();
}

/* ============ ИНВЕНТАРЬ SF2 ============ */
static void drawItemPreview(int cx,int cy,int tab,int idx){
    if(tab==0){ drawWeaponSprite(WEAPONS[idx].type,cx+4,cy+2,1); }
    else if(tab==1){ unsigned int c = ARMORS[idx].color;
        if(idx==0) c = RGB(120,100,80);
        fillRect(cx-6,cy-6,12,3,c); fillRect(cx-5,cy-3,10,9,c);
        fillRect(cx-7,cy-3,2,9,c); fillRect(cx+5,cy-3,2,9,c);
        fillRect(cx-3,cy+6,6,2,RGB(40,30,20));
    } else { unsigned int c = HELMETS[idx].color;
        if(idx==0) c = RGB(120,100,80);
        circle(cx,cy-2,7,c); fillRect(cx-8,cy+2,16,3,c);
        fillRect(cx-5,cy,10,2,RGB(30,20,15)); circle(cx,cy-2,4,RGB(30,20,15));
    }
}
static void drawTabIcon(int cx,int cy,int type,int selected){
    unsigned int c = selected ? RGB(255,220,100) : RGB(140,110,70);
    if(type==0){ line(cx-6,cy+6,cx+6,cy-6,c,2); line(cx-4,cy-2,cx-2,cy,c,3); }
    else if(type==1){ fillRect(cx-6,cy-6,12,3,c); fillRect(cx-5,cy-3,10,8,c);
        fillRect(cx-6,cy-6,2,12,c); fillRect(cx+4,cy-6,2,12,c); }
    else { circle(cx,cy-2,6,c); fillRect(cx-7,cy,14,3,c);
        fillRect(cx-5,cy-2,10,2,selected?RGB(80,60,30):RGB(30,20,15)); }
}
static void drawScrollFrame(void){
    int sx=140, sy=58, sw=200, sh=156;
    fillRect(sx+3,sy+3,sw,sh,RGB(60,40,20));
    fillRect(sx,sy,sw,sh,RGB(232,205,155));
    fillRect(sx,sy,sw,3,RGB(200,175,125));
    fillRect(sx,sy+sh-3,sw,3,RGB(200,175,125));
    fillRect(sx-6,sy-6,7,sh+12,RGB(120,80,40));
    fillRect(sx+sw-1,sy-6,7,sh+12,RGB(120,80,40));
}
static void drawInventory(void){
    for(int y=0;y<SH;y++){for(int x=0;x<SW;x++){
        int gx=x/32, gy=y/28;
        px(x,y,((gx+gy)%2)?RGB(60,75,50):RGB(48,60,40));}}
    for(int x=0;x<SW;x+=6){for(int y=0;y<SH;y+=28){
        px(x,y,RGB(90,110,70)); px(x+1,y+1,RGB(70,90,55));}}
    drawTopHUD();
    int mbx=10,mby=28,mbw=92,mbh=24;
    fillRect(mbx+2,mby+2,mbw,mbh,RGB(40,20,10));
    fillRect(mbx,mby,mbw,mbh,RGB(70,35,20));
    fillRect(mbx+3,mby+3,mbw-6,mbh-6,RGB(210,175,115));
    int mtw=textW("МЕНЮ",2);
    drawText(mbx+mbw/2-mtw/2,mby+8,"МЕНЮ",RGB(60,35,20),2);
    int ex=30,ey=72;
    unsigned int eCol=(gShopMode==0)?RGB(230,225,205):RGB(140,130,110);
    fillRect(ex-2,ey-2,90,22,RGB(50,30,15));
    fillRect(ex,ey,86,18,eCol);
    drawText(ex+22,ey+4,"НАДЕТЬ",RGB(60,35,20),2);
    int eny=ey+30;
    unsigned int enCol=(gShopMode==1)?RGB(180,220,130):RGB(110,140,80);
    fillRect(ex-2,eny-2,90,22,RGB(50,30,15));
    fillRect(ex,eny,86,18,enCol);
    drawText(ex+6,eny+4,"МАГАЗИН",RGB(30,50,20),2);
    drawScrollFrame();
    int tab=gInvTab;
    int total=(tab==0)?17:6;
    int page=gWeapon/4;
    int startIdx=page*4;
    int cellW=90, cellH=72;
    int gridX=145, gridY=62;
    for(int i=0;i<4;i++){
        int idx=startIdx+i;
        if(idx>=total) break;
        int col=i%2,row=i/2;
        int cx=gridX+col*cellW+cellW/2-10;
        int cy=gridY+row*cellH+cellH/2;
        int sel=(idx==gWeapon);
        if(sel){
            fillRect(gridX+col*cellW,gridY+row*cellH,cellW-4,cellH-4,RGB(210,185,140));
            fillRect(gridX+col*cellW,gridY+row*cellH,cellW-4,2,RGB(160,130,90));
            fillRect(gridX+col*cellW,gridY+row*cellH+cellH-6,cellW-4,2,RGB(160,130,90));
        }
        drawItemPreview(cx,cy,tab,idx);
        int owned,price;
        if(tab==0){owned=gSave.ownedWeapons[idx];price=WEAPONS[idx].price;}
        else if(tab==1){owned=gSave.ownedArmor[idx];price=ARMORS[idx].gems;}
        else{owned=gSave.ownedHelmets[idx];price=HELMETS[idx].gems;}
        if(owned){
            drawText(gridX+col*cellW+4,gridY+row*cellH+cellH-16,"*",RGB(180,140,60),2);
            line(cx+18,cy+4,cx+22,cy+8,RGB(80,140,60),2);
            line(cx+22,cy+8,cx+28,cy-2,RGB(80,140,60),2);
        } else {
            char pr[16]; snprintf(pr,sizeof(pr),"%d",price);
            drawText(gridX+col*cellW+4,gridY+row*cellH+cellH-14,pr,RGB(180,60,30),1);
        }
    }
    int ix=348,iy=58,iw=124,ih=156;
    fillRect(ix+3,iy+3,iw,ih,RGB(60,40,20));
    fillRect(ix,iy,iw,ih,RGB(232,205,155));
    fillRect(ix,iy,iw,3,RGB(200,175,125));
    fillRect(ix,iy+ih-3,iw,3,RGB(200,175,125));
    const char*name;int power;
    if(tab==0){name=WEAPONS[gWeapon].name;power=WEAPONS[gWeapon].dmg;}
    else if(tab==1){name=ARMORS[gWeapon].name;power=ARMORS[gWeapon].defense;}
    else{name=HELMETS[gWeapon].name;power=HELMETS[gWeapon].defense;}
    drawText(ix+6,iy+8,name,RGB(50,30,15),1);
    fillRect(ix+6,iy+22,iw-12,1,RGB(180,150,100));
    char pw[16]; snprintf(pw,sizeof(pw),"%d",power);
    drawText(ix+8,iy+32,pw,RGB(60,40,20),3);
    int ubx=ix+8,uby=iy+58,ubw=iw-16,ubh=10;
    fillRect(ubx-1,uby-1,ubw+2,ubh+2,RGB(80,60,40));
    fillRect(ubx,uby,ubw,ubh,RGB(50,35,20));
    int ufill=ubw*power/25; if(ufill>ubw)ufill=ubw;
    for(int x=0;x<ufill;x++){int gr=255-x*60/ubw;fillRect(ubx+x,uby,1,ubh,RGB(255,gr,50));}
    drawItemPreview(ix+20,iy+90,tab,gWeapon);
    int owned,price;
    if(tab==0){owned=gSave.ownedWeapons[gWeapon];price=WEAPONS[gWeapon].price;}
    else if(tab==1){owned=gSave.ownedArmor[gWeapon];price=ARMORS[gWeapon].gems;}
    else{owned=gSave.ownedHelmets[gWeapon];price=HELMETS[gWeapon].gems;}
    if(!owned){
        fillRect(ix+6,iy+ih-30,iw-12,22,RGB(80,40,20));
        char pb[32];
        if(tab==0)snprintf(pb,sizeof(pb),"%d МОНЕТ",price);
        else snprintf(pb,sizeof(pb),"%d ГЕМОВ",price);
        int pbw=textW(pb,1);
        drawText(ix+iw/2-pbw/2,iy+ih-24,pb,RGB(255,220,120),1);
    } else {
        fillRect(ix+6,iy+ih-30,iw-12,22,RGB(60,100,40));
        drawTextC(iy+ih-24,"В РУКАХ",RGB(200,255,180),1);
    }
    int taby=SH-26;
    fillRect(0,taby-4,SW,30,RGB(40,25,12));
    fillRect(0,taby-4,SW,2,RGB(120,80,45));
    for(int i=0;i<3;i++){
        int cx=SW/2+(i-1)*80;
        int cy=taby+8;
        int sel=(gInvTab==i);
        if(sel){circle(cx,cy,18,RGB(255,220,120));circle(cx,cy,15,RGB(80,50,25));}
        drawTabIcon(cx,cy,i,sel);
    }
    drawTextC(SH-12,"ЛЕВО/ПРАВО ВКЛАДКА  ВВЕРХ/ВНИЗ ВЫБОР  X ВЗЯТЬ  КВ МАГАЗИН  L/O ЗАКРЫТЬ",
              RGB(200,180,140),1);
}

/* ============ ПРОЧИЕ ЭКРАНЫ ============ */
static void drawGradBg(unsigned int t,unsigned int m,unsigned int b){
    for(int y=0;y<SH;y++){
        float u=(float)y/SH;unsigned int c;
        if(u<0.5f){float k=u*2;
            int r=(int)((t>>16&0xFF)*(1-k)+(m>>16&0xFF)*k);
            int g=(int)((t>>8&0xFF)*(1-k)+(m>>8&0xFF)*k);
            int bb=(int)((t&0xFF)*(1-k)+(m&0xFF)*k);c=RGB(r,g,bb);}
        else{float k=(u-0.5f)*2;
            int r=(int)((m>>16&0xFF)*(1-k)+(b>>16&0xFF)*k);
            int g=(int)((m>>8&0xFF)*(1-k)+(b>>8&0xFF)*k);
            int bb=(int)((m&0xFF)*(1-k)+(b&0xFF)*k);c=RGB(r,g,bb);}
        unsigned int*row=&fb[y*BW];for(int x=0;x<SW;x++)row[x]=c;}
}
static void drawAchPopup(void){
    if(gAchPopup<0||gAchPopupT<=0)return;
    float a=gAchPopupT>1.0f?1.0f:gAchPopupT;
    int al=(int)(255*a);
    fillRect(80,SH-60,SW-160,32,RGB(60,40,10));
    fillRect(82,SH-58,SW-164,28,RGB(120,80,20));
    drawTextC(SH-54,"ДОСТИЖЕНИЕ!",RGB(255,220,80),1);
    drawTextC(SH-40,ACHS[gAchPopup].name,RGB(255,al,al),1);
}
static void drawRadial(void){
    drawGradBg(RGB(5,3,15),RGB(20,10,35),RGB(5,3,15));
    drawTextC(10,"МЕНЮ",RGB(232,213,255),2);
    const char* items[5]={"ИНВЕНТАРЬ","ТРЕНИРОВКА","БОЙ","ОБУЧЕНИЕ","ХАРАКТЕР"};
    for(int i=0;i<5;i++){
        int y=50+i*32;
        int sel=(gRadialCursor==i);
        unsigned int c=sel?RGB(160,110,255):RGB(40,25,60);
        fillRect(100,y,SW-200,28,c);
        if(sel)fillRect(98,y-2,4,32,RGB(255,255,255));
        int tw=textW(items[i],2);
        drawText((SW-tw)/2,y+8,items[i],RGB(255,255,255),2);}
    drawTextC(SH-24,"СТРЕЛКИ ВЫБОР   X ВОЙТИ   SELECT ЗАКРЫТЬ",RGB(160,140,210),1);
}
static void drawCampaign(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    char title[64];snprintf(title,sizeof(title),"АКТ %d — ВЫБОР ПРОТИВНИКА",gAct);
    drawTextC(4,title,RGB(232,213,255),2);
    int startIdx=(gAct==1)?0:6,endIdx=(gAct==1)?6:13;
    int unlocked=(gAct==1)?gSave.unlockedEnemiesA1:gSave.unlockedEnemiesA2;
    int listN=endIdx-startIdx;
    for(int i=0;i<listN;i++){
        int idx=startIdx+i;int y=26+i*20;
        if(y>SH-30)break;
        int unlockedF=i<unlocked,done=i<unlocked-1;
        unsigned int c=done?RGB(30,30,40):(ENEMIES[idx].boss?RGB(80,20,20):(unlockedF?RGB(50,40,80):RGB(20,15,30)));
        fillRect(20,y,SW-40,18,c);
        if(unlockedF){
            drawText(24,y+5,done?"[V]":"[ ]",done?RGB(100,255,100):RGB(200,200,200),1);
            drawText(46,y+5,ENEMIES[idx].name,RGB(255,255,255),1);
            char hp[16];snprintf(hp,sizeof(hp),"HP %d",ENEMIES[idx].hp);
            drawText(SW-90,y+5,hp,RGB(200,180,220),1);
            if(ENEMIES[idx].boss)drawText(160,y+5,"БОСС",RGB(255,180,80),1);}
        else drawText(24,y+5,"[X] ЗАБЛОКИРОВАНО",RGB(100,90,120),1);}
    drawTextC(SH-16,"X НАЧАТЬ   O НАЗАД",RGB(160,140,210),1);
}
static void drawTraining(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(18,"ТРЕНИРОВКА",RGB(232,213,255),3);
    drawTextC(56,"ВЫБЕРИ ЛОКАЦИЮ",RGB(160,140,210),1);
    for(int i=0;i<3;i++){
        int y=76+i*40;
        unsigned int c=(i==gLocIdx)?RGB(160,110,255):RGB(50,40,80);
        fillRect(40,y,SW-80,32,c);
        drawText(50,y+10,LOCS[i].name,RGB(255,255,255),2);}
    drawTextC(SH-16,"ВВЕРХ/ВНИЗ   X НАЧАТЬ   O НАЗАД",RGB(160,140,210),1);
}
static void drawLearn(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    drawTextC(4,"ОБУЧЕНИЕ ПРИЁМАМ",RGB(232,213,255),2);
    char lv[32];snprintf(lv,sizeof(lv),"УРОВЕНЬ %d",gSave.level);
    drawTextC(22,lv,RGB(180,220,255),1);
    for(int i=0;i<8;i++){
        int y=42+i*22;
        int unlocked=(gSave.level>=MOVE_UNLOCKS[i].unlockLevel);
        int done=gSave.unlockedMoves[i];
        unsigned int c=done?RGB(30,60,30):(unlocked?RGB(50,40,80):RGB(20,15,30));
        fillRect(20,y,SW-40,20,c);
        char b[64];snprintf(b,sizeof(b),"%s %s",done?"[V]":(unlocked?"[ ]":"[X]"),MOVE_UNLOCKS[i].name);
        drawText(24,y+6,b,unlocked?RGB(255,255,255):RGB(100,90,120),1);
        drawText(220,y+6,MOVE_UNLOCKS[i].desc,RGB(180,180,220),1);
        char lvl[16];snprintf(lvl,sizeof(lvl),"УР.%d",MOVE_UNLOCKS[i].unlockLevel);
        drawText(SW-56,y+6,lvl,RGB(255,220,120),1);}
    drawTextC(SH-14,"X ИЗУЧИТЬ   O НАЗАД",RGB(160,140,210),1);
}
static void drawCharacter(void){
    drawGradBg(RGB(15,5,20),RGB(30,15,40),RGB(10,5,15));
    drawTextC(4,"ПЕРСОНАЖ",RGB(232,213,255),2);
    char b[80];
    snprintf(b,sizeof(b),"УРОВЕНЬ %d   XP: %d/%d",gSave.level,gSave.xp,xpForLevel(gSave.level));
    drawTextC(28,b,RGB(180,220,255),1);
    snprintf(b,sizeof(b),"ПОБЕД: %d  МОНЕТ: %d  ГЕМОВ: %d",gSave.totalWins,gSave.coins,gSave.gems);
    drawTextC(44,b,RGB(255,220,80),1);
    snprintf(b,sizeof(b),"ОРУЖИЕ: %s",WEAPONS[gWeapon].name);drawTextC(60,b,RGB(180,220,255),1);
    snprintf(b,sizeof(b),"БРОНЯ: %s + %s (ЗАЩ %d)",ARMORS[gSave.selectedArmor].name,
             HELMETS[gSave.selectedHelmet].name,getArmorDefense());
    drawTextC(76,b,RGB(180,220,255),1);
    snprintf(b,sizeof(b),"ТУРНИР: %d/24",gSave.tourProgress);drawTextC(92,b,RGB(255,180,120),1);
    snprintf(b,sizeof(b),"СЛОЖНОСТЬ: %s",DIFFS[gSave.difficulty].name);drawTextC(108,b,RGB(255,120,180),1);
    Fighter tmp;memset(&tmp,0,sizeof(tmp));
    tmp.x=SW*0.5f;tmp.y=GY-30;tmp.facing=1;tmp.color=RGB(26,10,42);
    tmp.isPlayer=1;tmp.wt=WEAPONS[gWeapon].type;tmp.onGround=1;
    drawFighter(&tmp);
    drawTextC(SH-16,"O НАЗАД",RGB(160,140,210),1);
}
static void drawAchievements(void){
    drawGradBg(RGB(20,15,5),RGB(40,30,10),RGB(10,8,3));
    drawTextC(6,"ДОСТИЖЕНИЯ",RGB(255,220,80),2);
    for(int i=0;i<8;i++){
        int y=28+i*22;
        int done=gSave.achievements[i];
        unsigned int c=done?RGB(80,60,20):RGB(30,25,15);
        fillRect(20,y,SW-40,20,c);
        drawText(28,y+6,done?"[V]":"[ ]",done?RGB(100,255,100):RGB(100,90,120),1);
        drawText(50,y+6,ACHS[i].name,done?RGB(255,255,255):RGB(120,110,140),1);}
    drawTextC(SH-16,"O НАЗАД",RGB(160,140,210),1);
}
static void drawMissions(void){
    drawGradBg(RGB(5,10,20),RGB(15,25,50),RGB(3,5,15));
    drawTextC(6,"МИССИИ",RGB(120,200,255),2);
    char b[64];snprintf(b,sizeof(b),"ГЕМОВ: %d",gSave.gems);
    drawTextC(24,b,RGB(180,240,255),1);
    for(int i=0;i<8;i++){
        int y=44+i*22;
        int st=gSave.missions[i];
        unsigned int c=st==2?RGB(40,80,120):(st==1?RGB(50,60,90):RGB(20,25,40));
        fillRect(20,y,SW-40,20,c);
        unsigned int tc=st==2?RGB(100,255,150):RGB(255,255,255);
        drawText(28,y+6,MISSIONS[i].name,tc,1);
        char pr[24];
        if(st==2)snprintf(pr,sizeof(pr),"ГОТОВО");
        else snprintf(pr,sizeof(pr),"%d/%d",gSave.missionProgress[i],MISSIONS[i].target);
        drawText(SW-100,y+6,pr,RGB(200,200,255),1);
        char rw[16];snprintf(rw,sizeof(rw),"+%d ГЕМ",MISSIONS[i].reward);
        drawText(SW-60,y+6,rw,RGB(120,220,255),1);}
    drawTextC(SH-16,"O НАЗАД",RGB(160,140,210),1);
}
static void drawSettings(void){
    drawGradBg(RGB(5,5,20),RGB(20,20,40),RGB(5,5,10));
    drawTextC(6,"НАСТРОЙКИ",RGB(232,213,255),2);
    for(int i=0;i<2;i++){
        int y=40+i*30;
        unsigned int c=(i==gSettingsIdx)?RGB(80,50,120):RGB(35,25,55);
        fillRect(40,y,SW-80,24,c);
        if(i==0){char b[32];snprintf(b,sizeof(b),"ГРОМКОСТЬ: %d",gSave.volume);
            drawText(50,y+8,b,RGB(255,255,255),1);}
        else drawText(50,y+8,"СБРОСИТЬ ПРОГРЕСС",RGB(255,140,140),1);}
    drawTextC(SH-16,"ВЛЕВО/ВПРАВО ИЗМЕНИТЬ   X СБРОС",RGB(160,140,210),1);
}
static void drawTournament(void){
    drawGradBg(RGB(20,15,5),RGB(50,30,10),RGB(10,5,3));
    drawTextC(6,"ТУРНИР АКТА I",RGB(255,220,80),2);
    char b[64];snprintf(b,sizeof(b),"ПРОГРЕСС: %d/24   УР.%d",gSave.tourProgress,gSave.level);
    drawTextC(24,b,RGB(255,200,120),1);
    for(int i=0;i<24;i++){
        int col=i%6,row=i/6;
        int x=14+col*76;int y=48+row*50;
        int w=70,h=44;
        int beaten=i<gSave.tourProgress;
        int current=(i==gSave.tourProgress);
        unsigned int c=beaten?RGB(30,60,30):(current?RGB(120,80,20):RGB(30,20,15));
        fillRect(x,y,w,h,c);
        if(current){line(x,y,x+w,y,RGB(255,220,120),2);line(x,y+h,x+w,y+h,RGB(255,220,120),2);}
        char num[8];snprintf(num,sizeof(num),"%d",i+1);
        drawText(x+3,y+3,num,RGB(255,255,255),1);
        int tw=textW(TOUR[i].name,1);
        if(tw>w-6){char shortname[10];strncpy(shortname,TOUR[i].name,8);shortname[8]=0;
            drawText(x+3,y+16,shortname,beaten?RGB(150,255,150):RGB(220,220,220),1);}
        else drawText(x+3,y+16,TOUR[i].name,beaten?RGB(150,255,150):RGB(220,220,220),1);
        char hp[16];snprintf(hp,sizeof(hp),"HP %d",TOUR[i].hp);
        drawText(x+3,y+30,hp,RGB(200,180,160),1);
        if(beaten)drawText(x+w-14,y+3,"[V]",RGB(100,255,100),1);}
    drawTextC(SH-16,"X НАЧАТЬ БОЙ   O НАЗАД",RGB(200,180,140),1);
}
static void drawEndScreen(void){
    drawGradBg(RGB(10,7,20),RGB(27,18,48),RGB(7,7,12));
    if(gPlayerWon){drawTextC(70,"ПОБЕДА!",RGB(232,213,255),4);drawTextC(140,"ТЫ ПРОШЁЛ АКТ II",RGB(240,230,255),2);}
    else drawTextC(70,"ПОРАЖЕНИЕ",RGB(255,120,120),4);
    drawTextC(SH-30,"X В МЕНЮ",RGB(240,230,255),1);
}
static void drawDialogScreen(void){
    drawGradBg(RGB(3,2,8),RGB(20,8,30),RGB(3,2,8));
    const Dialog*d=&DIALOGS[gDialogIdx];
    Fighter tmp;memset(&tmp,0,sizeof(tmp));
    tmp.x=SW*0.2f;tmp.y=GY-40;tmp.facing=1;
    tmp.color=ENEMIES[gDialogIdx].color;tmp.isBoss=ENEMIES[gDialogIdx].boss;
    tmp.wt=ENEMIES[gDialogIdx].wt;tmp.onGround=1;
    drawFighter(&tmp);
    drawText(160,30,d->speaker,RGB(255,200,80),2);
    fillRect(160,50,SW-180,2,RGB(160,110,255));
    const char* line=d->lines[gDialogLine];
    int cx=170,cy=70;char word[64];int wi=0;
    for(int i=0;line[i];i++){
        char ch=line[i];
        if(ch==' '){word[wi]=0;int wpx=textW(word,1);
            if(cx+wpx>SW-20){cx=170;cy+=12;}
            drawText(cx,cy,word,RGB(240,230,255),1);cx+=wpx+6;wi=0;}
        else if(wi<60)word[wi++]=ch;}
    if(wi>0){word[wi]=0;int wpx=textW(word,1);
        if(cx+wpx>SW-20){cx=170;cy+=12;}
        drawText(cx,cy,word,RGB(240,230,255),1);}
    drawTextC(SH-20,"НАЖМИ X",RGB(255,200,60),1);
}

/* ============ ВВОД ============ */
static void readInput(void){
    SceCtrlData pad;sceCtrlReadBufferPositive(&pad,1);
    int l=(pad.Buttons&PSP_CTRL_LEFT)!=0;
    int r=(pad.Buttons&PSP_CTRL_RIGHT)!=0;
    int u=(pad.Buttons&PSP_CTRL_UP)!=0;
    int d=(pad.Buttons&PSP_CTRL_DOWN)!=0;
    int x=(pad.Buttons&PSP_CTRL_CROSS)!=0;
    int o=(pad.Buttons&PSP_CTRL_CIRCLE)!=0;
    int sq=(pad.Buttons&PSP_CTRL_SQUARE)!=0;
    int tr=(pad.Buttons&PSP_CTRL_TRIANGLE)!=0;
    int sel=(pad.Buttons&PSP_CTRL_SELECT)!=0;
    int st=(pad.Buttons&PSP_CTRL_START)!=0;
    int Lb=(pad.Buttons&PSP_CTRL_LTRIGGER)!=0;
    int Rb=(pad.Buttons&PSP_CTRL_RTRIGGER)!=0;

    if(gState==ST_MAP && gMenuOpen){
        if(d&&!gWasDown&&gMenuCursor<3)gMenuCursor++;
        if(u&&!gWasUp&&gMenuCursor>0)gMenuCursor--;
        if(x&&!gWasCross){
            gMenuOpen=0;
            if(gMenuCursor==0)gState=ST_TRAINING;
            else if(gMenuCursor==1)gState=ST_LEARN;
            else if(gMenuCursor==2){gInvFromState=ST_MAP;gState=ST_INVENTORY;gShopMode=0;gInvTab=0;}
            else if(gMenuCursor==3)gState=ST_CAMPAIGN;
            sfxSelect();
        }
        if(o&&!gWasCircle){gMenuOpen=0;sfxSelect();}
        gWasLeft=l;gWasRight=r;gWasUp=u;gWasDown=d;
        gWasCross=x;gWasCircle=o;gWasSquare=sq;gWasTriangle=tr;
        gWasSelect=sel;gWasStart=st;gWasL=Lb;gWasR=Rb;
        return;
    }

    if(sel&&!gWasSelect){
        if(gState==ST_RADIAL)gState=ST_MAP;
        else if(gState!=ST_MAP){gState=ST_MAP;gMode=0;gBagActive=0;gTransition=0;gReturnTimer=0;musicSet(1);sfxSelect();}
        else{gState=ST_RADIAL;gRadialCursor=0;sfxSelect();}
        gWasCross=gWasCircle=gWasSquare=gWasLeft=gWasRight=gWasUp=gWasDown=1;
        gWasSelect=gWasL=gWasR=gWasStart=gWasTriangle=1;
        return;
    }
    if(st&&!gWasStart){if(gState==ST_MISSIONS)gState=ST_MAP;else gState=ST_MISSIONS;gWasStart=1;sfxSelect();return;}
    if(Lb&&!gWasL){
        if(gState==ST_MAP||gState==ST_CAMPAIGN||gState==ST_FIGHT){
            gInvFromState=gState;gState=ST_INVENTORY;gShopMode=0;gInvTab=0;gWasL=1;sfxSelect();return;}
        else if(gState==ST_INVENTORY){gState=gInvFromState;gWasL=1;sfxSelect();return;}}
    if(Rb&&!gWasR&&gState==ST_MAP){gState=ST_CHARACTER;gWasR=1;sfxSelect();return;}

    if(gState==ST_MAP){
        if(sq&&!gWasSquare){gMenuOpen=1;gMenuCursor=0;sfxSelect();}
        if(Lb&&!gWasL)gAct=1;
        if(Rb&&!gWasR&&gSave.unlockedEnemiesA2>0)gAct=2;
        if(l&&!gWasLeft&&gMapCursorX>0)gMapCursorX--;
        if(r&&!gWasRight&&gMapCursorX<2)gMapCursorX++;
        if(u&&!gWasUp&&gMapCursorY>0)gMapCursorY--;
        if(d&&!gWasDown&&gMapCursorY<1)gMapCursorY++;
        if(x&&!gWasCross){
            int idx=gMapCursorY*3+gMapCursorX;
            if(idx==0){gState=ST_CAMPAIGN;sfxSelect();}
            else if(idx==1){
                gEnemyIdx=rand()%6;initPlayer();initEnemy(gEnemyIdx);
                gBagActive=0;gTransition=0;gComboCount=0;gComboT=0;
                gState=ST_FIGHT;gMode=3;setMsg("ВЫЖИВАНИЕ",1.5f);musicSet(2);sfxSelect();}
            else if(idx==2){gState=ST_TOURNAMENT;gTourCursor=0;sfxSelect();}
            else if(idx==3){gState=ST_TRAINING;sfxSelect();}
            else if(idx==4){gState=ST_LEARN;sfxSelect();}
            else if(idx==5){gInvFromState=ST_MAP;gState=ST_INVENTORY;gShopMode=1;gInvTab=0;sfxSelect();}}}
    else if(gState==ST_RADIAL){
        if(u&&!gWasUp&&gRadialCursor>0)gRadialCursor--;
        if(d&&!gWasDown&&gRadialCursor<4)gRadialCursor++;
        if(x&&!gWasCross){
            if(gRadialCursor==0){gInvFromState=ST_MAP;gState=ST_INVENTORY;gShopMode=0;gInvTab=0;}
            else if(gRadialCursor==1)gState=ST_TRAINING;
            else if(gRadialCursor==2)gState=ST_CAMPAIGN;
            else if(gRadialCursor==3)gState=ST_LEARN;
            else if(gRadialCursor==4)gState=ST_CHARACTER;
            sfxSelect();}}
    else if(gState==ST_TOURNAMENT){
        if(l&&!gWasLeft&&gTourCursor%6>0)gTourCursor--;
        if(r&&!gWasRight&&gTourCursor%6<5&&gTourCursor<23)gTourCursor++;
        if(u&&!gWasUp&&gTourCursor>=6)gTourCursor-=6;
        if(d&&!gWasDown&&gTourCursor<18)gTourCursor+=6;
        if(x&&!gWasCross){if(gTourCursor<=gSave.tourProgress)startTournament(gTourCursor);sfxSelect();}
        if(o&&!gWasCircle)gState=ST_MAP;}
    else if(gState==ST_CAMPAIGN){
        if(x&&!gWasCross){
            int startIdx=(gAct==1)?0:6;
            int unlocked=(gAct==1)?gSave.unlockedEnemiesA1:gSave.unlockedEnemiesA2;
            if(unlocked>0)startDialog(startIdx+unlocked-1);}
        if(o&&!gWasCircle)gState=ST_MAP;}
    else if(gState==ST_INVENTORY){
        int total=(gInvTab==0)?17:6;
        if(u&&!gWasUp){if(gWeapon>0)gWeapon--;sfxSelect();}
        if(d&&!gWasDown){if(gWeapon<total-1)gWeapon++;sfxSelect();}
        if(l&&!gWasLeft){gInvTab=(gInvTab+2)%3;gWeapon=0;sfxSelect();}
        if(r&&!gWasRight){gInvTab=(gInvTab+1)%3;gWeapon=0;sfxSelect();}
        if(tr&&!gWasTriangle){gShopMode=!gShopMode;sfxSelect();}
        if(x&&!gWasCross){
            if(gInvTab==0){
                if(gShopMode){if(!gSave.ownedWeapons[gWeapon]&&gSave.coins>=WEAPONS[gWeapon].price){
                    gSave.coins-=WEAPONS[gWeapon].price;gSave.ownedWeapons[gWeapon]=1;sfxCoin();saveGame();}}
                else if(gSave.ownedWeapons[gWeapon]){if(gInvFromState==ST_FIGHT)initPlayer();sfxSelect();}
            } else if(gInvTab==1){
                if(gShopMode){if(!gSave.ownedArmor[gWeapon]&&gSave.gems>=ARMORS[gWeapon].gems){
                    gSave.gems-=ARMORS[gWeapon].gems;gSave.ownedArmor[gWeapon]=1;sfxGem();saveGame();}}
                else if(gSave.ownedArmor[gWeapon]){gSave.selectedArmor=gWeapon;sfxSelect();saveGame();}
            } else {
                if(gShopMode){if(!gSave.ownedHelmets[gWeapon]&&gSave.gems>=HELMETS[gWeapon].gems){
                    gSave.gems-=HELMETS[gWeapon].gems;gSave.ownedHelmets[gWeapon]=1;sfxGem();saveGame();}}
                else if(gSave.ownedHelmets[gWeapon]){gSave.selectedHelmet=gWeapon;sfxSelect();saveGame();}
            }
        }
        if(o&&!gWasCircle){gState=gInvFromState;sfxSelect();}}
    else if(gState==ST_TRAINING){
        if(d&&!gWasDown)gLocIdx=(gLocIdx+1)%3;
        if(u&&!gWasUp)gLocIdx=(gLocIdx+2)%3;
        if(x&&!gWasCross)startTraining(gLocIdx);
        if(o&&!gWasCircle)gState=ST_MAP;}
    else if(gState==ST_LEARN){
        if(x&&!gWasCross){for(int i=0;i<8;i++){
            if(gSave.level>=MOVE_UNLOCKS[i].unlockLevel&&!gSave.unlockedMoves[i]){startLearn(i);break;}}}
        if(o&&!gWasCircle)gState=ST_MAP;}
    else if(gState==ST_DIALOG){
        if(x&&!gWasCross){gDialogLine++;sfxSelect();
            if(gDialogLine>=DIALOGS[gDialogIdx].count)startBattle(gDialogIdx);}}
    else if(gState==ST_FIGHT){
        gWasLeft=l;gWasRight=r;gWasSquare=sq;
        if(o&&!gWasUp){if(gPlayer.onGround&&gPlayer.stunT<=0&&gPlayer.attackT<=0){
            gPlayer.vy=-600;gPlayer.onGround=0;sndPlay(420,80,0,40);}}
        if(x&&!gWasCross){
            if(gPlayer.cdT<=0&&gPlayer.attackT<=0&&gPlayer.stunT<=0){
                gAttackChain++;gAttackChainT=0.35f;
                gPlayer.attackT=gPlayer.dur;gPlayer.cdT=gPlayer.maxCd;
                gPlayer.hitDone=0;gPlayer.blocking=0;
                if(gAttackChain>=3&&gSave.unlockedMoves[7]){
                    gPlayer.attackT=gPlayer.dur*1.5f;gPlayer.dmg=WEAPONS[gWeapon].dmg*2;setMsg("УЛЬТА!",0.8f);}
                else if(gWasDown&&gSave.unlockedMoves[1]){
                    gPlayer.attackT=gPlayer.dur*1.2f;gPlayer.dmg=WEAPONS[gWeapon].dmg+3;setMsg("ПИНОК!",0.5f);}
                else if(gWasRight&&gSave.unlockedMoves[4]){
                    gPlayer.attackT=gPlayer.dur*1.3f;gPlayer.dmg=WEAPONS[gWeapon].dmg+5;setMsg("ТЯЖЁЛЫЙ!",0.5f);}
                else if(gWasLeft&&gSave.unlockedMoves[6]){
                    gPlayer.attackT=gPlayer.dur*1.4f;gPlayer.dmg=WEAPONS[gWeapon].dmg+6;setMsg("ЯРОСТЬ!",0.5f);}
                sfxWhoosh();}}
        if(tr&&!gWasTriangle&&gSave.unlockedMoves[5]){
            if(gPlayer.magicCD<=0){spawnProjectile(gPlayer.x+gPlayer.facing*20,gPlayer.y-55,1,0);
                gPlayer.magicCD=180;sfxMagic();}}}
    else if(gState==ST_END){if(x&&!gWasCross)gState=ST_MAP;}
    else if(gState==ST_CHARACTER){if((o&&!gWasCircle)||(x&&!gWasCross))gState=ST_MAP;}
    else if(gState==ST_ACHIEVE){if((o&&!gWasCircle)||(x&&!gWasCross))gState=ST_MAP;}
    else if(gState==ST_MISSIONS){if((o&&!gWasCircle)||(x&&!gWasCross))gState=ST_MAP;}
    else if(gState==ST_SETTINGS){
        if(d&&!gWasDown)gSettingsIdx=(gSettingsIdx+1)%2;
        if(u&&!gWasUp)gSettingsIdx=(gSettingsIdx+1)%2;
        if(gSettingsIdx==0){
            if(l&&!gWasLeft){gSave.volume-=10;if(gSave.volume<0)gSave.volume=0;gVolume=gSave.volume;saveGame();}
            if(r&&!gWasRight){gSave.volume+=10;if(gSave.volume>100)gSave.volume=100;gVolume=gSave.volume;saveGame();}}
        if(x&&!gWasCross&&gSettingsIdx==1){saveDefaults();saveGame();}
        if(o&&!gWasCircle)gState=ST_MAP;}

    if(gPlayer.magicCD>0)gPlayer.magicCD-=(int)(0.016f*60);
    gWasLeft=l;gWasRight=r;gWasUp=u;gWasDown=d;
    gWasCross=x;gWasCircle=o;gWasSquare=sq;gWasTriangle=tr;
    gWasSelect=sel;gWasStart=st;gWasL=Lb;gWasR=Rb;
}

/* ============ MAIN ============ */
int main(void){
    sceDisplaySetMode(0,SW,SH);
    sceDisplaySetFrameBuf((void*)fb,BW,PSP_DISPLAY_PIXEL_FORMAT_8888,PSP_DISPLAY_SETBUF_IMMEDIATE);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    audioInit();loadGame();gVolume=gSave.volume;
    gLastUs=sceKernelGetSystemTimeLow();musicSet(1);
    while(1){
        sceDisplayWaitVblankStart();
        float d=dt();
        if(gAchPopupT>0){gAchPopupT-=d;if(gAchPopupT<=0)gAchPopup=-1;}
        readInput();update(d);
        musicTick((int)(d*1000));audioTick();
        if(gState==ST_MAP){
            if(gAct==1)drawMap();
            else drawMapAct2();
        }
        else if(gState==ST_RADIAL)drawRadial();
        else if(gState==ST_CAMPAIGN)drawCampaign();
        else if(gState==ST_TRAINING)drawTraining();
        else if(gState==ST_LEARN)drawLearn();
        else if(gState==ST_INVENTORY)drawInventory();
        else if(gState==ST_CHARACTER)drawCharacter();
        else if(gState==ST_ACHIEVE)drawAchievements();
        else if(gState==ST_MISSIONS)drawMissions();
        else if(gState==ST_SETTINGS)drawSettings();
        else if(gState==ST_TOURNAMENT)drawTournament();
        else if(gState==ST_DIALOG)drawDialogScreen();
        else if(gState==ST_END)drawEndScreen();
        else if(gState==ST_FIGHT){
            drawBackground();
            if(gBagActive)drawBag();
            else if(gEnemy.hp>0)drawFighter(&gEnemy);
            drawFighter(&gPlayer);
            drawProjectiles();drawParticles();drawHUD();}
        drawAchPopup();
    }
    return 0;
}
