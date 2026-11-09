#include "RestoreCompletion.h"
#include "SettingsDurability.h"
#include "MqttConnection.h"
#include "outputs/ProtoSocket.h"
extern MqttConnection mqtt;
void completeRestoreModel(const RestoreApplyResult &result){
  if(!result.modelApplied)return;
  protoSocket.resyncAll();
  ModelCommitReport report;report.file=result.modelFile;
  protoSocket.modelPersistence(report);
  // A returned applied MQTT bit certifies its live hook completed. Merely
  // requesting/verifying storage is not a reason to skip model reconnection.
  if(result.settings.saved() &&
      (result.settings.appliedNamespaces&settingsNamespaceBit(SettingsNamespace::mqtt)))return;
  mqtt.disconnect();
  mqtt.lastConnect=0; // Ordinary loop reconnects; never connect under restore's pause.
}
