#include "esp_log.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include "ConfigSettings.h"
#include "ESPNetwork.h"
#include "entryPoints/Web.h"
#include "outputs/ProtoSocket.h"
#include "Utils.h"
#include "Somfy.h"
#include "MqttConnection.h"
#include "LoopQueue.h"
#include "RebootDeadline.h"
#include "ShadeRequests.h"
#include "SomfyShadeOps.h"
#include "esp_core_dump.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"
#include <inttypes.h>
#include "Timing.h"
#include "BoardProfile.h"
#include "GitUpdate.h"
#include "GitUpdateMemory.h"
#include "GitUpdateHealth.h"
#include "GitUpdateIdentity.h"
#include "GitUpdateAssets.h"
#include "GitUpdateBootloader.h"
#include "ConfigFile.h"
#include "UpdateSnapshot.h"

static const char *TAG = "Main";
#define LOOP_SLOW_MS 100
// platformio.ini can set its own, along with a matching monitor_speed.
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif
// How long an updated firmware has to run, and then have the network up, before
// it is kept.  A reset before then rolls back to the previous firmware.
#define OTA_PROVEN_MS 60000

// Overrides the core's weak hook so it leaves an updated firmware unverified
// at boot.  loop() confirms it once otaProven().
extern "C" bool verifyRollbackLater() { return true; }
static bool otaPending = false;
extern SomfyShadeController somfy;
extern MqttConnection mqtt;
extern LoopQueue loopQueue;
#if defined(ESPSOMFY_GITHUB_UPDATES)
static GitUpdateHealth updateHealth;
static bool filesystemHealthy = false;
static bool configHealthy = false;
static bool applicationHealthy = false;
static bool bootloaderHealthy = false;
static bool updateQuiesced = false;

// Called on loop() alone after the entire candidate has been verified. Refuse
// to interrupt a moving shade or a pending radio job (including its outcome).
static bool prepareGitInstall() {
  if(!filesystemHealthy || !configHealthy || !applicationHealthy || !bootloaderHealthy || (somfy.transceiver.config.enabled && !somfy.transceiver.available())) return false;
  for(auto &shade : somfy.shades) {
    if(shade.getShadeId() != 255 && !shade.readyForUpdate()) return false;
  }
  // Allocate before taking either admission gate; OOM leaves RX and callers
  // untouched. All serialized record counts are bounded by the model limits.
  String serialized;
  if(!serialized.reserve(UPDATE_SNAPSHOT_CAPACITY)) return false;
  if(!loopQueue.freezeIfEmpty()) return false;
  if(!somfy.transceiver.suspendForUpdateIfIdle()) {
    loopQueue.resumeAdmission();
    return false;
  }
  // Serialize in RAM first, write a temporary file, and compare it back before
  // atomic replacement. A short write must not destroy the current config.
  ShadeConfigFile config;
  config.beginRAM(&serialized);
  const bool encoded = config.save(&somfy);
  config.end();
  bool saved = encoded && updateSnapshotComplete(config.header, serialized.length());
  File temporary = LittleFS.open("/shades.ota.tmp", "w");
  saved = saved && temporary && temporary.write(reinterpret_cast<const uint8_t *>(serialized.c_str()), serialized.length()) == serialized.length();
  if(temporary) { temporary.flush(); temporary.close(); }
  if(saved) {
    File check = LittleFS.open("/shades.ota.tmp", "r");
    saved = check && check.size() == serialized.length();
    uint8_t chunk[256];
    for(size_t offset = 0; saved && offset < serialized.length();) {
      const size_t count = std::min(sizeof(chunk), serialized.length() - offset);
      saved = check.read(chunk, count) == count && memcmp(chunk, serialized.c_str() + offset, count) == 0;
      offset += count;
    }
    check.close();
  }
  saved = saved && ShadeConfigFile::validateFile("/shades.ota.tmp") && LittleFS.rename("/shades.ota.tmp", "/shades.cfg");
  if(!saved) {
    LittleFS.remove("/shades.ota.tmp");
    somfy.transceiver.resumeAfterUpdatePreparation();
    loopQueue.resumeAdmission();
    return false;
  }
  somfy.transceiver.end();
  mqtt.end();
  updateQuiesced = true;
  return true;
}
#endif

ConfigSettings settings;
Web webServer;
ESPNetwork net;
rebootDelay_t rebootDelay;
SomfyShadeController somfy;
MqttConnection mqtt;
SomfyShadeOps shadeOps;
ShadeRequests shadeRequests(shadeOps);
LoopQueue loopQueue;
LoopStats loopStats;

uint32_t oldheap = 0;

void listDir(const char *dirname, uint8_t levels) {
    ESP_LOGI(TAG, "Listing: %s", dirname);
    File root = LittleFS.open(dirname);
    if (!root || !root.isDirectory()) {
        ESP_LOGE(TAG, "Failed to open directory");
        return;
    }
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            ESP_LOGI(TAG, "  DIR : %s", file.name());
            if (levels) listDir(file.path(), levels - 1);
        } else {
            ESP_LOGI(TAG, "  FILE: %-30s  %d bytes", file.name(), file.size());
        }
        file = root.openNextFile();
    }
}

// A failed allocation in library code aborts with only the backtrace, so this
// leaves the size and what the heap had left next to it.  Printed by the ROM
// as the logger might need the heap itself.
static void onAllocFailed(size_t size, uint32_t caps, const char *function) {
  esp_rom_printf("Alloc of %u bytes (caps 0x%" PRIx32 ") failed in %s: free %u, largest block %u, min free %u\n",
    (unsigned)size, caps, function ? function : "?", (unsigned)heap_caps_get_free_size(caps),
    (unsigned)heap_caps_get_largest_free_block(caps), (unsigned)heap_caps_get_minimum_free_size(caps));
}

#include "StatusPanel.h"

void setup() {
  Serial.begin(SERIAL_BAUD);
  BoardProfile::begin();
#if defined(ESPSOMFY_GITHUB_UPDATES)
  if(!gitUpdateMemoryBegin()) ESP_LOGE(TAG, "PSRAM TLS allocator unavailable; GitHub installation disabled");
  ESP_LOGI(TAG, "%s", gitUpdateIdentity());
#endif
  StatusPanel::begin();
  heap_caps_register_failed_alloc_callback(onAllocFailed);
  ESP_LOGI(TAG, "Startup/Boot....");
  esp_core_dump_summary_t summary;
  if (esp_core_dump_get_summary(&summary) == ESP_OK) {
    // The dump stays in flash until the next crash replaces it, so it can still
    // be read later with espcoredump.py.  Only shout about it on the boot right
    // after the crash.
    const esp_reset_reason_t reason = esp_reset_reason();
    const bool newCrash = reason == ESP_RST_PANIC || reason == ESP_RST_TASK_WDT ||
      reason == ESP_RST_INT_WDT || reason == ESP_RST_WDT;
#define CRASH_LOG(fmt, ...) do { if(newCrash) ESP_LOGW(TAG, fmt, ##__VA_ARGS__); else ESP_LOGI(TAG, fmt, ##__VA_ARGS__); } while(0)
    if(newCrash) ESP_LOGW(TAG, "*** Crash coredump from the last boot ***");
    else ESP_LOGI(TAG, "Stored coredump from an earlier crash");
    // exc_task isn't always terminated.
    CRASH_LOG("  Task: %.*s", (int)sizeof(summary.exc_task), summary.exc_task);
    CRASH_LOG("  PC: 0x%08x", summary.exc_pc);
#ifdef CONFIG_IDF_TARGET_ARCH_XTENSA
    CRASH_LOG("  Cause: %d", summary.ex_info.exc_cause);
    char bt_buf[256];
    formatBacktrace(bt_buf, sizeof(bt_buf), summary.exc_bt_info.bt,
      sizeof(summary.exc_bt_info.bt) / sizeof(summary.exc_bt_info.bt[0]), summary.exc_bt_info.depth);
    CRASH_LOG("  Backtrace:%s%s", bt_buf, summary.exc_bt_info.corrupted ? " (corrupted)" : "");
#elif CONFIG_IDF_TARGET_ARCH_RISCV
    CRASH_LOG("  Cause: %d", summary.ex_info.mcause);
    CRASH_LOG("  MTVAL: 0x%08x  RA: 0x%08x  SP: 0x%08x",
             summary.ex_info.mtval, summary.ex_info.ra, summary.ex_info.sp);
#endif
    // The backtrace only decodes against the firmware.elf of the build that
    // crashed, so say whether that is the one running now.
    // app_elf_sha256 isn't always terminated, so copy it before comparing or
    // printing it.
    char crashedSha[APP_ELF_SHA256_SZ];
    static_assert(sizeof(crashedSha) <= sizeof(summary.app_elf_sha256), "copy would read past app_elf_sha256");
    SETCHARPROP(crashedSha, (const char *)summary.app_elf_sha256, sizeof(crashedSha));
    char runningSha[APP_ELF_SHA256_SZ] = {0};
    esp_app_get_elf_sha256(runningSha, sizeof(runningSha));
    if(strcmp(crashedSha, runningSha) == 0)
      CRASH_LOG("  Build: %s (running %s, same build)", crashedSha, runningSha);
    else
      CRASH_LOG("  Build: %s (running %s, different build, decode the backtrace with the matching firmware.elf)", crashedSha, runningSha);
#undef CRASH_LOG
  }
  ESP_LOGI(TAG, "Mounting File System...");
  if (LittleFS.begin()) {
#if defined(ESPSOMFY_GITHUB_UPDATES)
      filesystemHealthy = true;
#endif
      ESP_LOGI(TAG, "Total: %d bytes", LittleFS.totalBytes());
      ESP_LOGI(TAG, "Used:  %d bytes", LittleFS.usedBytes());
      ESP_LOGI(TAG, "Free:  %d bytes", LittleFS.totalBytes() - LittleFS.usedBytes());
      listDir("/", 3);
      ESP_LOGI(TAG, "File system mounted successfully");
  } else {
      ESP_LOGE(TAG, "LittleFS mount failed!");
  }
#if defined(ESPSOMFY_GITHUB_UPDATES)
  applicationHealthy = filesystemHealthy && gitUpdateAssetsMatch();
#endif
  settings.begin();
  if(WiFi.status() == WL_CONNECTED) WiFi.disconnect(true);
  delay(10);
  // Before anything that can submit to it: the web servers and MQTT.
  loopQueue.begin();
  webServer.startup();
  webServer.begin();
  delay(1000);
  net.setup();  
#if defined(ESPSOMFY_GITHUB_UPDATES)
  configHealthy = filesystemHealthy && (!ShadeConfigFile::exists() || ShadeConfigFile::validateFile("/shades.cfg"));
  bootloaderHealthy = gitUpdateBootloaderMatches();
#endif
  somfy.begin();
  // The core starts the watchdog with a 5s timeout before setup(), so
  // esp_task_wdt_init() would fail and keep that.
  const esp_task_wdt_config_t wdt_config = { .timeout_ms = 15000, .idle_core_mask = 1, .trigger_panic = true };
  esp_task_wdt_reconfigure(&wdt_config);
  esp_task_wdt_add(NULL); //add current thread to WDT watch
  esp_ota_img_states_t otaState;
  otaPending = esp_ota_get_state_partition(esp_ota_get_running_partition(), &otaState) == ESP_OK &&
    otaState == ESP_OTA_IMG_PENDING_VERIFY;
  if(otaPending) ESP_LOGW(TAG, "Updated firmware, keeping it once it has run %ds with the network up", OTA_PROVEN_MS / 1000);
  gitUpdate.begin(
#if defined(ESPSOMFY_GITHUB_UPDATES)
    prepareGitInstall
#endif
  );
}

void loop() {
  // put your main code here, to run repeatedly:
  //uint32_t heap = ESP.getFreeHeap();
  // Times the whole pass for the memory event, including any time the loop task was
  // pre-empted, since that delays the loop too.
  ElapsedTimer pass;
  loopQueue.applyRequestedReboot();
  if(rebootDelay.reboot && rebootDeadlineReached(millis(), rebootDelay.rebootTime)) {
    ESP_LOGI(TAG, "Rebooting at %" PRIu32 "ms", rebootDelay.rebootTime);
    loopQueue.commitBeforeReboot();
    net.end();
    ESP.restart();
    return;
  }
  // Each step only logs when it holds up the loop for LOOP_SLOW_MS or more.
  ElapsedTimer timing;
  net.loop();
  logIfSlow(TAG, "Loop net", timing.lap(), LOOP_SLOW_MS);
  esp_task_wdt_reset();
#if defined(ESPSOMFY_GITHUB_UPDATES)
  if(updateQuiesced) {
    gitUpdate.loop();
    // Keep status/cancellation responsive, without executing queued model edits
    // or commands after the state snapshot. Every prepared outcome reboots.
    protoSocket.loop();
    esp_task_wdt_reset();
    return;
  }
#endif
  somfy.loop();
  logIfSlow(TAG, "Loop somfy", timing.lap(), LOOP_SLOW_MS);
  esp_task_wdt_reset();
  loopQueue.drain();
  logIfSlow(TAG, "Loop queue", timing.lap(), LOOP_SLOW_MS);
  esp_task_wdt_reset();
  // What changed in this pass goes out once, to MQTT and the sockets.
  somfy.sendChanges();
  logIfSlow(TAG, "Loop changes", timing.lap(), LOOP_SLOW_MS);
  esp_task_wdt_reset();
  if(net.connected() || net.softAPOpened) {
    webServer.loop();
    logIfSlow(TAG, "Loop web server", timing.lap(), LOOP_SLOW_MS);
    esp_task_wdt_reset();
    protoSocket.loop();
    logIfSlow(TAG, "Loop sockets", timing.lap(), LOOP_SLOW_MS);
    esp_task_wdt_reset();
  }
#if defined(ESPSOMFY_GITHUB_UPDATES)
  // Run normal RX, delayed actions, accepted queue items and change fan-out
  // before considering a persisted update snapshot. Admission is frozen by the
  // preparation callback only after the queue is observed empty.
  gitUpdate.loop();
  if(updateQuiesced) {
    esp_task_wdt_reset();
    return;
  }
#endif
  loopQueue.applyRequestedReboot();
  if(rebootDelay.reboot && rebootDeadlineReached(millis(), rebootDelay.rebootTime)) {
    loopQueue.commitBeforeReboot();
    net.end();
    ESP.restart();
  }
  // Include LCD, battery and local-control work in postboot loop health.
  // The update-quiesced returns above must continue to bypass this call.
  StatusPanel::loop();
  logIfSlow(TAG, "Loop display", timing.lap(), LOOP_SLOW_MS);
  esp_task_wdt_reset();
  const bool bootProven = otaPending &&
#if defined(ESPSOMFY_GITHUB_UPDATES)
    updateHealth.proven(filesystemHealthy && configHealthy && applicationHealthy && bootloaderHealthy && net.connected() &&
      (!somfy.transceiver.config.enabled || somfy.transceiver.available()) &&
      pass.us() < 1000000 && heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) >= 24576,
      millis(), OTA_PROVEN_MS);
#else
    otaProven(net.connected(), millis(), OTA_PROVEN_MS);
#endif
  if(otaPending && bootProven) {
    if(esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      otaPending = false;
      ESP_LOGI(TAG, "Updated firmware confirmed, rollback cancelled");
    }
    else ESP_LOGE(TAG, "Unable to confirm the updated firmware");
  }
#if defined(ESPSOMFY_GITHUB_UPDATES)
  if(otaPending && millis() >= 180000) {
    ESP_LOGE(TAG, "Updated firmware failed its boot health deadline; rolling back");
    esp_ota_mark_app_invalid_rollback_and_reboot();
  }
#endif
  esp_task_wdt_reset();
  loopStats.record(pass.us());
}
