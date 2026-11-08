#pragma once
// Stream-check the exact packaged web files once during Plus startup. The
// compiled compatibility digest authenticates the complete asset manifest;
// configuration and counters outside that manifest are never read or changed.
bool gitUpdateAssetsMatch();
