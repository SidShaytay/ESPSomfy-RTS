#include "ModelEditor.h"
#include "SettingsLoadState.h"
#include "ShadeGpioPolicy.h"
#include <ArduinoJson.h>
#include <set>
#include <unity.h>
#define SHADE_GPIO_CONFIG_HOST_TEST
#define SHADE_GPIO_ADMISSION_HOST_TEST
#define HAS_ETHERNET 1
#define GPIO_IS_VALID_OUTPUT_GPIO(pin)                                         \
  ((pin) < 49 && (pin) != 46 && ((pin) < 22 || (pin) > 25))
static std::set<uint8_t> radioPads, ethernetPads;
struct SomfyShade {
  uint8_t id = 255, gpioUp = 17, gpioDown = 40, gpioMy = 42;
  radio_proto proto = radio_proto::GP_Remote;
  uint8_t getShadeId() { return id; }
  bool usesPin(uint8_t pin) {
    const auto mask = ShadeGpioPolicy::physicalMask(proto);
    return pin != 255 && (((mask & ShadeGpioPolicy::up) && pin == gpioUp) ||
                          ((mask & ShadeGpioPolicy::down) && pin == gpioDown) ||
                          ((mask & ShadeGpioPolicy::my) && pin == gpioMy));
  }
  int8_t validateGPIOs(radio_proto, uint8_t, uint8_t, uint8_t);
  int8_t validateJSON(JsonObject &);
};
struct SomfyShadeController {
  SomfyShade shades[2];
  struct Radio {
    struct {SettingsLoadStatus loadStatus{SettingsLoadState::loaded};}config;
    bool usesPin(uint8_t pin) { return radioPads.count(pin); }
  } transceiver;
} somfy;
enum class conn_types_t { wifi, ethernet, ethernetpref };
struct ConfigSettings {
  SettingsLoadStatus loadStatus{SettingsLoadState::loaded};
  conn_types_t connType = conn_types_t::wifi;
  struct Ethernet {
    SettingsLoadStatus loadStatus{SettingsLoadState::loaded};
    bool usesPin(uint8_t pin) { return ethernetPads.count(pin); }
  } Ethernet;
} settings;
#include "../../src/ShadeGpioAdmission.cpp"
#include "../../src/ShadeGpioConfig.cpp"
void setUp() {
  somfy.transceiver.config.loadStatus={SettingsLoadState::loaded};
  settings.loadStatus={SettingsLoadState::loaded};
  settings.Ethernet.loadStatus={SettingsLoadState::loaded};
  radioPads.clear();
  ethernetPads.clear();
  somfy.shades[0] = {};
  somfy.shades[0].id = 4;
  somfy.shades[1] = {};
  settings.connType = conn_types_t::wifi;
}
void tearDown() {}
static int8_t validate(JsonDocument &request) {
  auto obj = request.as<JsonObject>();
  return somfy.shades[0].validateJSON(obj);
}
static void
projected_admission_checks_every_physical_pad_independently_of_category() {
  for (int category = 0; category <= 16; category++)
    for (auto protocol : {radio_proto::GP_Remote, radio_proto::GP_Relay}) {
      for (unsigned slot = 0;
           slot < (protocol == radio_proto::GP_Remote ? 3u : 2u); slot++) {
        JsonDocument request;
        request["proto"] = int(protocol);
        request["shadeType"] = category;
        const char *keys[] = {"gpioUp", "gpioDown", "gpioMy"};
        request[keys[slot]] = 15;
        radioPads = {15};
        TEST_ASSERT_EQUAL(PIN_CONFLICT_RADIO, validate(request));
        radioPads.clear();
        settings.connType = conn_types_t::ethernet;
        ethernetPads = {15};
        TEST_ASSERT_EQUAL(PIN_CONFLICT_ETHERNET, validate(request));
        ethernetPads.clear();
        settings.connType = conn_types_t::wifi;
      }
    }
}
static void
output_invalid_duplicates_and_unset_values_are_checked_before_conversion() {
  const char *keys[] = {"gpioUp", "gpioDown", "gpioMy"};
  for (const auto *key : keys)
    for (auto number : {-1, 22, 46, 49, 256}) {
      JsonDocument request;
      request[key] = number;
      TEST_ASSERT_EQUAL(PIN_INVALID_OUTPUT, validate(request));
      TEST_ASSERT_EQUAL(17, somfy.shades[0].gpioUp);
      TEST_ASSERT_EQUAL(40, somfy.shades[0].gpioDown);
      TEST_ASSERT_EQUAL(42, somfy.shades[0].gpioMy);
    }
  JsonDocument request;
  request["gpioUp"] = 40;
  TEST_ASSERT_EQUAL(PIN_CONFLICT_DUPLICATE, validate(request));
  request.clear();
  request["gpioUp"] = request["gpioDown"] = request["gpioMy"] = 255;
  TEST_ASSERT_EQUAL(0, validate(request));
  request["proto"] = 256;
  TEST_ASSERT_EQUAL(PIN_INVALID_OUTPUT, validate(request));
}
static void another_shades_physically_touched_pad_cannot_be_borrowed() {
  auto &other = somfy.shades[1];
  other.id = 7;
  other.gpioUp = 15;
  other.gpioDown = 16;
  other.gpioMy = 17;
  JsonDocument request;
  request.to<JsonObject>();
  TEST_ASSERT_EQUAL(PIN_CONFLICT_SHADE, validate(request));
  other.id = 4;
  TEST_ASSERT_EQUAL(PIN_CONFLICT_SHADE, validate(request));
  other.gpioMy = 255;
  TEST_ASSERT_EQUAL(0, validate(request));
}
static void
unsafe_legacy_assignments_remain_unchanged_and_can_be_explicitly_repaired() {
  radioPads = {15};
  somfy.shades[0].gpioUp = 15;
  JsonDocument request;
  request.to<JsonObject>();
  TEST_ASSERT_EQUAL(PIN_CONFLICT_RADIO, validate(request));
  TEST_ASSERT_EQUAL(15, somfy.shades[0].gpioUp);
  request["gpioUp"] = 17;
  TEST_ASSERT_EQUAL(0, validate(request));
  TEST_ASSERT_EQUAL(15, somfy.shades[0].gpioUp);
  request.clear();
  request["proto"] = int(radio_proto::RTS);
  TEST_ASSERT_EQUAL(0, validate(request));
}
static void unavailable_pin_ownership_refuses_only_physical_gpio_without_mutation() {
 for(unsigned source=0;source<3;++source){
  somfy.transceiver.config.loadStatus={source?SettingsLoadState::loaded:SettingsLoadState::failed};
  settings.Ethernet.loadStatus={source==1?SettingsLoadState::failed:SettingsLoadState::loaded};
  settings.loadStatus={source==2?SettingsLoadState::failed:SettingsLoadState::loaded};
  const auto before=somfy.shades[0];JsonDocument request;request["gpioUp"]=19;
  TEST_ASSERT_EQUAL(PIN_STORAGE_UNAVAILABLE,validate(request));
  TEST_ASSERT_EQUAL(before.gpioUp,somfy.shades[0].gpioUp);
  TEST_ASSERT_EQUAL(before.gpioDown,somfy.shades[0].gpioDown);
  TEST_ASSERT_EQUAL(0,somfy.shades[0].validateGPIOs(radio_proto::RTS,19,40,42));
 }
}
int main() {
  UNITY_BEGIN();RUN_TEST(unavailable_pin_ownership_refuses_only_physical_gpio_without_mutation);
  RUN_TEST(
      projected_admission_checks_every_physical_pad_independently_of_category);
  RUN_TEST(
      output_invalid_duplicates_and_unset_values_are_checked_before_conversion);
  RUN_TEST(another_shades_physically_touched_pad_cannot_be_borrowed);
  RUN_TEST(
      unsafe_legacy_assignments_remain_unchanged_and_can_be_explicitly_repaired);
  return UNITY_END();
}
