#pragma once

// Compare installed raw bootloader bytes with this build's generated pinned
// rollback-enabled bootloader. Cached: the updater never writes the bootloader.
bool gitUpdateBootloaderMatches();
