#include "Transceiver.h"
#include "SettingsLoad.h"
#include "SettingsTypedValidation.h"
bool transceiver_config_t::loadReadOnly() {
  transceiver_config_t draft=*this;SettingsLoadReader reader("CC1101");
  int8_t protocol=int8_t(draft.proto);
  reader.u8("type",draft.type);
  reader.u8("TXPin",draft.TXPin);
  reader.u8("RXPin",draft.RXPin);
  reader.u8("SCKPin",draft.SCKPin);
  reader.u8("MOSIPin",draft.MOSIPin);
  reader.u8("MISOPin",draft.MISOPin);
  reader.u8("CSNPin",draft.CSNPin);
  reader.real("frequency",draft.frequency);
  reader.real("deviation",draft.deviation);
  reader.real("rxBandwidth",draft.rxBandwidth);
  reader.flag("enabled",draft.enabled);
  reader.i8("txPower",draft.txPower);
  reader.i8("proto",protocol);
  reader.flag("noiseDetect",draft.noiseDetection);
  draft.startupInitAllowed=true;
  reader.flag("radioInit",draft.startupInitAllowed);
  auto status=reader.status();
  if(status.readable()) {
    draft.proto=radio_proto(uint8_t(protocol));draft.radioInit=false;
    draft.applyBoardPins(); // Existing fixed-board normalization; no hardware IO.
    if(!validSettings(draft))status.state=SettingsLoadState::invalid;
    *this=draft;
  } else {startupInitAllowed=false;radioInit=false;}
  loadStatus=status;return status.usable();
}
