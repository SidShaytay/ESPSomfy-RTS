#include "LocalSystem.h"
#include "LocalUiRenderer.h"
#include "../../src/LocalEntityRenderer.cpp"
#include <cassert>
#include <fstream>
#include <vector>
using namespace LocalUi;
struct Backend final:System::Backend {
  Battery pack{true,true,false,87,0,4137};
  SecuritySnapshot authorization()override{return {AuthMode::pin,true,false,3,5};}
  void activity()override{}
  RestartResult restart(uint32_t,uint32_t)override{return {};}
  RestartSnapshot status()override{return {};}
  bool battery(Battery &out)override{out=pack;return true;}
  bool setChargeLimit(uint8_t limit)override{pack.limit=limit;return true;}
};
static void save(Canvas &canvas,std::vector<uint16_t>&pixels,const EntityAdmin::FormView &view,const char *dir,const char *name){
 canvas.paintEntity(view);std::ofstream f(std::string(dir)+"/battery-"+name+".ppm",std::ios::binary);f<<"P6\n320 170\n255\n";
 for(auto c:pixels){char rgb[]={char((c>>11)*255/31),char(((c>>5)&63)*255/63),char((c&31)*255/31)};f.write(rgb,3);}
}
// The value column starts at x=200 and is 106 px wide; nothing may spill past the panel.
static void fits(Canvas &canvas,const EntityAdmin::FormView &view){
 for(uint8_t i=0;i<view.rowCount;++i){
  char value[48];snprintf(value,sizeof(value),"%s",view.rows[i].value);
  char shown[48];tail(canvas,value,shown,106);assert(std::string(shown)==value);
 }
 char first[48],second[48];lines(canvas,view.hint,first,second);assert(std::string(first)+(second[0]?" ":"")+second==view.hint);
}
int main(int argc,char **argv){
 assert(argc==2);Canvas canvas;std::vector<uint16_t>pixels(320*170);canvas.attach(pixels.data());
 Backend backend;System::Form form(backend);EntityAdmin::FormView view;
 assert(form.open(0,System::Menu::battery));form.render(view);save(canvas,pixels,view,argv[1],"full");fits(canvas,view);
 for(int i=0;i<4;++i)form.turn(1,1);form.render(view);save(canvas,pixels,view,argv[1],"choose-60");fits(canvas,view);
 form.activate(2);form.render(view);save(canvas,pixels,view,argv[1],"set-60");fits(canvas,view);
 backend.pack.limitFailed=true;form.render(view);save(canvas,pixels,view,argv[1],"retrying");fits(canvas,view);
 backend.pack={};form.discard();form.open(0,System::Menu::battery);form.render(view);save(canvas,pixels,view,argv[1],"unsupported");fits(canvas,view);
}
