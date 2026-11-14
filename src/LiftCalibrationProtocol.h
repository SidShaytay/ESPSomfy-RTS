#pragma once
#include <ArduinoJson.h>
#include "LiftGuide.h"
#include "ProtoSchema.h"
#include "LiftCalibrationRequest.h"
const char *parseLiftCalibrationPayload(JsonObjectConst, LiftCalibrationRequest &);
