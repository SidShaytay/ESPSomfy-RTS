#pragma once
#include "ModelEditor.h"
#include "ProtoSchema.h"
#include <ArduinoJson.h>
// Protocol serialization only; no live model, socket, edits or filesystem I/O.
void writeModelEditResult(ProtoWriter &writer, model_edit_type_t type,
                          const edit_result_t &result);
void writeModelCommitReport(ProtoWriter &writer, ModelCommitReport report);
const char *parseModelCommitPayload(JsonObjectConst payload);
