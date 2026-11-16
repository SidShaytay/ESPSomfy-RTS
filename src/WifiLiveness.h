#ifndef wifiliveness_h
#define wifiliveness_h
#include <stdint.h>

// Decides when a Wi-Fi link that the driver still reports as associated has
// stopped carrying traffic, so ESPNetwork can force a clean rejoin (#47).
//
// Auto-reconnect is off and ESPNetwork::loop() only rejoins after a STA
// disconnect event.  An access point that keeps beaconing after it restarts or
// forgets us never causes one, so on its own the board would wait forever.
//
// While the link is up, loop() pings the gateway every WIFI_LIVENESS_PROBE_MS.
// Once the gateway has answered at least once, WIFI_LIVENESS_MIN_FAILURES
// failed pings in a row with no answer for the current dead window force a
// reconnect.  The window starts at WIFI_LIVENESS_DEAD_MS and doubles after
// each forced reconnect that doesn't bring the gateway back, up to
// WIFI_LIVENESS_MAX_DEAD_MS.  An answer resets it.
//
// A gateway that never answers pings never forces a reconnect, so a router
// that drops ICMP doesn't make the board rejoin over and over.  The gateway
// is remembered across rejoins, so a reconnect that lands back on a dead link
// keeps retrying.  A new gateway address starts again.
//
// Time is a 64-bit ms clock (esp_timer_get_time() / 1000).  loop() is the only
// caller; the ping task hands its results to loop() first.

#define WIFI_LIVENESS_PROBE_MS 15000ULL
#define WIFI_LIVENESS_DEAD_MS 90000ULL
#define WIFI_LIVENESS_MAX_DEAD_MS 900000ULL
#define WIFI_LIVENESS_MIN_FAILURES 4

class WifiLiveness {
  public:
    enum class Action : uint8_t { none, probe, reconnect };

    // The link came up with this gateway (IPv4, any byte order).  The join
    // itself proved traffic, so the dead window starts now.
    void linkUp(uint32_t gateway, uint64_t now) {
      if(gateway != this->gateway) {
        this->gateway = gateway;
        this->answered = false;
        this->backoff = 0;
      }
      this->up = gateway != 0;
      this->lastAnswer = now;
      this->nextProbe = now + WIFI_LIVENESS_PROBE_MS;
      this->failures = 0;
    }
    void linkDown() { this->up = false; }
    bool isUp() const { return this->up; }

    // The result of the ping that probe asked for.
    void probed(bool ok, uint64_t now) {
      if(!this->up) return;
      if(ok) {
        this->answered = true;
        this->lastAnswer = now;
        this->failures = 0;
        this->backoff = 0;
      }
      else if(this->failures < UINT16_MAX) this->failures++;
    }

    // What loop() should do now.  busy: a ping is still out, so no new one.
    // deferred: something else owns the network (a firmware update), so hold
    // off a reconnect until it ends.
    Action poll(uint64_t now, bool busy, bool deferred) {
      if(!this->up) return Action::none;
      if(this->answered && this->failures >= WIFI_LIVENESS_MIN_FAILURES &&
         now - this->lastAnswer >= this->deadWindow()) {
        if(deferred) return Action::none;
        this->up = false;
        if(this->deadWindow() < WIFI_LIVENESS_MAX_DEAD_MS) this->backoff++;
        return Action::reconnect;
      }
      if(busy || now < this->nextProbe) return Action::none;
      this->nextProbe = now + WIFI_LIVENESS_PROBE_MS;
      return Action::probe;
    }

    // How long the gateway must stay silent before the next forced reconnect.
    uint64_t deadWindow() const {
      uint64_t window = WIFI_LIVENESS_DEAD_MS;
      for(uint8_t i = 0; i < this->backoff && window < WIFI_LIVENESS_MAX_DEAD_MS; i++) window *= 2;
      return window < WIFI_LIVENESS_MAX_DEAD_MS ? window : WIFI_LIVENESS_MAX_DEAD_MS;
    }
    uint16_t failedProbes() const { return this->failures; }
    bool gatewayAnswered() const { return this->answered; }

  private:
    bool up = false;
    bool answered = false;
    uint8_t backoff = 0;
    uint16_t failures = 0;
    uint32_t gateway = 0;
    uint64_t lastAnswer = 0;
    uint64_t nextProbe = 0;
};
#endif
