#ifndef GIT_UPDATE_IDENTITY_H
#define GIT_UPDATE_IDENTITY_H

// Return the complete compiled identity. Referencing this function keeps the
// marker inside the ESP image, covered by its image checksum and SHA256.
const char *gitUpdateIdentity();

#endif
