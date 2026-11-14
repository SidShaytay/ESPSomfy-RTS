#pragma once
#include "LocalCatalogue.h"
#include "LocalEntityForm.h"
#include "LocalOrder.h"
#include "LocalAdvanced.h"
#include "LocalCalibration.h"
namespace LocalUi {
// Loop-owned navigation around the shared entity form. Pages own three copied
// labels and stable IDs; no model pointers or full-model JSON are retained.
class Administration {
  EntityAdmin::Backend &backend;
  CatalogueSource &catalogue;
  EntityAdmin::Form form;
  Ordering::Form ordering;
  Advanced::Form advanced;
  Calibration::Form calibration;
  EntityAdmin::FormView frame;
  EntityAdmin::Authorization admitted;
  enum class Page : uint8_t { types, entries, entity, order, more, advanced, calibration } page=Page::types;
  uint64_t revision=0,observedAt=0;
  uint8_t cursor=0,anchor=0,returnId=0,typeCursor=0;
  bool active=false;
  bool guard(uint64_t now);
  bool entry(uint8_t ordinal,CatalogueEntry &out);
  void render();
  void resume();
  void finishOrder();
  void finishAdvanced();
  void finishCalibration();
  void resumeAdvanced();
  bool renew();
  void close(EntityAdmin::Notice);
public:
  Administration(EntityAdmin::Backend &backend,CatalogueSource &source,ArduinoJson::Allocator &allocator,Advanced::Backend &advancedBackend, Calibration::Backend *calibrationBackend=nullptr)
    :backend(backend),catalogue(source),form(backend,allocator),ordering(backend,allocator),advanced(advancedBackend,allocator),calibration(calibrationBackend,source,allocator) {}
  bool open(uint64_t now);
  void tick(uint64_t now);
  void turn(int direction,uint64_t now);
  void activate(uint64_t now, bool human=true);
  void back(uint64_t now);
  void discard() {close(EntityAdmin::Notice::none);}
  bool closed() const {return !active;}
  bool takePrivateProjectionDiscard();
  const EntityAdmin::FormView &view() const {return frame;}
  uint64_t epoch() {return (page==Page::entity || page==Page::order || page==Page::advanced || page==Page::calibration)?backend.revision():revision;}
};
static_assert(sizeof(Administration)<=1704,"One bounded administration page and entity form");
}
