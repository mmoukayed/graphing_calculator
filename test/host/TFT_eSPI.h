#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#define TFT_WHITE 0xFFFF
#define TFT_BLACK 0
#define PROGMEM
namespace hostFonts {
#include "../../lib/TFT_eSPI/Fonts/glcdfont.c"
#include "../../lib/TFT_eSPI/Fonts/Font16.c"
}
class TFT_eSPI {
public:
    void setTextColor(int,int=0){}void drawString(const char*,int,int,int){}
};
// Desktop framebuffer: runs the actual app drawing/layout and input handlers.
// Font 1/2 use the vendored display fonts; font 4 is approximated for host tests.
class TFT_eSprite {
    std::vector<uint16_t> pixels=std::vector<uint16_t>(320*240);
    uint16_t color=0;
    int clipX=0,clipY=0,clipW=320,clipH=240;
public:
    TFT_eSprite(TFT_eSPI*){}
    void setColorDepth(int){}void* createSprite(int,int){return pixels.data();}
    void setTextWrap(bool){}void setTextColor(uint16_t c){color=c;}
    void fillSprite(uint16_t c){std::fill(pixels.begin(),pixels.end(),c);}
    void pushSprite(int,int){}
    void setViewport(int x,int y,int w,int h,bool=false){clipX=x;clipY=y;clipW=w;clipH=h;}
    void resetViewport(){clipX=clipY=0;clipW=320;clipH=240;}
    void drawPixel(int x,int y,uint16_t c){if(x>=0&&x<320&&y>=0&&y<240&&x>=clipX&&y>=clipY&&x<clipX+clipW&&y<clipY+clipH)pixels[y*320+x]=c;}
    void fillRect(int x,int y,int w,int h,uint16_t c){for(int j=std::max(0,y);j<std::min(240,y+h);++j)for(int i=std::max(0,x);i<std::min(320,x+w);++i)drawPixel(i,j,c);}
    void drawFastVLine(int x,int y,int h,uint16_t c){fillRect(x,y,1,h,c);}
    void drawFastHLine(int x,int y,int w,uint16_t c){fillRect(x,y,w,1,c);}
    void drawRect(int x,int y,int w,int h,uint16_t c){drawFastHLine(x,y,w,c);drawFastHLine(x,y+h-1,w,c);drawFastVLine(x,y,h,c);drawFastVLine(x+w-1,y,h,c);}
    void fillRoundRect(int x,int y,int w,int h,int r,uint16_t c){fillRect(x+r,y,w-2*r,h,c);fillRect(x,y+r,w,h-2*r,c);for(int dy=0;dy<r;++dy)for(int dx=0;dx<r;++dx)if((r-dx)*(r-dx)+(r-dy)*(r-dy)<=r*r){drawPixel(x+dx,y+dy,c);drawPixel(x+w-dx-1,y+dy,c);drawPixel(x+dx,y+h-dy-1,c);drawPixel(x+w-dx-1,y+h-dy-1,c);}}
    void drawRoundRect(int x,int y,int w,int h,int,uint16_t c){drawRect(x,y,w,h,c);}
    void drawLine(int x0,int y0,int x1,int y1,uint16_t c){int dx=std::abs(x1-x0),sx=x0<x1?1:-1,dy=-std::abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy;for(int n=0;n<2000;++n){drawPixel(x0,y0,c);if(x0==x1&&y0==y1)break;int e2=e*2;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;}}}
    void drawCircle(int x,int y,int r,uint16_t c){for(int j=0;j<360;++j)drawPixel(x+int(r*std::cos(j*3.14159/180)),y+int(r*std::sin(j*3.14159/180)),c);}
    void drawTriangle(int a,int b,int c,int d,int e,int f,uint16_t col){drawLine(a,b,c,d,col);drawLine(c,d,e,f,col);drawLine(e,f,a,b,col);}
    void fillTriangle(int a,int b,int c,int d,int e,int f,uint16_t col){drawTriangle(a,b,c,d,e,f,col);}
    int textWidth(const char* s,int font=2){int w=0;for(;*s;++s){unsigned ch=static_cast<unsigned char>(*s);w+=font==2&&ch>=32&&ch<128?hostFonts::widtbl_f16[ch-32]:font==4?12:6;}return w;}
    void drawString(const char* s,int x,int y,int font){for(;*s;++s){unsigned ch=static_cast<unsigned char>(*s);if(font==2&&ch>=32&&ch<128){int w=hostFonts::widtbl_f16[ch-32],stride=(w+6)/8;auto p=hostFonts::chrtbl_f16[ch-32];for(int row=0;row<16;++row)for(int col=0;col<w-1;++col)if(p[row*stride+col/8]&(0x80>>(col%8)))drawPixel(x+col,y+row,color);x+=w;}else{int scale=font==4?2:1;for(int col=0;col<5;++col)for(int row=0;row<8;++row)if(hostFonts::font[ch*5+col]&(1<<row))fillRect(x+col*scale,y+row*scale,scale,scale,color);x+=6*scale;}}}
    void drawCentreString(const char* s,int x,int y,int font){drawString(s,x-textWidth(s,font)/2,y,font);}
    bool writePPM(const char* path){FILE* f=std::fopen(path,"wb");if(!f)return false;std::fprintf(f,"P6\n320 240\n255\n");for(uint16_t c:pixels){unsigned char rgb[]={static_cast<unsigned char>(((c>>11)&31)*255/31),static_cast<unsigned char>(((c>>5)&63)*255/63),static_cast<unsigned char>((c&31)*255/31)};std::fwrite(rgb,1,3,f);}std::fclose(f);return true;}
};
