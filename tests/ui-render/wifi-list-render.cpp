#include "LocalEntityView.h"
#include "LocalUiRenderer.h"
#ifndef ENTITY_RENDER_SOURCE
#define ENTITY_RENDER_SOURCE "../../src/LocalEntityRenderer.cpp"
#endif
#include ENTITY_RENDER_SOURCE
#include <assert.h>
#include <fstream>
#include <string>
#include <vector>
using namespace LocalUi;
int main(int argc,char **argv) {
  assert(argc==2);Canvas canvas;std::vector<uint16_t> pixels(320*170);canvas.attach(pixels.data());
  EntityAdmin::FormView view;view.mode=EntityAdmin::Mode::wifiList;view.total=17;view.rowCount=4;
  strcpy(view.title,"Wi-Fi: first 16");
  const char *names[]={"Guest Lounge","Chill Vibes and WiFi","Hidden network","0123456789012345678901234567890"};
  const char *metadata[]={"-32dBm Secured","-55dBm Open","-96dBm Unknown","-63dBm Secured"};
  for(unsigned i=0;i<4;i++){strcpy(view.rows[i].value,names[i]);strcpy(view.rows[i].label,metadata[i]);}
  view.rows[1].selected=true;view.selected=1;canvas.paintEntity(view);const auto original=pixels;
  strcpy(view.hint,"Footer text must not crowd the fourth result");canvas.paintEntity(view);assert(pixels==original);
  strcpy(view.rows[3].value,"Fourth result changed");canvas.paintEntity(view);bool fourthVisible=false;
  for(unsigned y=132;y<164;y++)for(unsigned x=8;x<312;x++)fourthVisible|=pixels[y*320+x]!=original[y*320+x];
  assert(fourthVisible);
  strcpy(view.rows[3].value,names[3]);strcpy(view.rows[0].label,"-32dBm Open");canvas.paintEntity(view);bool lockVisible=false;
  for(unsigned y=43;y<60;y++)for(unsigned x=298;x<309;x++)lockVisible|=pixels[y*320+x]!=original[y*320+x];
  assert(lockVisible);
  strcpy(view.rows[0].label,metadata[0]);canvas.paintEntity(view);
  std::ofstream image(argv[1],std::ios::binary);image<<"P6\n320 170\n255\n";
  for(auto c:pixels){char rgb[3]={char((c>>11)*255/31),char(((c>>5)&63)*255/63),char((c&31)*255/31)};image.write(rgb,3);}
  printf("WiFi list production renderer: four rows, distinct lock and no crowded footer; FormView=%zu bytes\n",sizeof(view));
}
