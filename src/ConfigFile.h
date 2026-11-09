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


// Opens shades.cfg or a backup and writes or reads its values
// (ConfigRecords.h), from the file or into a String in RAM.
class ConfigFile : public ConfigReader, public ConfigWriter {
  protected:
    File file;
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
    static bool load(SomfyShadeController *somfy, const char *filename = "/shades.cfg");
    static bool restore(SomfyShadeController *somfy, const char *filename, restore_options_t &opts, const GitUpdateOwnerToken &owner = {});
    // Whether the file would pass the checks restore() starts with.  Only
    // reads the file, so any task can call it.
    static bool validateFile(const char *filename);
    bool begin(const char *filename, bool readOnly = false);
    bool begin(bool readOnly = false);
    bool save(SomfyShadeController *somfy);
    bool backup(SomfyShadeController *somfy);
    bool loadFile(SomfyShadeController *somfy, const char *filename = "/shades.cfg");
    bool restoreFile(SomfyShadeController *somfy, const char *filename, restore_options_t &opts, const GitUpdateOwnerToken &owner = {});
    void end();
    //bool seekRecordById(uint8_t id);
    bool validate();
};
#endif
