#include "LocalEntityView.h"
#include "LiftGuideView.h"
#include "LocalPrimaryCounter.h"
#include "LocalUiRenderer.h"
#include "../../src/LocalEntityRenderer.cpp"
#include <assert.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <fstream>
#include <string.h>
#include <vector>
using namespace LocalUi;
static void save(Canvas &canvas, uint16_t *pixels,
                 const EntityAdmin::FormView &view, const char *out,
                 const char *name) {
  canvas.paintEntity(view);
  std::ofstream file(std::string(out) + "/" + name + ".ppm", std::ios::binary);
  file << "P6\n320 170\n255\n";
  for (unsigned i = 0; i < 320 * 170; i++) {
    unsigned c = pixels[i];
    char rgb[3] = {char((c >> 11) * 255 / 31), char(((c >> 5) & 63) * 255 / 63),
                   char((c & 31) * 255 / 31)};
    file.write(rgb, 3);
  }
}
int main(int argc, char **argv) {
  assert(argc == 2);
  Canvas canvas;
  std::vector<uint16_t> pixels(320 * 170);
  canvas.attach(pixels.data());
  // Real production guide wording and production renderer; synthetic state,
  // deliberately separate from physical-board evidence.
  for(auto stage : {LiftGuideStage::reference,LiftGuideStage::downTakeup,
      LiftGuideStage::reverseTakeupUp,LiftGuideStage::review,LiftGuideStage::complete}) {
    EntityAdmin::FormView form;
    form.mode=EntityAdmin::Mode::fields;
    const auto guide=liftGuideView(stage,false,false,false);
    snprintf(form.title,sizeof(form.title),"%s",guide.title);
    snprintf(form.hint,sizeof(form.hint),"%s",guide.hint);
    form.total=guide.count;form.rowCount=guide.count>3?3:guide.count;
    for(unsigned i=0;i<form.rowCount;++i) {
      snprintf(form.rows[i].label,sizeof(form.rows[i].label),"%s",guide.choices[i].label);
      form.rows[i].selected=i==0;
    }
    char name[48];snprintf(name,sizeof(name),"entity-calibration-%u",unsigned(stage));
    save(canvas,pixels.data(),form,argv[1],name);
  }
  EntityAdmin::FormView view;
  view.mode = EntityAdmin::Mode::fields;
  view.id = 4;
  strcpy(view.title, "Edit Room 4");
  view.total = 5;
  view.rowCount = 3;
  strcpy(view.rows[0].label, "Name");
  strcpy(view.rows[0].value, "Office / \xe2\x98\x83");
  view.rows[0].selected = true;
  strcpy(view.rows[1].label, "Sort order");
  strcpy(view.rows[1].value, "2");
  strcpy(view.rows[2].label, "Save");
  strcpy(view.hint, "Apply fields to draft, then Save. Back discards.");
  save(canvas, pixels.data(), view, argv[1], "entity-room-fields");
  view.mode = EntityAdmin::Mode::confirm;
  view.rowCount = 2;
  strcpy(view.rows[0].label, "Cancel");
  view.rows[0].value[0] = 0;
  strcpy(view.rows[1].label, "Delete");
  view.rows[1].value[0] = 0;
  strcpy(view.hint, "Delete Room 4? Confirm this exact ID.");
  save(canvas, pixels.data(), view, argv[1], "entity-room-confirm");
  const auto before = pixels;
  view.notice = EntityAdmin::Notice::busy;
  save(canvas, pixels.data(), view, argv[1],
       "entity-room-confirm-stale-notice");
  assert(before == pixels); // The action-specific warning always wins.
  view.mode = EntityAdmin::Mode::replace;
  strcpy(view.title, "Room 9: Sort order");
  strcpy(view.rows[1].label, "Replace invalid value");
  strcpy(view.hint, "Original kept until Apply; Save persists replacement.");
  save(canvas, pixels.data(), view, argv[1], "entity-room-replace-invalid");
  const auto replacement = pixels;
  view.notice = EntityAdmin::Notice::failed;
  save(canvas, pixels.data(), view, argv[1],
       "entity-room-replace-stale-notice");
  assert(replacement == pixels);
  view.mode = EntityAdmin::Mode::result;
  view.notice = EntityAdmin::Notice::appliedNotSaved;
  strcpy(view.title, "Room 9 file result");
  strcpy(view.rows[0].label, "Close");
  strcpy(view.rows[1].label, "Retry model file save");
  strcpy(view.hint, "Room 9 applied. Retry saves current model file only.");
  save(canvas, pixels.data(), view, argv[1], "entity-room-file-failed");
  view.notice = EntityAdmin::Notice::uncertain;
  save(canvas, pixels.data(), view, argv[1], "entity-room-file-uncertain");
  view.mode = EntityAdmin::Mode::text;view.textLength=20;view.textLimit=20;
  strcpy(view.title,"Edit Room 9");
  view.notice = EntityAdmin::Notice::none;
  view.rowCount = 3;
  strcpy(view.rows[0].label, "Name");
  strcpy(view.rows[0].value, "12345678901234567END");
  strcpy(view.rows[1].label, "Character");
  strcpy(view.rows[1].value, "Apply to draft");
  view.rows[2] = {};
  strcpy(view.hint, "20/20 bytes. Apply to draft; Back cancels.");
  save(canvas, pixels.data(), view, argv[1], "entity-room-text");
  // A legal 20-byte name can still exceed the viewport in the real font.
  strcpy(view.rows[0].value, "WWWWWWWWWWWWWWWWWEND");
  assert(strlen(view.rows[0].value) == 20);
  save(canvas, pixels.data(), view, argv[1], "entity-room-wide-tail");
  const auto wide = pixels;
  char expected[48];
  const char *start = view.rows[0].value;
  int16_t x, y;
  uint16_t width, height;
  canvas.getTextBounds(start, 0, 0, &x, &y, &width, &height);
  assert(width > 246);
  do {
    ++start;
    snprintf(expected, sizeof(expected), "...%s", start);
    canvas.getTextBounds(expected, 0, 0, &x, &y, &width, &height);
  } while (width > 246);
  assert(width <= 246 && !strcmp(expected + strlen(expected) - 3, "END"));
  strcpy(view.rows[0].value, expected);
  save(canvas, pixels.data(), view, argv[1], "entity-room-wide-tail-expected");
  assert(wide ==
         pixels); // Compare the complete actual render, not source text.
  strcpy(view.rows[0].value, "WWWWWWWWWWWWWWWWWXYZ");
  save(canvas, pixels.data(), view, argv[1], "entity-room-wide-tail-alternate");
  bool lastChanged = false;
  for (unsigned row = 34; row < 54; row++)
    for (unsigned col = 170; col < 258; col++)
      lastChanged |= pixels[row * 320 + col] != wide[row * 320 + col];
  assert(lastChanged); // The newest suffix reaches the visible right edge.
  view = {};
  view.kind = EntityAdmin::Kind::shade;
  view.id = 4;
  view.mode = EntityAdmin::Mode::validation;
  view.rowCount = view.total = 2;
  strcpy(view.title, "Shade 4: Up GPIO");
  strcpy(view.rows[0].label, "Revise field"); view.rows[0].selected = true;
  strcpy(view.rows[1].label, "Cancel form");
  strcpy(view.hint, "Revert inactive GPIO draft or select GPIO protocol.");
  save(canvas, pixels.data(), view, argv[1], "entity-device-refused");
  const auto refusal = pixels;
  view.notice = EntityAdmin::Notice::busy;
  save(canvas, pixels.data(), view, argv[1], "entity-device-refused-stale-notice");
  assert(refusal == pixels); // Never hide the current actionable reason.
  strcpy(view.hint, "Revert inactive GPIO draft or select radio protocol.");
  save(canvas, pixels.data(), view, argv[1], "entity-device-refused-alternate");
  bool reasonChanged = false;
  for (unsigned row = 128; row < 154; row++)
    for (unsigned col = 12; col < 308; col++)
      reasonChanged |= pixels[row * 320 + col] != refusal[row * 320 + col];
  assert(reasonChanged); // The last words reach the actual second display line.
  view.mode = EntityAdmin::Mode::draft;
  view.notice = EntityAdmin::Notice::none;
  view.rowCount = view.total = 3;
  strcpy(view.rows[0].label, "Cancel");
  strcpy(view.rows[1].label, "Edit draft");
  strcpy(view.rows[2].label, "Revert field draft");
  strcpy(view.hint, "Revert keeps other drafts.");
  save(canvas, pixels.data(), view, argv[1], "entity-device-draft");
  view.mode = EntityAdmin::Mode::confirm;
  view.rowCount = view.total = 2;
  strcpy(view.title, "Save Shade 4");
  strcpy(view.rows[1].label, "Save");
  strcpy(view.hint, "May change live GPIO, motor timing and RF identity.");
  save(canvas, pixels.data(), view, argv[1], "entity-device-save-confirm");
  // Advanced uses this same borrowed frame/renderer. Check the complete
  // configuration-only warning and maximum-width association identity.
  view={};view.mode=EntityAdmin::Mode::confirm;view.rowCount=view.total=2;
  strcpy(view.title,"#27 Office");strcpy(view.rows[0].label,"Cancel");view.rows[0].selected=true;
  strcpy(view.rows[1].label,"Remove #4294967295");
  const char *advancedWarnings[]={"Marker only. Motor pairing is unchanged.",
    "Remove all stale links. No physical unpair.","Stop future repeats. No physical unpair.",
    "Remove association. No physical unpair."};
  for(unsigned i=0;i<4;i++) {
    strcpy(view.hint,advancedWarnings[i]);
    char file[48];snprintf(file,sizeof(file),"entity-advanced-confirm-%u",i);
    save(canvas,pixels.data(),view,argv[1],file);
    std::string first=view.hint,second;int16_t x,y;uint16_t width,height;
    canvas.getTextBounds(first.c_str(),0,0,&x,&y,&width,&height);
    while(width>296) {
      const auto space=first.rfind(' ');assert(space!=std::string::npos);
      second=first.substr(space+1)+(second.empty()?"":" ")+second;
      first.resize(space);canvas.getTextBounds(first.c_str(),0,0,&x,&y,&width,&height);
    }
    canvas.getTextBounds(second.c_str(),0,0,&x,&y,&width,&height);assert(width<=296);
    canvas.getTextBounds(view.rows[1].label,0,0,&x,&y,&width,&height);assert(width<=284);
  }
  const auto exactAddress=pixels;strcpy(view.rows[1].label,"Remove #4294967294");
  save(canvas,pixels.data(),view,argv[1],"entity-advanced-confirm-address-changed");
  bool addressChanged=false;
  for(unsigned row=74;row<104;row++)for(unsigned col=130;col<308;col++)
    addressChanged|=pixels[row*320+col]!=exactAddress[row*320+col];
  assert(addressChanged); // The final identity digits are not clipped away.
  view={};view.mode=EntityAdmin::Mode::wifiReview;view.total=view.rowCount=2;
  strcpy(view.title,"Review WiFi #16");strcpy(view.rows[0].label,"Cancel");view.rows[0].selected=true;
  strcpy(view.rows[1].label,"Use SSID in settings draft");strcpy(view.rows[2].label,"AA:BB:CC:DD:EE:FF");
  memset(view.rows[2].value,'W',29);strcpy(view.rows[2].value+29,"END");
  strcpy(view.hint,"Draft only. Review password and Save separately.");
  strcpy(view.rows[0].value,"Ch255 -32768dBm Unknown");
  save(canvas,pixels.data(),view,argv[1],"entity-wifi-review-wide");
  char firstLine[48],secondLine[48];lines(canvas,view.rows[2].value,firstLine,secondLine);
  assert(std::string(firstLine)+secondLine==view.rows[2].value);
  int16_t wx,wy;uint16_t ww,wh;
  canvas.getTextBounds(firstLine,0,0,&wx,&wy,&ww,&wh);assert(ww<=296);
  canvas.getTextBounds(secondLine,0,0,&wx,&wy,&ww,&wh);assert(ww<=296);
  std::vector<uint16_t> previousMetadata;unsigned securityIndex=0;
  for(const char*metadata:{"Ch1 -32dBm Open","Ch14 -96dBm Secured","Ch255 -32768dBm Unknown"}) {
    canvas.getTextBounds(metadata,0,0,&wx,&wy,&ww,&wh);assert(ww<=296);
    strcpy(view.rows[0].value,metadata);
    const auto name="entity-wifi-security-"+std::to_string(securityIndex++);
    save(canvas,pixels.data(),view,argv[1],name.c_str());
    if(!previousMetadata.empty()) {
      bool changed=false;for(unsigned y=84;y<103;y++)for(unsigned x=12;x<308;x++)changed|=pixels[y*320+x]!=previousMetadata[y*320+x];assert(changed);
    }
    previousMetadata=pixels;
  }
  {
    EntityAdmin::FormView diagnostics;diagnostics.mode=EntityAdmin::Mode::stackedRows;
    strcpy(diagnostics.title,"Pulse timings");diagnostics.total=151;diagnostics.rowCount=3;
    strcpy(diagnostics.rows[0].label,"1-2 (microseconds)");
    strcpy(diagnostics.rows[0].value,"4294967295 4294967295");
    strcpy(diagnostics.rows[1].label,"#18446744073709551614");
    strcpy(diagnostics.rows[1].value,"FFFFFFFF code 65535 Invalid");
    strcpy(diagnostics.hint,"Copied raw timings. Back: frame. No transmit action.");
    save(canvas,pixels.data(),diagnostics,argv[1],"entity-diagnostic-pulses");
    for(const char *text:{diagnostics.rows[0].value,diagnostics.rows[1].label,diagnostics.rows[1].value}) {
      canvas.getTextBounds(text,0,0,&wx,&wy,&ww,&wh);assert(ww<=284);
    }
    canvas.paintEntity(view); // Restore WiFi frame for its existing final-glyph check.
  }
  const auto visibleSSID=pixels;view.rows[2].value[31]='X';canvas.paintEntity(view);
  bool tailVisible=false;for(unsigned y=35;y<72;y++)for(unsigned x=12;x<308;x++)tailVisible|=pixels[y*320+x]!=visibleSSID[y*320+x];assert(tailVisible);
  for(const char*warning:{"May interrupt WiFi. Results only; no connection or Save.","Waiting up to 30s. An accepted shared scan may finish.","First 16 results. Others/hidden SSIDs: enter manually.","Hidden SSID: enter its name manually in settings.","Draft only. Review password and Save separately."}) {
    lines(canvas,warning,firstLine,secondLine);assert(std::string(firstLine)+(secondLine[0]?" ":"")+secondLine==warning);
    canvas.getTextBounds(firstLine,0,0,&wx,&wy,&ww,&wh);assert(ww<=296);canvas.getTextBounds(secondLine,0,0,&wx,&wy,&ww,&wh);assert(ww<=296);
  }
  EntityAdmin::FormView learn;learn.mode=EntityAdmin::Mode::learnReview;learn.total=learn.rowCount=2;
  strcpy(learn.title,"#32 Office device");strcpy(learn.rows[0].label,"Cancel");learn.rows[0].selected=true;
  strcpy(learn.rows[1].label,"Link received remote");strcpy(learn.rows[2].label,"#16777215 Code 65535");
  strcpy(learn.rows[2].value,"RTW 80-bit MyUpDown RSSI unavailable");
  strcpy(learn.hint,"Links address/counter; observed format is not a filter.");
  save(canvas,pixels.data(),learn,argv[1],"entity-learn-review-max");const auto completeLearn=pixels;
  for(const char*complete:{learn.rows[2].value,learn.hint}){
    char first[48],second[48];lines(canvas,complete,first,second);
    assert(std::string(first)+(second[0]?" ":"")+second==complete);
    int16_t x,y;uint16_t w,h;canvas.getTextBounds(first,0,0,&x,&y,&w,&h);assert(w<=296);
    canvas.getTextBounds(second,0,0,&x,&y,&w,&h);assert(w<=296);
  }
  learn.rows[2].label[8]='4';canvas.paintEntity(learn);bool exactAddressVisible=false;
  for(unsigned y=35;y<58;y++)for(unsigned x=12;x<308;x++)exactAddressVisible|=pixels[y*320+x]!=completeLearn[y*320+x];assert(exactAddressVisible);
  const char *learnHints[]={"Future received commands may affect device; Link sends no RF.","FIFO; expires after 5s. Back cancels. No automatic retry.","Counter/link applied. Commit file only; never Link again.","Link not applied; stored code unknown. Do not replay/reset.","Earlier link kept. Counter state unknown. Do not replay.","Received frame changed. Choose and review a frame again.","Stored/live code newer. Cancel, then Begin observation again.","Counter storage read/open failed. Nothing was linked."};
  for(const char*hint:learnHints){
    assert(strlen(hint)<sizeof(learn.hint));char first[48],second[48];lines(canvas,hint,first,second);
    assert(std::string(first)+(second[0]?" ":"")+second==hint);
    int16_t x,y;uint16_t w,h;canvas.getTextBounds(first,0,0,&x,&y,&w,&h);assert(w<=296);canvas.getTextBounds(second,0,0,&x,&y,&w,&h);assert(w<=296);
  }
  learn={};learn.mode=EntityAdmin::Mode::validation;learn.rowCount=learn.total=2;learn.selected=1;
  strcpy(learn.title,"Linked; file uncertain");strcpy(learn.rows[0].label,"Retry model file save");strcpy(learn.rows[1].label,"Close");learn.rows[1].selected=true;
  strcpy(learn.hint,learnHints[2]);save(canvas,pixels.data(),learn,argv[1],"entity-learn-file-uncertain");
  learn.rowCount=learn.total=1;learn.selected=0;strcpy(learn.rows[0].label,"Close");learn.rows[0].selected=true;learn.rows[1]={};
  strcpy(learn.title,"Counter uncertain");strcpy(learn.hint,learnHints[3]);save(canvas,pixels.data(),learn,argv[1],"entity-learn-counter-uncertain");
  canvas.setFont(&FreeSansBold12pt7b);
  for(const char*title:{"Remote linked","Linked; file not saved","Linked; file uncertain","Counter uncertain","Counter blocked","Link recovery blocked","Link queued","File save queued","Link not applied"}) {
    int16_t x,y;uint16_t w,h;canvas.getTextBounds(title,0,0,&x,&y,&w,&h);assert(w<=296);
  }
  canvas.setFont(&FreeSans9pt7b);
  PrimaryCounterReview primarySource;primarySource.shadeId=32;primarySource.address=16777215;primarySource.primary=65535;primarySource.stored={65535,true};
  memset(primarySource.matching,0xff,sizeof(primarySource.matching));for(auto &code:primarySource.codes)code=65535;
  PrimaryCounterDraft primary;primary.open(primarySource);EntityAdmin::FormView primaryView;primary.render(primaryView,32);
  save(canvas,pixels.data(),primaryView,argv[1],"entity-primary-current-max");const auto primaryPixels=pixels;
  auto alteredPrimary=primaryView;alteredPrimary.rows[2].label[8]='4';canvas.paintEntity(alteredPrimary);bool primaryLastDigit=false;
  for(unsigned y=35;y<58;y++)for(unsigned x=12;x<308;x++)primaryLastDigit|=pixels[y*320+x]!=primaryPixels[y*320+x];assert(primaryLastDigit);
  primary.turn(1,1);primary.activate(2);primary.turn(-1,3);primary.activate(4);primary.turn(1,5);primary.activate(6);primary.render(primaryView,32);
  save(canvas,pixels.data(),primaryView,argv[1],"entity-primary-lowering-review");
  const char *primaryWarnings[]={primaryView.rows[2].value,primaryView.hint,"Stored code unknown; running code unchanged. No replay/reset.","Counter applied. Retry file only; never repeat correction.","ALL aliases/storage/model verified. Correction sent no RF.","Queue/owner/radio/restart unavailable. No automatic retry.","Close and reopen primary code to review fresh live/NVS state."};
  for(const char*complete:primaryWarnings) {
    char first[48],second[48];lines(canvas,complete,first,second);assert(std::string(first)+(second[0]?" ":"")+second==complete);
    int16_t x,y;uint16_t w,h;canvas.getTextBounds(first,0,0,&x,&y,&w,&h);assert(w<=296);canvas.getTextBounds(second,0,0,&x,&y,&w,&h);assert(w<=296);
  }
  primary.complete({CounterDomainState::modelUncertain,true,{ModelCommitState::uncertain,ModelCommitStage::verifyBytes}});primary.render(primaryView,32);
  save(canvas,pixels.data(),primaryView,argv[1],"entity-primary-file-uncertain");
  canvas.setFont(&FreeSansBold12pt7b);
  for(const char*title:{"#32 Primary code","Correction queued","Counter work busy","Counter saved","Code set; file not saved","Counter uncertain","Counter blocked","Counter review changed","Counter not changed"}) {
    int16_t x,y;uint16_t w,h;canvas.getTextBounds(title,0,0,&x,&y,&w,&h);assert(w<=296);
  }
  canvas.setFont(&FreeSans9pt7b);
  puts("Production entity renderer: fields, identity confirmation, complete "
       "applied/file-only failure and text states passed.");
}
