#pragma once
#include <stdint.h>
#include <stddef.h>
#include "LoginThrottle.h"

namespace LocalUi {
// Same configured modes and byte limits as SecuritySettings. No web token or
// per-IP throttle is shared with this loop-owned, volatile local session.
enum class AuthMode : uint8_t { none, pin, password, unavailable };
enum class AuthResult : uint8_t { unlocked, wrong, locked, unavailable };
inline void wipe(void *memory, size_t bytes) {
  auto *out = static_cast<volatile uint8_t *>(memory);
  while(bytes--) *out++ = 0;
}
struct Credentials {
  char username[33] = {}, secret[33] = {};
  void clear() { wipe(this, sizeof(*this)); }
  ~Credentials() { clear(); }
};
class Authorization {
  AuthMode mode = AuthMode::unavailable;
  Credentials expected;
  LoginThrottle attempts;
  bool active = false, timedOut = false;
  uint64_t lastActivity = 0, expiredAt = 0, observedAt = 0;
  bool observedClock = false;
  uint32_t revision = 0, configurationRevision = 0;
  static bool copy(char *out, const char *value, size_t maximum) {
    if(!value) return false;
    size_t n = 0;
    while(n <= maximum && value[n]) ++n;
    if(n > maximum) return false;
    for(size_t i = 0; i < n; ++i) out[i] = value[i];
    return true;
  }
  static bool same(const Credentials &a, const Credentials &b) {
    uint8_t diff = 0;
    for(size_t i = 0; i < sizeof(a.username); ++i) {
      diff |= uint8_t(a.username[i] ^ b.username[i]);
      diff |= uint8_t(a.secret[i] ^ b.secret[i]);
    }
    return diff == 0;
  }
  bool observeClock(uint64_t now) {
    if(observedClock && now < observedAt) {
      if(active || timedOut) lock();
      return false;
    }
    observedAt = now;
    observedClock = true;
    return true;
  }
public:
  static constexpr uint64_t inactivityMs = 120000;
  static constexpr uint64_t stopGraceMs = 30000;
  Authorization() = default;
  Authorization(const Authorization &) = delete;
  Authorization &operator=(const Authorization &) = delete;
  ~Authorization() { expected.clear(); }
  void configure(AuthMode nextMode, const char *username, const char *secret) {
    Credentials next;
    bool valid = false;
    switch(nextMode) {
    case AuthMode::none: valid = true; break;
    case AuthMode::pin: valid = copy(next.secret, secret, 4); break;
    case AuthMode::password:
      valid = copy(next.username, username, 32) && copy(next.secret, secret, 32);
      break;
    default: break;
    }
    if(!valid) { next.clear(); nextMode = AuthMode::unavailable; }
    if(mode != nextMode || !same(expected, next)) {
      expected.clear();
      expected = next;
      mode = nextMode;
      configurationChanged();
    }
    next.clear();
  }
  void configurationChanged() { ++configurationRevision; lock(); }
  AuthMode configuredMode() const { return mode; }
  uint32_t generation() const { return revision; }
  uint32_t configurationGeneration() const { return configurationRevision; }
  bool stopGraceAllowed() const { return timedOut; }
  void lock() { active = timedOut = false; lastActivity = expiredAt = 0; ++revision; }
  bool allowed(uint64_t now) {
    if(!observeClock(now)) return false;
    if(mode == AuthMode::none) return true;
    if(active && now < lastActivity) {
      lock();
    } else if(active && now - lastActivity >= inactivityMs) {
      const uint64_t deadline = lastActivity + inactivityMs;
      lock();
      expiredAt = deadline;
      timedOut = true;
    }
    // A delayed loop must not start a fresh grace period when it first notices
    // expiry. Activity and wrong credentials cannot extend the original deadline.
    if(timedOut && (now < expiredAt || now - expiredAt >= stopGraceMs)) timedOut = false;
    return active;
  }
  void activity(uint64_t now) {
    if(allowed(now) && mode != AuthMode::none) lastActivity = now;
  }
  uint32_t retryAfter(uint64_t now) {
    if(!observeClock(now)) {
      const uint64_t catchup = observedAt - now;
      if(catchup > UINT32_MAX) return UINT32_MAX;
      const uint64_t wait = catchup + attempts.retryAfter(0, observedAt);
      return wait > UINT32_MAX ? UINT32_MAX : uint32_t(wait);
    }
    return attempts.retryAfter(0, now);
  }
  AuthResult unlock(const Credentials &given, uint64_t now) {
    if(mode == AuthMode::unavailable) return AuthResult::unavailable;
    allowed(now);
    if(now < observedAt) return AuthResult::locked;
    if(retryAfter(now)) return AuthResult::locked;
    const bool wasExpired = timedOut;
    const uint64_t wasExpiredAt = expiredAt;
    lock();
    if(mode == AuthMode::none || same(expected, given)) {
      attempts.succeeded(0);
      active = true;
      lastActivity = now;
      return AuthResult::unlocked;
    }
    timedOut = wasExpired;
    expiredAt = wasExpiredAt;
    attempts.failed(0, now);
    return AuthResult::wrong;
  }
};
} // namespace LocalUi
