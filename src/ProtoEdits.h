#ifndef PROTO_EDITS_H
#define PROTO_EDITS_H
#include <stdint.h>
#include <ArduinoJson.h>
#include "ProtoParse.h"
#include "ModelEditor.h"

// Socket protocol 2's edit ops and the reads an edit form needs
// (docs/PROTOCOL.md).  An edit op is the edit route of the same name: its
// payload, in full names, is what loop() applies with applyModelEdit(), so the
// rules stay in one place.  Kept free of the shade model and the web server so
// it can be unit tested on the PC (pio test -e native).

struct edit_form_t {
  const proto_field_t *fields;
  uint8_t count;
  proto_field_t identity; // count for add; stable ID supplied separately on save
};
// Stable metadata for add/save room/shade/group; nullptr for action-only edits.
// Field kinds/names/keys come from protoField(). Model validation still owns
// entity ranges, capabilities, unique identities and cross-field constraints.
const edit_form_t *editForm(model_edit_type_t type);

// The edit an op asks for; false for a type that isn't an edit op.
bool protoEditType(uint8_t type, model_edit_type_t &out);
// The request of an edit route, from the op's payload d: full names for the
// short keys, and booleans as true and false.  A sort order's request is its
// list of ids.  nullptr when it is good; otherwise what is wrong with it, for
// a bad request reply.  As with every op, only what was sent is in it.
const char *expandEditPayload(model_edit_type_t type, JsonObjectConst d, JsonDocument &doc, proto_error_buffer_t &why);

// A read op's payload (getNextRoom, getNextShade, getNextGroup and
// groupOptions, answered by loop() from the shade model): groupOptions' group,
// left 0 for the others.  nullptr when it is good; otherwise what is wrong
// with it.
const char *parseReadPayload(uint8_t type, JsonObjectConst d, uint8_t &groupId);
#endif
