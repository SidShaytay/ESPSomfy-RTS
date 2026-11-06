#!/usr/bin/env python3
"""Run the production SSDP parser on synthetic datagrams under the undefined-behavior sanitizer."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/SSDP.cpp').read_text()
header = (root / 'src/SSDP.h').read_text()
# Keep the actual parser, packet layout and bounds; substitute only UDP I/O.
parser = source[source.index('void SSDPClass::_parsePacket('):source.index('IPAddress SSDPClass::localIP()')]
types = header[header.index('#define SSDP_UUID_SIZE'):header.index('class UPNPDeviceType')]
bound = re.search(r'^#define SSDP_BUFFER_SIZE\s+\d+', source, re.M).group()
program = r'''
#include <cassert>
#include <string>
#include <cmath>
#include "Utils.h"
using std::isnan;
''' + types + '\n' + bound + r'''
struct AsyncUDPPacket {
  std::string bytes;
  size_t offset = 0;
  int available() const { return bytes.size() - offset; }
  char read() { return bytes.at(offset++); }
};
struct SSDPClass {
  bool _startsWith(const char *pre, const char *s) { return strncmp(pre, s, strlen(pre)) == 0; }
  void _parsePacket(ssdp_packet_t *, AsyncUDPPacket &);
};
''' + parser + r'''
int main() {
  SSDPClass server;
  const std::string headers = "\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nST: ssdp:all\r\nMX: 1\r\n\r\n";
  AsyncUDPPacket valid{"M-SEARCH * HTTP/1.1" + headers};
  ssdp_packet_t result;
  server._parsePacket(&result, valid);
  assert(result.valid && result.method == SEARCH && result.mx == 1);
  // Test both sides of the buffer boundary, then much longer datagrams.
  for(size_t length : {size_t(62), size_t(63), size_t(64), size_t(65), size_t(128), size_t(1024)}) {
    AsyncUDPPacket longLine{"M-SEARCH " + std::string(length, 'x') + headers};
    server._parsePacket(&result, longLine);
    assert(!result.valid);
  }
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'parser.cpp'
    exe = Path(tmp) / 'parser-test'
    cpp.write_text(program)
    subprocess.run(shlex.split(os.environ.get('CXX', 'g++')) + [
        '-std=c++17', '-g', '-O1', '-fsanitize=undefined',
        '-fsanitize-undefined-trap-on-error',
        '-fno-omit-frame-pointer', '-I' + str(root / 'src'),
        '-I' + str(root / 'test/stubs'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get('TEST_RUNNER', '')) + [str(exe)], check=True)
print('Production SSDP parser boundary checks passed (UBSan trap mode).')
