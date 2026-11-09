#ifndef configfile_h
#define configfile_h
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "Somfy.h"
#include "ConfigSettings.h"
#include "ConfigHeader.h"
#include "ConfigRecords.h"
#include "ModelCommitResult.h"
#include "GitUpdate.h"
#include "ModelLoadSnapshot.h"
#include "CounterLoad.h"
#include "RestoreResult.h"


class SelectedSettingsRestore;

// Opens shades.cfg or a backup and writes or reads its values
// (ConfigRecords.h), from the file or into a String in RAM.
class ConfigFile : public ConfigReader, public ConfigWriter {
  protected:
    File file;
    ModelLoadSnapshot snapshot;
    ModelLoadStatus snapshotStatus=ModelLoadStatus::unknown;
    size_t readPosition=0;
    bool readFailed=false;
    bool seekRead(uint32_t offset);
    size_t readSize() const { return snapshot.size(); }
    String *_ramBuf = nullptr;
    bool readOnly = false;
    bool begin(const char *filename, bool readOnly = false);
    bool _opened = false;
    bool _writeFailed = false;
  public:
    config_header_t header;
    bool beginRAM(String *buf);
    void end();
    bool isOpen();
    bool readHeader();
    bool writeHeader();
    bool readByte(uint8_t &val) override;
    uint32_t position() override;
    bool writeBytes(const uint8_t *buf, size_t len) override;
};
class ShadeConfigFile : public ConfigFile {
  protected:
    bool writeModel(SomfyShadeController *s,ConfigWriter &writer);
    CounterLoad counters;
    bool prepareCounters(CounterLoadControl control={});
    bool validateRestoreRecords(const restore_options_t &);
    bool validateRestoreGPIOs(SomfyShadeController *,bool imported,const SelectedSettingsRestore &);
    // Loading or restoring keeps the higher file/NVS rolling code for each address.
    void readModel(SomfyShadeController *s);
    void readRoom(SomfyRoom *room);
    void readShade(SomfyShade *shade);
    void readGroup(SomfyGroup *group);
    void readRepeaters(SomfyShadeController *s);
  public:
    // Loop-owned checked persistence; does not apply edits or rolling-code NVS.
    static ModelCommitResult commitModel(SomfyShadeController *s);
    static bool exists();
    static ModelLoadStatus loadBoot(SomfyShadeController *somfy);
    static bool load(SomfyShadeController *somfy, const char *filename = "/shades.cfg");
    static RestoreApplyResult restore(SomfyShadeController *somfy, const char *filename, restore_options_t &opts, const GitUpdateOwnerToken &owner, const RadioInstallToken &radio);
    // Whether the file would pass the checks restore() starts with.  Only
    // reads the file, so any task can call it.
    static bool validateFile(const char *filename);
    bool begin(const char *filename, bool readOnly = false);
    bool begin(bool readOnly = false);
    bool save(SomfyShadeController *somfy);
    bool backup(SomfyShadeController *somfy);
    bool loadFile(SomfyShadeController *somfy, const char *filename = "/shades.cfg");
    RestoreApplyResult restoreFile(SomfyShadeController *somfy, const char *filename, restore_options_t &opts, const GitUpdateOwnerToken &owner, const RadioInstallToken &radio);
    void end();
    //bool seekRecordById(uint8_t id);
    bool validate();
};
#endif
