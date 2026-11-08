#pragma once
#include "LocalCatalogue.h"
#include "LocalEntityForm.h"
namespace LocalUi {
// Loop-owned navigation around the shared entity form. Pages own three copied
// labels and stable IDs; no model pointers or full-model JSON are retained.
class Administration {
  EntityAdmin::Backend &backend;
  CatalogueSource &catalogue;
  EntityAdmin::Form form;
  EntityAdmin::FormView frame;
  EntityAdmin::Authorization admitted;
  enum class Page : uint8_t { types, entries, entity } page=Page::types;
  uint64_t revision=0,observedAt=0;
  uint8_t cursor=0,anchor=0,returnId=0;
  bool active=false;
  bool guard(uint64_t now);
  bool entry(uint8_t ordinal,CatalogueEntry &out);
  void render();
  void resume();
  void close(EntityAdmin::Notice);
public:
  Administration(EntityAdmin::Backend &backend,CatalogueSource &source,ArduinoJson::Allocator &allocator)
    :backend(backend),catalogue(source),form(backend,allocator) {}
  bool open(uint64_t now);
  void tick(uint64_t now);
  void turn(int direction,uint64_t now);
  void activate(uint64_t now);
  void back(uint64_t now);
  void discard() {close(EntityAdmin::Notice::none);}
  bool closed() const {return !active;}
  const EntityAdmin::FormView &view() const {return frame;}
  uint64_t epoch() {return page==Page::entity?backend.revision():revision;}
};
static_assert(sizeof(Administration)<=1536,"One bounded administration page and entity form");
}
