#!/usr/bin/env python3
"""Compile real file validation and record codecs against an in-memory file."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/ConfigFile.cpp').read_text()
read_header = source[source.index('bool ConfigFile::readHeader()'):source.index('// Between the shade model')]
validate = source[source.index('bool ShadeConfigFile::validate()'):source.index('bool ShadeConfigFile::load(')]
program = r'''
#include <cassert>
#include <string>
#include "ConfigRecords.h"
#include "esp_log.h"
constexpr int SeekSet = 0;
static const char *TAG = "ConfigFile";
struct MemoryFile {
  std::string bytes;
  size_t pos = 0;
  uint32_t position() const { return pos; }
  uint32_t size() const { return bytes.size(); }
  bool seek(uint32_t offset, int) { pos = offset; return pos <= bytes.size(); }
};
struct ConfigFile : ConfigReader {
  config_header_t header;
  MemoryFile file;
  bool isOpen() const { return true; }
  bool readByte(uint8_t &v) override {
    if(file.pos >= file.bytes.size()) return false;
    v = file.bytes[file.pos++];
    return true;
  }
  uint32_t position() override { return file.position(); }
  bool readHeader();
};
struct ShadeConfigFile : ConfigFile { bool validate(); };
''' + read_header + validate + r'''
struct Writer : ConfigWriter {
  std::string bytes;
  bool writeBytes(const uint8_t *p, size_t n) override { bytes.append((const char *)p, n); return true; }
};
static bool accepts(uint8_t rooms, uint8_t shades, uint8_t groups, uint8_t bits = 56) {
  config_header_t h;
  h.version = SHADE_HDR_VER; h.length = SHADE_HDR_SIZE;
  h.roomRecordSize = ROOM_REC_SIZE; h.roomRecords = rooms;
  h.shadeRecordSize = SHADE_REC_SIZE; h.shadeRecords = shades;
  h.groupRecordSize = GROUP_REC_SIZE; h.groupRecords = groups;
  Writer w;
  assert(writeConfigHeader(w, h));
  for(unsigned i=0;i<rooms;i++) assert(writeRoomRecord(w, room_record_t{}));
  shade_record_t shade; shade.shadeId = 1; shade.bitLength = bits;
  group_record_t group; group.groupId = 1; group.bitLength = bits;
  for(unsigned i=0;i<shades;i++) assert(writeShadeRecord(w, shade));
  for(unsigned i=0;i<groups;i++) assert(writeGroupRecord(w, group));
  ShadeConfigFile f; f.file.bytes = w.bytes;
  return f.validate();
}
int main() {
  assert(accepts(0, 0, 0));
  assert(accepts(SOMFY_MAX_ROOMS, SOMFY_MAX_SHADES, SOMFY_MAX_GROUPS));
  assert(!accepts(SOMFY_MAX_ROOMS + 1, 0, 0));
  assert(!accepts(0, SOMFY_MAX_SHADES + 1, 0));
  assert(!accepts(0, 0, SOMFY_MAX_GROUPS + 1));
  assert(accepts(0, 1, 1, 80));
  for(uint8_t bits : {0, 55, 57, 79, 81, 255}) {
    assert(!accepts(0, 1, 0, bits));
    assert(!accepts(0, 0, 1, bits));
  }
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'validation.cpp'
    exe = Path(tmp) / 'validation-test'
    cpp.write_text(program)
    subprocess.run(shlex.split(os.environ.get('CXX', 'g++')) + [
        '-std=c++17', '-g', '-O1', '-fsanitize=undefined',
        '-fsanitize-undefined-trap-on-error', '-I' + str(root / 'src'),
        '-I' + str(root / 'test/stubs'), str(cpp),
        str(root / 'src/ConfigRecords.cpp'), '-o', str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get('TEST_RUNNER', '')) + [str(exe)], check=True)
print('Production config validation checks passed (UBSan trap mode).')
