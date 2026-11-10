#include "LocalUpdates.h"
#include "LocalJsonAllocator.h"
#include "LocalUiRenderer.h"
#include "../../src/LocalEntityRenderer.cpp"
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <vector>
using namespace LocalUi;
struct Backend final:Updates::Backend {
  GitUpdateSnapshot value;
  unsigned installs=0;
  Backend(){value.supported=true;value.phase=GitUpdatePhase::idle;strcpy(value.installed,"v4.6.1");}
  SecuritySnapshot authorization()override{return {AuthMode::pin,true,false,3,5};}
  void activity()override{}
  GitUpdateSnapshot status()override{return value;}
  GitUpdateBootStatus boot()override{return {};}
  GitUpdateRequest check(uint32_t,uint32_t,uint64_t &op)override{value.operation=op=7;value.reason=GitUpdateReason::none;value.phase=GitUpdatePhase::checking;value.busy=value.cancellable=true;return GitUpdateRequest::accepted;}
  GitUpdateRequest install(uint64_t,uint32_t,uint32_t,uint64_t &op)override{++installs;value.operation=op=8;value.phase=GitUpdatePhase::downloading;value.busy=value.cancellable=true;return GitUpdateRequest::accepted;}
  bool cancel(uint64_t,uint32_t,uint32_t)override{return true;}
};
static void save(Canvas &canvas,std::vector<uint16_t>&pixels,const EntityAdmin::FormView &view,const char *dir,const char *name){
 canvas.paintEntity(view);std::ofstream f(std::string(dir)+"/update-"+name+".ppm",std::ios::binary);f<<"P6\n320 170\n255\n";
 for(auto c:pixels){char rgb[]={char((c>>11)*255/31),char(((c>>5)&63)*255/63),char((c&31)*255/31)};f.write(rgb,3);}
}
int main(int argc,char **argv){
 assert(argc==2);Canvas canvas;std::vector<uint16_t>pixels(320*170);canvas.attach(pixels.data());
 Backend backend;JsonAllocator pool(4096,malloc,free);Updates::Form form(backend,pool);EntityAdmin::FormView view;
 assert(form.open(0));form.render(view);assert(view.rowCount==2&&view.total==2);save(canvas,pixels,view,argv[1],"check");
 // Actual glyph bounds, not guessed font height: installed label and version
 // must share a readable single row rather than crowd the selected Check row.
 auto installed=view;installed.rowCount=1;installed.rows[0].selected=false;installed.hint[0]=0;
 canvas.paintEntity(installed);int labelTop=170,labelBottom=-1,valueTop=170,valueBottom=-1;
 for(int y=35;y<125;y++)for(int x=12;x<308;x++){
  if(pixels[y*320+x]==foreground){labelTop=std::min(labelTop,y);labelBottom=std::max(labelBottom,y);}
  if(pixels[y*320+x]==muted){valueTop=std::min(valueTop,y);valueBottom=std::max(valueBottom,y);}
 }
 assert(labelBottom>=labelTop&&valueBottom>=valueTop);
 assert(abs(labelBottom-valueBottom)<=2 && abs(labelTop-valueTop)<=3);
 const auto installedPixels=pixels;canvas.paintEntity(view);
 for(int y=valueTop;y<=valueBottom;y++)for(int x=12;x<308;x++)if(installedPixels[y*320+x]==muted)assert(pixels[y*320+x]==muted);
 // Changing a long version cannot overwrite the Installed label or escape
 // into the two-line explanation area.
 auto shortVersion=installed;canvas.paintEntity(shortVersion);const auto shortPixels=pixels;
 memset(installed.rows[0].value,'W',46);installed.rows[0].value[46]=0;canvas.paintEntity(installed);
 for(int y=35;y<170;y++)for(int x=0;x<320;x++)if(x<180 || y>=125)assert(pixels[y*320+x]==shortPixels[y*320+x]);
 backend.value.reason=GitUpdateReason::transport;form.render(view);save(canvas,pixels,view,argv[1],"transport");
 char first[48],second[48];lines(canvas,view.hint,first,second);assert(second[0]);
 assert(std::string(first)+" "+second==view.hint);
 for(const char *line:{first,second}){int16_t x,y;uint16_t w,h;canvas.getTextBounds(line,0,0,&x,&y,&w,&h);assert(w<=296);}
 form.activate(1);form.render(view);assert(!strchr(view.rows[0].label,'%'));assert(!strcmp(view.rows[0].label,"Checking GitHub..."));save(canvas,pixels,view,argv[1],"checking");
 backend.value.busy=backend.value.cancellable=false;backend.value.phase=GitUpdatePhase::available;backend.value.canInstall=true;backend.value.candidate.generation=12;strcpy(backend.value.candidate.version,"v4.7.0");
 form.tick(2);form.render(view);assert(view.mode==EntityAdmin::Mode::updateReview&&view.selected==0&&backend.installs==0);save(canvas,pixels,view,argv[1],"review-after-check");
 form.activate(3);form.render(view);assert(view.mode==EntityAdmin::Mode::updateStatus&&backend.installs==0);save(canvas,pixels,view,argv[1],"available");
 form.turn(1,4);form.activate(5);form.render(view);assert(view.mode==EntityAdmin::Mode::updateReview&&view.selected==0);save(canvas,pixels,view,argv[1],"review");
 form.turn(1,6);form.activate(7);form.render(view);assert(backend.installs==1);save(canvas,pixels,view,argv[1],"progress");
 backend.value.phase=GitUpdatePhase::error;backend.value.reason=GitUpdateReason::activationFailed;form.render(view);save(canvas,pixels,view,argv[1],"recovery");
 puts("Actual update Form/GFX: independent compact version rows, long-version clipping, two-line errors, review and progress passed.");
}
