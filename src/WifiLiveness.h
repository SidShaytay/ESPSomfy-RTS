#ifndef wifiliveness_h
#define wifiliveness_h
#include <stdint.h>

// Decides when a Wi-Fi link that the driver still reports as associated has
// stopped carrying traffic, so ESPNetwork can force a clean rejoin (#47).
//
// Auto-reconnect is off and ESPNetwork::loop() only rejoins after a STA
// disconnect event.  An access point that restarts on another channel, or
// forgets us, can leave the station associated with no disconnect event, so
// on its own the board would wait forever.
//
// It is passive.  The Wi-Fi interface stamps the time of every frame it hands
// to the IP stack.  Only frames that decrypt with the current keys get there,
// and a home LAN delivers some every few seconds (ARP, mDNS, Home Assistant),
// so a healthy link sends nothing extra.  Only after WIFI_LIVENESS_QUIET_MS
// with nothing received does loop() ask the gateway for its address (ARP),
// up to WIFI_LIVENESS_PROBES times WIFI_LIVENESS_PROBE_MS apart.  Every IPv4
// host must answer ARP, and any frame that arrives ends the check.  If the
// link is still silent one interval after the last request, it reconnects.
//
// A rejoin that comes back silent doubles the quiet window, up to
// WIFI_LIVENESS_MAX_QUIET_MS, so a dead network isn't churned every two
// minutes.  Traffic that arrives a full base window after a join proves the
// link and resets it.
//
// Time is a 64-bit ms clock (esp_timer_get_time() / 1000).  loop() is the only
// caller; the interface hook hands its stamp to loop() first.

#define WIFI_LIVENESS_QUIET_MS 120000ULL
#define WIFI_LIVENESS_PROBE_MS 5000ULL
#define WIFI_LIVENESS_PROBES 3
#define WIFI_LIVENESS_MAX_QUIET_MS 900000ULL

class WifiLiveness {
  public:
    enum class Action : uint8_t { none, probe, reconnect };

    // A join finished.  It took traffic to get here, so the link counts as
    // heard from now.
    void linkUp(uint64_t now) {
      this->up = true;
      this->upSince = this->lastHeard = now;
      this->probes = 0;
    }
    bool isUp() const { return this->up; }

    // The latest time a frame arrived.  Older stamps are ignored.
    void heard(uint64_t at) {
      if(!this->up || at <= this->lastHeard) return;
      this->lastHeard = at;
      this->probes = 0;
      if(at - this->upSince >= WIFI_LIVENESS_QUIET_MS) this->backoff = 0;
    }

    // What loop() should do now.  deferred: something else owns the network
    // (a firmware update), so hold off a reconnect until it ends.
    Action poll(uint64_t now, bool deferred) {
      if(!this->up || now - this->lastHeard < this->quietWindow()) return Action::none;
      // Each request gets a full interval to be answered, even after a stall.
      if(this->probes > 0 && now - this->lastProbe < WIFI_LIVENESS_PROBE_MS) return Action::none;
      if(this->probes < WIFI_LIVENESS_PROBES) {
        this->probes++;
        this->lastProbe = now;
        return Action::probe;
      }
      if(deferred) return Action::none;
      this->up = false;
      if(this->quietWindow() < WIFI_LIVENESS_MAX_QUIET_MS) this->backoff++;
      return Action::reconnect;
    }

    // How long the link must stay silent before the gateway is asked.
    uint64_t quietWindow() const {
      uint64_t window = WIFI_LIVENESS_QUIET_MS;
      for(uint8_t i = 0; i < this->backoff && window < WIFI_LIVENESS_MAX_QUIET_MS; i++) window *= 2;
      return window < WIFI_LIVENESS_MAX_QUIET_MS ? window : WIFI_LIVENESS_MAX_QUIET_MS;
    }
    uint64_t silentFor(uint64_t now) const { return now - this->lastHeard; }

  private:
    bool up = false;
    uint8_t probes = 0;
    uint8_t backoff = 0;
    uint64_t upSince = 0;
    uint64_t lastHeard = 0;
    uint64_t lastProbe = 0;
};
#endif
