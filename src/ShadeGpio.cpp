#include "esp_log.h"
#include "Somfy.h"
#include "ShadeGpioPolicy.h"

static const char *TAG = "Somfy";

// Legacy/foreign assignments stay stored for explicit repair, but never reach
// physical GPIO setup or writes when reserved, invalid or aliased.
static bool unsafeGPIOs(SomfyShade &shade) {
  if(!ShadeGpioPolicy::physicalMask(shade.proto))return false;
  return shade.validateGPIOs(shade.proto,shade.gpioUp,shade.gpioDown,shade.gpioMy)!=0;
}
static void writeAssignedGPIO(uint8_t pin,uint8_t value) {
  if(pin!=255)digitalWrite(pin,value);
}
void SomfyShadeController::configureGPIOs() {
  for(auto &shade:this->shades)if(shade.getShadeId()!=255)shade.configureGPIOs();
}
void SomfyShade::configureGPIOs() {
  if(unsafeGPIOs(*this))return;
  const uint8_t mask=ShadeGpioPolicy::physicalMask(this->proto);
  const uint8_t pins[]={this->gpioUp,this->gpioDown,this->gpioMy};
  for(uint8_t i=0;i<3;i++)if((mask & (1u<<i)) && pins[i]!=255)pinMode(pins[i],OUTPUT);
}

void SomfyShade::setGPIOs() {
  if(unsafeGPIOs(*this)) return;
  if(this->proto == radio_proto::GP_Relay) {
    // Determine whether the direction needs to be set.
    uint8_t p_on = (this->gpioFlags & (uint8_t)gpio_flags_t::LowLevelTrigger) == 0x00 ? HIGH : LOW;
    uint8_t p_off = (this->gpioFlags & (uint8_t)gpio_flags_t::LowLevelTrigger) == 0x00 ? LOW : HIGH;
    
    int8_t dir = this->state().direction;
    if(dir == 0 && this->tiltType == tilt_types::integrated)
      dir = this->state().tiltDirection;
    else if(this->tiltType == tilt_types::tiltonly)
      dir = this->state().tiltDirection;
    if(this->shadeType == shade_types::drycontact) {
      writeAssignedGPIO(this->gpioDown, this->state().currentPos == 100 ? p_on : p_off);
      this->gpioDir = this->state().currentPos == 100 ? 1 : -1;
    }
    else if(this->shadeType == shade_types::drycontact2) {
      if(this->state().currentPos == 100) {
        writeAssignedGPIO(this->gpioDown, p_off);
        writeAssignedGPIO(this->gpioUp, p_on);
      }
      else {
        writeAssignedGPIO(this->gpioUp, p_off);
        writeAssignedGPIO(this->gpioDown, p_on);
      }
      this->gpioDir = this->state().currentPos == 100 ? 1 : -1;
    }
    else {
      switch(dir) {
        case -1:
          writeAssignedGPIO(this->gpioDown, p_off);
          writeAssignedGPIO(this->gpioUp, p_on);
          if(dir != this->gpioDir) ESP_LOGD(TAG, "UP: true, DOWN: false");
          this->gpioDir = dir;
          break;
        case 1:
          writeAssignedGPIO(this->gpioUp, p_off);
          writeAssignedGPIO(this->gpioDown, p_on);
          if(dir != this->gpioDir) ESP_LOGD(TAG, "UP: false, DOWN: true");
          this->gpioDir = dir;
          break;
        default:
          writeAssignedGPIO(this->gpioUp, p_off);
          writeAssignedGPIO(this->gpioDown, p_off);
          if(dir != this->gpioDir) ESP_LOGD(TAG, "UP: false, DOWN: false");
          this->gpioDir = dir;
          break;
      }
    }
  }
  else if(this->proto == radio_proto::GP_Remote) {
    if(millis() > this->gpioRelease) {
      //uint8_t p_on = (this->gpioFlags & (uint8_t)gpio_flags_t::LowLevelTrigger) == 0x00 ? HIGH : LOW;
      uint8_t p_off = (this->gpioFlags & (uint8_t)gpio_flags_t::LowLevelTrigger) == 0x00 ? LOW : HIGH;
      writeAssignedGPIO(this->gpioUp, p_off);
      writeAssignedGPIO(this->gpioDown, p_off);
      writeAssignedGPIO(this->gpioMy, p_off);
      this->gpioRelease = 0;
    }
  }
}
void SomfyRemote::triggerGPIOs(somfy_frame_t &) { }
void SomfyShade::triggerGPIOs(somfy_frame_t &frame) {
  if(unsafeGPIOs(*this)) return;
  if(this->proto == radio_proto::GP_Remote) {
    uint8_t p_on = (this->gpioFlags & (uint8_t)gpio_flags_t::LowLevelTrigger) == 0x00 ? HIGH : LOW;
    uint8_t p_off = (this->gpioFlags & (uint8_t)gpio_flags_t::LowLevelTrigger) == 0x00 ? LOW : HIGH;
    int8_t dir = 0;
    switch(frame.cmd) {
      case somfy_commands::My:
        if(this->shadeType != shade_types::drycontact && !this->isToggle()) {
          writeAssignedGPIO(this->gpioUp, p_off);
          writeAssignedGPIO(this->gpioDown, p_off);
          writeAssignedGPIO(this->gpioMy, p_on);
          dir = 0;
          if(dir != this->gpioDir) ESP_LOGD(TAG, "UP: false, DOWN: false, MY: true");
        }
        break;
      case somfy_commands::Up:
        if(this->shadeType != shade_types::drycontact && !this->isToggle() && this->shadeType != shade_types::drycontact2) {
          writeAssignedGPIO(this->gpioMy, p_off);
          writeAssignedGPIO(this->gpioDown, p_off);
          writeAssignedGPIO(this->gpioUp, p_on);
          dir = -1;
          ESP_LOGD(TAG, "UP: true, DOWN: false, MY: false");
        }
        break;
      case somfy_commands::Toggle:
      case somfy_commands::Down:
        if(this->shadeType != shade_types::drycontact && !this->isToggle() && this->shadeType != shade_types::drycontact2) {
          writeAssignedGPIO(this->gpioMy, p_off);
          writeAssignedGPIO(this->gpioUp, p_off);
        }
        writeAssignedGPIO(this->gpioDown, p_on);
        dir = 1;
        ESP_LOGD(TAG, "UP: false, DOWN: true, MY: false");
        break;
      case somfy_commands::MyUp:
        if(this->shadeType != shade_types::drycontact && !this->isToggle() && this->shadeType != shade_types::drycontact2) {
          writeAssignedGPIO(this->gpioDown, p_off);
          writeAssignedGPIO(this->gpioMy, p_on);
          writeAssignedGPIO(this->gpioUp, p_on);
          ESP_LOGD(TAG, "UP: true, DOWN: false, MY: true");
        }
        break;
      case somfy_commands::MyDown:
        if(this->shadeType != shade_types::drycontact && !this->isToggle() && this->shadeType != shade_types::drycontact2) {
          writeAssignedGPIO(this->gpioUp, p_off);
          writeAssignedGPIO(this->gpioMy, p_on);
          writeAssignedGPIO(this->gpioDown, p_on);
          ESP_LOGD(TAG, "UP: false, DOWN: true, MY: true");
        }
        break;
      case somfy_commands::MyUpDown:
        if(this->shadeType != shade_types::drycontact && this->isToggle() && this->shadeType != shade_types::drycontact2) {
          writeAssignedGPIO(this->gpioUp, p_on);
          writeAssignedGPIO(this->gpioMy, p_on);
          writeAssignedGPIO(this->gpioDown, p_on);
          ESP_LOGD(TAG, "UP: true, DOWN: true, MY: true");
        }
        break;
      default:
        break;
    }
    this->gpioRelease = millis() + (frame.repeats * 200);
    this->gpioDir = dir;
  }  
}
bool SomfyShade::usesPin(uint8_t pin) {
  if(pin==255)return false;
  const uint8_t mask=ShadeGpioPolicy::physicalMask(this->proto);
  return ((mask & ShadeGpioPolicy::up) && this->gpioUp==pin) ||
    ((mask & ShadeGpioPolicy::down) && this->gpioDown==pin) ||
    ((mask & ShadeGpioPolicy::my) && this->gpioMy==pin);
}
