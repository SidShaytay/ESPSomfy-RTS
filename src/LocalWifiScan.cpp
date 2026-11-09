#include "LocalWifiScan.h"
#include "LocalJsonAllocator.h"
#include <new>
#include <stdio.h>
#include <string.h>
namespace LocalUi {
namespace WifiScan {
using namespace EntityAdmin;
struct Form::Session {
  enum class Page : uint8_t {
    confirm,
    waiting,
    list,
    review,
    failed,
    selected
  } page = Page::confirm;
  WifiScanNetwork rows[16];
  SecuritySnapshot auth;
  uint64_t revision = 0, observed = 0, activity = 0, started = 0, token = 0;
  uint8_t count = 0, cursor = 0, chosen = 0;
  bool overflow = false, hidden = false, manual = false;
};
void Form::close(Notice reason) {
  if (session) {
    // Retire receipt while its callback context still exists, then
    // destruct/wipe.
    if (session->token)
      backend.feed().cancel(session->token);
    session->token = 0;
    session->~Session();
    allocator.deallocate(session);
    session = nullptr;
  }
  notice = reason;
}
bool Form::guard(uint64_t now) {
  if (!session)
    return false;
  auto &s = *session;
  auto auth = backend.authorization();
  Notice failure = Notice::none;
  if (now < s.observed || now - s.activity >= 120000 ||
      (s.page == Session::Page::waiting && now - s.started >= 30000))
    failure = Notice::expired;
  else if (!auth.allowed)
    failure = Notice::locked;
  else if (s.token && backend.feed().token() != s.token)
    failure = Notice::changed;
  else if (!auth.generation || auth.generation != s.auth.generation ||
           auth.configurationGeneration != s.auth.configurationGeneration ||
           backend.networkRevision() != s.revision)
    failure = Notice::changed;
  if (failure != Notice::none) {
    close(failure);
    return false;
  }
  s.observed = now;
  return true;
}
void Form::touch(uint64_t now) {
  session->activity = now;
  backend.activity();
}
bool Form::open(uint64_t now) {
  discard();
  auto auth = backend.authorization();
  auto revision = backend.networkRevision();
  if (!auth.allowed || !auth.generation || !auth.configurationGeneration) {
    close(Notice::locked);
    return false;
  }
  if (!revision) {
    close(Notice::changed);
    return false;
  }
  static_assert(sizeof(Session) + JsonAllocator::allocationOverhead <= 1536,
                "16 bounded copied rows in wiping PSRAM slab");
  void *memory = allocator.allocate(sizeof(Session));
  if (!memory) {
    close(Notice::noMemory);
    return false;
  }
  session = new (memory) Session;
  session->auth = auth;
  session->revision = revision;
  session->observed = session->activity = now;
  return guard(now);
}
void Form::completed(void *context, uint64_t token, bool success,
                     const WifiScanReader &reader) {
  auto &self = *static_cast<Form *>(context);
  if (!self.session || self.session->token != token)
    return;
  self.session->token = 0;
  if (!self.guard(self.backend.now()))
    return;
  auto &s = *self.session;
  if (!success) {
    s.page = Session::Page::failed;
    s.cursor = 0;
    return;
  }
  const auto total = reader.count();
  s.overflow = total > 16;
  s.count = total > 16 ? 16 : total;
  for (uint8_t i = 0; i < s.count; ++i) {
    if (!reader.read(i, s.rows[i]) ||
        !memchr(s.rows[i].ssid, 0, sizeof(s.rows[i].ssid))) {
      self.close(Notice::changed);
      return;
    }
  }
  if (!self.guard(self.backend.now()))
    return;
  s.page = Session::Page::list;
  s.cursor = 0;
}
void Form::turn(int direction, uint64_t now) {
  if (!direction || !guard(now))
    return;
  touch(now);
  auto &s = *session;
  const uint8_t last =
      s.page == Session::Page::list ? s.count
      : (s.page == Session::Page::confirm || s.page == Session::Page::review)
          ? 1
          : 0;
  if (direction > 0 && s.cursor < last)
    ++s.cursor;
  if (direction < 0 && s.cursor)
    --s.cursor;
}
void Form::activate(uint64_t now) {
  if (!guard(now))
    return;
  touch(now);
  auto &s = *session;
  switch (s.page) {
  case Session::Page::confirm:
    if (!s.cursor) {
      discard();
      return;
    }
    s.token = backend.feed().request(completed, this);
    if (!s.token) {
      s.page = Session::Page::failed;
      s.cursor = 0;
      return;
    }
    s.started = now;
    s.page = Session::Page::waiting;
    s.cursor = 0;
    break;
  case Session::Page::waiting:
    discard();
    break;
  case Session::Page::list:
    s.manual = s.cursor == s.count;
    if (s.manual) {
      s.page = Session::Page::selected;
      break;
    }
    s.chosen = s.cursor;
    s.hidden = !s.rows[s.chosen].ssid[0];
    s.page = Session::Page::review;
    s.cursor = 0;
    break;
  case Session::Page::review:
    if (!s.cursor) {
      s.page = Session::Page::list;
      s.cursor = s.chosen;
    } else {
      s.manual = s.hidden;
      s.page = Session::Page::selected;
    }
    break;
  case Session::Page::failed:
    discard();
    break;
  case Session::Page::selected:
    break;
  }
}
void Form::back(uint64_t now) {
  if (!guard(now))
    return;
  touch(now);
  auto &s = *session;
  if (s.page == Session::Page::review) {
    s.page = Session::Page::list;
    s.cursor = s.chosen;
  } else
    discard();
}
bool Form::takeSSID(char (&out)[33], uint64_t now, SecuritySnapshot &auth,
                    uint64_t &revision) {
  memset(out, 0, sizeof(out));
  auth = {};
  revision = 0;
  if (!guard(now) || session->page != Session::Page::selected)
    return false;
  auto &s = *session;
  auth = s.auth;
  revision = s.revision;
  if (!s.manual)
    memcpy(out, s.rows[s.chosen].ssid, sizeof(out));
  s.page = Session::Page::list;
  s.cursor = s.chosen;
  return true;
}
void Form::render(FormView &out) const {
  out = {};
  out.notice = notice;
  if (!session)
    return;
  auto &s = *session;
  out.mode = Mode::validation;
  snprintf(out.title, sizeof(out.title), "WiFi scan");
  const char *hint = "Press: choose   Back: System";
  if (s.page == Session::Page::confirm) {
    out.total = out.rowCount = 2;
    strcpy(out.rows[0].label, "Cancel");
    strcpy(out.rows[1].label, "Start WiFi scan");
    hint = "May interrupt WiFi. Results only; no connection or Save.";
  } else if (s.page == Session::Page::waiting) {
    out.total = out.rowCount = 1;
    strcpy(out.rows[0].label, "Cancel local request");
    hint = "Waiting up to 30s. An accepted shared scan may finish.";
  } else if (s.page == Session::Page::failed) {
    out.total = out.rowCount = 1;
    strcpy(out.rows[0].label, "Back to System");
    hint = "Scan unavailable or failed. No automatic retry.";
  } else if (s.page == Session::Page::list ||
             s.page == Session::Page::selected) {
    out.mode = Mode::stackedRows;
    out.total = s.count + 1;
    out.offset = s.cursor / 3 * 3;
    for (uint8_t i = out.offset; i < out.total && out.rowCount < 3; ++i) {
      auto &row = out.rows[out.rowCount++];
      if (i == s.count)
        strcpy(row.label, "Enter SSID manually");
      else {
        auto &r = s.rows[i];
        snprintf(row.label, sizeof(row.label),
                 "#%u %02X:%02X:%02X:%02X:%02X:%02X", unsigned(i) + 1,
                 r.bssid[0], r.bssid[1], r.bssid[2], r.bssid[3], r.bssid[4],
                 r.bssid[5]);
        snprintf(row.value, sizeof(row.value), "%s",
                 r.ssid[0] ? r.ssid : "<hidden; manual>");
      }
    }
    hint = s.overflow ? "First 16 results. Others/hidden SSIDs: enter manually."
                      : "Choose a result to review, or enter SSID manually.";
  } else {
    out.mode = Mode::wifiReview;
    snprintf(out.title, sizeof(out.title), "Review WiFi #%u",
             unsigned(s.chosen) + 1);
    out.total = out.rowCount = 2;
    strcpy(out.rows[0].label, "Cancel");
    strcpy(out.rows[1].label,
           s.hidden ? "Enter SSID manually" : "Use SSID in settings draft");
    auto &r = s.rows[s.chosen];
    // Pinned ESP-IDF wifi_auth_mode_t: Open=0, named secured modes=1..16.
    // Future/invalid numeric modes are not guessed to be a supported mode.
    const char *security = !r.authentication        ? "Open"
                           : r.authentication <= 16 ? "Secured"
                                                    : "Unknown";
    snprintf(out.rows[0].value, sizeof(out.rows[0].value), "Ch%u %ddBm %s",
             unsigned(r.channel), int(r.rssi), security);
    snprintf(out.rows[2].label, sizeof(out.rows[2].label),
             "%02X:%02X:%02X:%02X:%02X:%02X", r.bssid[0], r.bssid[1],
             r.bssid[2], r.bssid[3], r.bssid[4], r.bssid[5]);
    snprintf(out.rows[2].value, sizeof(out.rows[2].value), "%s",
             r.ssid[0] ? r.ssid : "<hidden>");
    hint = s.hidden ? "Hidden SSID: enter its name manually in settings."
                    : "Draft only. Review password and Save separately.";
  }
  snprintf(out.hint, sizeof(out.hint), "%s", hint);
  out.selected = s.cursor;
  for (uint8_t i = 0; i < out.rowCount; ++i)
    out.rows[i].selected = out.offset + i == s.cursor;
}
} // namespace WifiScan
} // namespace LocalUi
