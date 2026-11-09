#include <Preferences.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <SPI.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <atomic>
#include <esp_chip_info.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "Utils.h"
#include "BootPins.h"
#include "ConfigSettings.h"
#include "Somfy.h"
#include "outputs/ProtoSocket.h"
#include "ProtoSync.h"
#include "BitLength.h"
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalReceive.h"
#include <esp_timer.h>
#endif
#include "CounterTxPolicy.h"
#include "CounterCommandGuard.h"
#include "SettingsTypedValidation.h"
#include "SettingsDurability.h"
#include "RadioInitMarker.h"
#include "RadioSettingsAdmission.h"
#include "GitUpdate.h"

static const char *TAG = "Somfy";

extern SomfyShadeController somfy;
extern ConfigSettings settings;

uint8_t rxmode = 0;  // Indicates whether the radio is in receive mode.  Just to ensure there isn't more than one interrupt hooked.
#if defined(ESP8266)
    #define RECEIVE_ATTR ICACHE_RAM_ATTR
#elif defined(ESP32)
    #define RECEIVE_ATTR IRAM_ATTR
#else
    #define RECEIVE_ATTR
#endif
#define TX_QUEUE_DELAY 100
#define TX_TASK_PRIORITY 5
#define TX_TASK_STACK 4096
#define TX_RX_IDLE_WAIT 300  // Max ms a repeater frame waits for an incoming frame to finish.
// Serializes all access to the CC1101 (SPI) and the TX/RX pins between the
// radio task, the main loop and the async web server task.
static SemaphoreHandle_t radioMutex = nullptr;
// Serializes every call into the RadioScheduler.  The radio task, the main loop
// and the async web server task all use it.
static SemaphoreHandle_t txQueueMutex = nullptr;
static TaskHandle_t txTaskHandle = nullptr;
static bool radioUpdateResourcesReady(){return radioMutex && txQueueMutex;}
// Number of tasks waiting for the radio to reconfigure it.  A job in flight
// stops after its current frame so they never wait for a whole transmission.
static std::atomic<uint32_t> txPreemptWaiters{0};
struct RadioLock {
  bool locked = true;  // Before the mutex exists there is nothing to serialize against.
  bool owned = false;
  // preempt cuts short a transmission in flight instead of waiting for it.
  RadioLock(TickType_t wait = portMAX_DELAY, bool preempt = false) {
    if(!radioMutex) return;
    if(preempt) ++txPreemptWaiters;
    locked = owned = xSemaphoreTakeRecursive(radioMutex, wait) == pdTRUE;
    if(preempt) --txPreemptWaiters;
  }
  ~RadioLock() { if(owned) xSemaphoreGiveRecursive(radioMutex); }
};
#ifdef ESPSOMFY_STATUS_DISPLAY
#include "DisplayBus.h"
bool tryDisplayBus(void (*transfer)(void *), void *context) {
  RadioLock lock(0);
  if(!lock.locked) return false;
  transfer(context);
  return true;
}
#endif
bool Transceiver::displayStatus(bool &ready, float &frequency) const {
  RadioLock lock(0);
  if(!lock.locked) return false;
  ready = this->available();
  frequency = this->config.frequency;
  return true;
}
struct TxQueueLock {
  bool locked=true,owned=false;
  explicit TxQueueLock(TickType_t wait=portMAX_DELAY) {
    if(txQueueMutex)locked=owned=xSemaphoreTake(txQueueMutex,wait)==pdTRUE;
  }
  ~TxQueueLock() { if(owned)xSemaphoreGive(txQueueMutex); }
};
static int interruptPin = 0;
static volatile bool noiseDetected = false;
static volatile unsigned long noiseStart = 0;
static volatile uint32_t noiseCount = 0;
uint8_t radioBitLength = 56;
void somfy_frame_t::decodeFrame(somfy_rx_t *rx) {
  this->hwsync = rx->cpt_synchro_hw;
  this->pulseCount = rx->pulseCount;
  this->bitLength = rx->bit_length;
  {
    // Don't stall the main loop behind a transmission just to read the RSSI.
    // The frame object is reused so clear the RSSI rather than reporting a stale one.
    RadioLock lock(0);
    this->rssi = lock.locked ? ELECHOUSE_cc1101.getRssi() : 0;
  }
  this->decodeFrame(rx->payload);
}
// Transceiver Implementation
// One boundary for decoder state, receive FIFO, and ISR admission. Setting
// paused under this mux precedes detachInterrupt; an ISR already waiting on
// the mux cannot enqueue a new frame after the installation snapshot.
static portMUX_TYPE rxMux=portMUX_INITIALIZER_UNLOCKED;
static bool rxPausedForInstall=false;
struct RxTaskLock {
  RxTaskLock(){portENTER_CRITICAL(&rxMux);}
  ~RxTaskLock(){portEXIT_CRITICAL(&rxMux);}
};
struct RxInterruptLock {
  bool held=true;
  __attribute__((always_inline)) inline RxInterruptLock(){portENTER_CRITICAL_ISR(&rxMux);}
  __attribute__((always_inline)) inline void release(){if(held){portEXIT_CRITICAL_ISR(&rxMux);held=false;}}
  __attribute__((always_inline)) inline ~RxInterruptLock(){release();}
};
static bool rxInProgress();
static somfy_rx_t somfy_rx;
static somfy_rx_queue_t rx_queue;
static somfy_tx_queue_t tx_queue;
void somfy_rx_queue_t::init() { 
  for (uint8_t i = 0; i < MAX_RX_BUFFER; i++)
    this->items[i].clear();
  memset(&this->index[0], 0xFF, MAX_RX_BUFFER);
  this->length = 0;
}
bool somfy_rx_queue_t::pop(somfy_rx_t *rx) {
  // Read off the data from the oldest index.
  //Serial.println("Popping RX Queue");
  for(int8_t i = MAX_RX_BUFFER - 1; i >= 0; i--) {
    if(this->index[i] < MAX_RX_BUFFER) {
      uint8_t ndx = this->index[i];
      memcpy(rx, &this->items[this->index[i]], sizeof(somfy_rx_t));
      this->items[ndx].clear();
      if(this->length > 0) this->length--;
      this->index[i] = 255;
      return true;      
    }
  }
  return false;
}
void Transceiver::sendFrame(byte *frame, uint8_t sync, uint8_t bitLength) {
  // Keep malformed queued or repeater frames away from the ten-byte payload.
  if(!supportedBitLength(bitLength)) return;
  if(!this->available()) return;
  uint32_t pin = 1 << this->config.TXPin;
  if (sync == 2 || sync == 12) {  // Only with the first frame.  Repeats do not get a wakeup pulse.
    // All information online for the wakeup pulse appears to be incorrect.  While there is a wakeup
    // pulse it only sends an initial pulse.  There is no further delay after this.
    
    // Wake-up pulse
    //Serial.printf("Sending wakeup pulse: %d\n", sync);
    REG_WRITE(GPIO_OUT_W1TS_REG, pin);
    delayMicroseconds(10920);
    //delayMicroseconds(9415);
    
    // There is no silence after the wakeup pulse.  I tested this with Telis and no silence
    // was detected.  I suspect that for some battery powered shades the shade would go back
    // to sleep from the time of the initial pulse while the silence was occurring.
    REG_WRITE(GPIO_OUT_W1TC_REG, pin);
    delayMicroseconds(7357);
    //delayMicroseconds(9565);
    //delay(80);
  }
  // Depending on the bitness of the protocol we will be sending a different hwsync.
  // 56-bit 2 pulses for the first frame and 7 for the repeats
  // 80-bit 24 pulses for the first frame and 14 pulses for the repeats
  for (int i = 0; i < sync; i++) {
    REG_WRITE(GPIO_OUT_W1TS_REG, pin);
    delayMicroseconds(4 * SYMBOL);
    REG_WRITE(GPIO_OUT_W1TC_REG, pin);
    delayMicroseconds(4 * SYMBOL);
  }
  // Software sync
  REG_WRITE(GPIO_OUT_W1TS_REG, pin);
  //delayMicroseconds(4450); -- Initial timing.
  delayMicroseconds(4850);
  // Start 0
  REG_WRITE(GPIO_OUT_W1TC_REG, pin);
  delayMicroseconds(SYMBOL);
  // Payload starting with the most significant bit.  The frame is always supplied in 80 bits
  // but if the protocol is calling for 56 bits it will only send 56 bits of the frame.
  uint8_t last_bit = 0;
  for (byte i = 0; i < bitLength; i++) {
    if (((frame[i / 8] >> (7 - (i % 8))) & 1) == 1) {
      REG_WRITE(GPIO_OUT_W1TC_REG, pin);
      delayMicroseconds(SYMBOL);
      REG_WRITE(GPIO_OUT_W1TS_REG, pin);
      delayMicroseconds(SYMBOL);
      last_bit = 1;
    } else {
      REG_WRITE(GPIO_OUT_W1TS_REG, pin);
      delayMicroseconds(SYMBOL);
      REG_WRITE(GPIO_OUT_W1TC_REG, pin);
      delayMicroseconds(SYMBOL);
      last_bit = 0;
    }
  }
  // End with a 0 no matter what.  This accommodates the 56-bit protocol by telling the
  // motor that there are no more follow on bits.
  if(last_bit == 0) {
    REG_WRITE(GPIO_OUT_W1TS_REG, pin);
    //delayMicroseconds(SYMBOL);
  }
    
  // Inter-frame silence for 56-bit protocols are around 34ms.  However, an 80 bit protocol should
  // reduce this by the transmission of SYMBOL * 24 or 15,360us
  REG_WRITE(GPIO_OUT_W1TC_REG, pin);
  // Below are the original calculations for inter-frame silence.  However, when actually inspecting this from
  // the remote it appears to be closer to 27500us.  The delayMicoseconds call cannot be called with
  // values larger than 16383.
  if(bitLength != 80) {
    delayMicroseconds(13717);
    delayMicroseconds(13717);
  }
}
void RECEIVE_ATTR Transceiver::handleReceive() {
    const unsigned long edgeMicros=micros(),edgeMillis=millis();
    RxInterruptLock receiveLock;
    if(rxPausedForInstall)return;
    if (noiseDetected) return;
    if (somfy.transceiver.config.noiseDetection) {
        const unsigned long now=edgeMillis;
        if (noiseStart == 0) noiseStart = now;
        if (now - noiseStart >= 10000) {
            noiseStart = now;
            noiseCount = 0;
        }
        noiseCount++;
        if (noiseCount > 100) {
            const int noisyPin=interruptPin;
            noiseDetected=true;
            receiveLock.release();
            gpio_intr_disable((gpio_num_t)noisyPin);
            return;
        }
    }
    static unsigned long last_time = 0;
    const long time=edgeMicros;
    const unsigned int duration = time - last_time;
    const PulseResult result = decodePulse(somfy_rx, duration);
    if (result == PulseResult::glitch) return;
    last_time = time;
    if (result == PulseResult::frame) {
        // Since we are operating within the interrupt all data really needs to be static
        // for the handoff to the frame decoder.  For this reason we are buffering up to
        // 3 total frames.  Althought it may not matter considering the length of a packet
        // will likely not push over the loop timing.  For now lets assume that there
        // may be some pressure on the loop for features.
        if(rx_queue.length >= MAX_RX_BUFFER) {
          // We have overflowed the buffer simply empty the last item
          // in this instance we will simply throw it away.
          uint8_t ndx = rx_queue.index[MAX_RX_BUFFER - 1];
          if(ndx < MAX_RX_BUFFER) rx_queue.items[ndx].pulseCount = 0;
          //memset(&this->items[ndx], 0x00, sizeof(somfy_rx_t));
          rx_queue.index[MAX_RX_BUFFER - 1] = 255;
          rx_queue.length--;
        }
        uint8_t first = 0;
        // Place this record in the first empty slot.  There will
        // be one since we cleared a space above should there
        // be an overflow.
        for(uint8_t i = 0; i < MAX_RX_BUFFER; i++) {
          if(rx_queue.items[i].pulseCount == 0) {
            first = i;
            memcpy(&rx_queue.items[i], &somfy_rx, sizeof(somfy_rx_t));
            break;
          }
        }
        // Move the index so that it is the at position 0.  The oldest item will fall off.
        for(uint8_t i = MAX_RX_BUFFER - 1; i > 0; i--) {
          rx_queue.index[i] = rx_queue.index[i - 1];
        }
        rx_queue.length++;
        rx_queue.index[0] = first;
        memset(&somfy_rx.payload, 0x00, sizeof(somfy_rx.payload));
        somfy_rx.cpt_synchro_hw = 0;
        somfy_rx.previous_bit = 0x00;
        somfy_rx.waiting_half_symbol = false;
        somfy_rx.cpt_bits = 0;
        somfy_rx.pulseCount = 0;
        somfy_rx.status = waiting_synchro;
    }
}
float currFreq = 433.0f;
int currRSSI = -100;
float markFreq = 433.0f;
int markRSSI = -100;
uint32_t lastScan = 0;
static uint32_t scanStart = 0;
// Scan state changes made off loop() (the web handlers and apply() run on
// async_tcp) are only flagged here.  The socket's frame buffer is shared, so
// loop() sends the event.
static std::atomic<bool> scanEmitPending{false};
// A scan nobody ended (closed browser tab) stops on its own after this long so
// the radio doesn't stay unable to transmit.
#define SCAN_MAX_MS 300000
void Transceiver::beginFrequencyScan() {
  // The scan retunes the radio so nothing may transmit until it ends.  This
  // also aborts a transmission in flight so the lock below is never held long.
  if(this->available()) this->suspendTx(TX_SUSPEND_SCAN);
  RadioLock lock(portMAX_DELAY, true);
  if(this->available()) {
    this->disableReceive();
    rxmode = 3;
    scanStart = millis();
    pinMode(this->config.RXPin, INPUT);
    interruptPin = digitalPinToInterrupt(this->config.RXPin);
    ELECHOUSE_cc1101.setRxBW(this->config.rxBandwidth);              // Set the Receive Bandwidth in kHz. Value from 58.03 to 812.50. Default is 812.50 kHz.
    ELECHOUSE_cc1101.SetRx();
    markFreq = currFreq = 433.0f;
    markRSSI = -100;
    ELECHOUSE_cc1101.setMHZ(currFreq);
    ESP_LOGD(TAG, "Begin frequency scan on Pin #%d", this->config.RXPin);
    attachInterrupt(interruptPin, handleReceive, CHANGE);
    scanEmitPending = true;
  }
}
void Transceiver::processFrequencyScan(bool received) {
  RadioLock lock;
  if(this->available() && rxmode == 3) {
    if(received) {
      currRSSI = ELECHOUSE_cc1101.getRssi();
      if((long)(markFreq * 100) == (long)(currFreq * 100)) {
        markRSSI = currRSSI;
      }
      else if(currRSSI >-75) {
        if(currRSSI > markRSSI) {
          markRSSI = currRSSI;
          markFreq = currFreq;
        }
      }
    }
    else {
      currRSSI = -100;
    }
    
    if(millis() - lastScan > 100 && (somfy_rx.status == waiting_synchro || noiseDetected)) {
      lastScan = millis();
      this->emitFrequencyScan();
      currFreq += 0.01f;
      if(currFreq > 434.0f) currFreq = 433.0f;
      ELECHOUSE_cc1101.setMHZ(currFreq);
    }
  }
}
void Transceiver::endFrequencyScan() {
  RadioLock lock;
  if(rxmode == 3) {
    rxmode = 0;
    if(interruptPin > 0) detachInterrupt(interruptPin); 
    interruptPin = 0;
    this->config.apply();
    scanEmitPending = true;
  }
  // Even if something else already ended the scan, nothing may keep TX suspended.
  this->resumeTx(TX_SUSPEND_SCAN);
}
void Transceiver::emitFrequencyScan() {
  if(protoSocket.subscribed(proto_feed_t::frequencyScan)) {
    ProtoWriter w = protoSocket.beginFeed(proto_type_t::frequencyScan);
    w.add(proto_field_t::scanning, rxmode == 3);
    w.add(proto_field_t::testFreq, currFreq);
    w.add(proto_field_t::testRssi, (int32_t)currRSSI);
    w.add(proto_field_t::frequency, markFreq);
    w.add(proto_field_t::rssi, (int32_t)markRSSI);
    protoSocket.endFeed(proto_feed_t::frequencyScan);
  }
}
bool Transceiver::receive(somfy_rx_t *rx) {
    bool received;
    { RxTaskLock receiveLock;received=rx_queue.pop(rx); }
    if(received) {
      this->frame.decodeFrame(rx);
      if(this->frame.valid) {
        ESP_LOGI(TAG, "RX ADDR:%d CMD:%s RAW_CMD:0x%X KEY:0x%02X PROTO:%u",
            this->frame.remoteAddress, translateSomfyCommand(this->frame.cmd).c_str(),
            this->frame.rawCmd, this->frame.encKey, (uint8_t)this->frame.proto);
      }
      this->emitFrame(&this->frame, rx);
      return this->frame.valid;
    }
    return false;
}
void Transceiver::emitFrame(somfy_frame_t *frame, somfy_rx_t *rx) {
#ifdef ESPSOMFY_LOCAL_UI
  // This callback has a raw RX record. Never manufacture a learn candidate
  // from a future debug/TX-only emitFrame call or a websocket subscription.
  if(frame && rx)LocalUi::publishDecodedReceive(LocalUi::receiveHub(),*frame,*rx,
      uint64_t(esp_timer_get_time())/1000,rxmode==3);
#endif
  // The largest event: every pulse as up to 10 digits and a comma, plus the
  // other fields and framing.
  static_assert(MAX_TIMINGS * 11 + 512 <= PROTO_MAX_FRAME, "a frame event can outgrow the socket's frame buffer");
  if(protoSocket.subscribed(proto_feed_t::frames)) {
    ProtoWriter w = protoSocket.beginFeed(proto_type_t::frame);
    w.add(proto_field_t::encKey, frame->encKey);
    w.add(proto_field_t::remoteAddress, (uint32_t)frame->remoteAddress);
    w.add(proto_field_t::lastRollingCode, (uint32_t)frame->rollingCode);
    w.add(proto_field_t::command, translateSomfyCommand(frame->cmd).c_str());
    w.add(proto_field_t::rawCmd, frame->rawCmd);
    w.add(proto_field_t::rssi, (int32_t)frame->rssi);
    w.add(proto_field_t::bits, rx ? rx->bit_length : (uint8_t)0);
    w.add(proto_field_t::proto, static_cast<uint8_t>(frame->proto));
    w.add(proto_field_t::valid, frame->valid);
    w.add(proto_field_t::hwsync, frame->hwsync);
    if(frame->cmd == somfy_commands::StepUp || frame->cmd == somfy_commands::StepDown) w.add(proto_field_t::stepSize, frame->stepSize);
    if(rx && rx->pulseCount) {
      w.beginList(proto_field_t::pulses);
      for(uint16_t i = 0; i < rx->pulseCount; i++) w.item(rx->pulses[i]);
      w.endList();
    }
    protoSocket.endFeed(proto_feed_t::frames);
  }
}
void Transceiver::clearReceived(void) {
    RadioLock radio(0);if(!radio.locked)return;
    { RxTaskLock receiveLock;if(rxPausedForInstall)return; }
    //packet_received = false;
    //memset(receive_buffer, 0x00, sizeof(receive_buffer));
    if(this->available())
      //attachInterrupt(interruptPin, handleReceive, FALLING);
      attachInterrupt(interruptPin, handleReceive, CHANGE);
}
void Transceiver::enableReceive(void) {
    RadioLock radio(0);if(!radio.locked)return;
    { RxTaskLock receiveLock;if(rxPausedForInstall)return; }
    uint32_t timing = millis();
    if(rxmode > 0) return;
    if(this->_available.load()) {
      rxmode = 1;
      pinMode(this->config.RXPin, INPUT);
      interruptPin = digitalPinToInterrupt(this->config.RXPin);
      ELECHOUSE_cc1101.SetRx();
      if(BoardProfile::integratedRadio) {
        uint8_t state;
        do {
          delay(1);
          state = ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & 0x1F;
        } while(state != 0x0D && millis() - timing < 20);
        Serial.printf("CC1101 MARCSTATE=0x%02X (RX=0x0D); RF switch readback SW1=%d SW0=%d\n",
          state, digitalRead(BoardProfile::sw1), digitalRead(BoardProfile::sw0));
        if(state != 0x0D) {
          Serial.println("CC1101 failed to enter RX; radio unavailable");
          ELECHOUSE_cc1101.setSidle();
          this->config.radioInit = false;
          this->_available = false;
          rxmode = 0;
          return;
        }
      }
      //attachInterrupt(interruptPin, handleReceive, FALLING);
      attachInterrupt(interruptPin, handleReceive, CHANGE);
      ESP_LOGD(TAG, "Enabled receive on Pin #%d Timing: %ld", this->config.RXPin, millis() - timing);
    }
}
void Transceiver::disableReceive(void) { 
  rxmode = 0;
  if(interruptPin > 0) detachInterrupt(interruptPin); 
  interruptPin = 0;
  
}
settings_save_result_t Transceiver::setConfig(const transceiver_config_t &cfg) {
  return saveRadioConfiguration(*this,cfg);
}
settings_save_result_t Transceiver::setConfigWhilePaused(const transceiver_config_t &cfg,
    const RadioInstallToken &pause,const GitUpdateOwnerToken &owner) {
  if((owner.kind!=GitUpdateExternalOwner::configuration && owner.kind!=GitUpdateExternalOwner::restore) ||
     !gitUpdate.ownsExternal(owner) || !installPauseHeld(pause))
    return {settings_save_state_t::failed,"radio admission"};
  // Persist before touching live pins/config. A partial NVS write is reported
  // honestly, without applying it to the running radio or claiming rollback.
  RadioLock lock(portMAX_DELAY, true);
  if(!lock.locked)return {settings_save_state_t::failed,"radio lock"};
  transceiver_config_t next = cfg;
  next.applyBoardPins();
  const auto result = next.save();
  if(!result.saved()) return result;
  if(this->config.enabled && this->config.TXPin != next.TXPin) pinMode(this->config.TXPin, INPUT);
  ++configurationRevision;
  this->config = next;
  auto applied=this->config.apply();
  applied.liveApplied=true;
  if(!applied.saved()) {
    applied.state=settings_save_state_t::partial;
    settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::radio));
  }
  return applied;
}
bool Transceiver::usesPin(uint8_t pin) {
  if(BoardProfile::usesPin(pin)) return true;
  if(this->config.enabled) {
    if(this->config.SCKPin == pin ||
      this->config.TXPin == pin ||
      this->config.RXPin == pin ||
      this->config.MOSIPin == pin ||
      this->config.MISOPin == pin ||
      this->config.CSNPin == pin)
      return true;
  }
  return false;  
}
bool Transceiver::end() {
    // Nothing may transmit (and re-enable receive) after this.  It is not
    // resumed since the only caller is OTA, which reboots when the upload
    // completes, fails or is abandoned.
    this->suspendTx(TX_SUSPEND_OTA);
    RadioLock lock(portMAX_DELAY, true);
    this->disableReceive();
    return true;
}
void transceiver_config_t::applyChipDefaults() {
    esp_chip_info_t ci;
    esp_chip_info(&ci);
    switch(ci.model) {
      case esp_chip_model_t::CHIP_ESP32S3:
        ESP_LOGD(TAG, "Setting S3 Transceiver Defaults...");
        this->TXPin = 15;
        this->RXPin = 14;
        this->MOSIPin = 11;
        this->MISOPin = 13;
        this->SCKPin = 12;
        this->CSNPin = 10;
        break;
      case esp_chip_model_t::CHIP_ESP32S2:
        this->TXPin = 15;
        this->RXPin = 14;
        this->MOSIPin = 35;
        this->MISOPin = 37;
        this->SCKPin = 36;
        this->CSNPin = 34;
        break;
      case esp_chip_model_t::CHIP_ESP32C3:
        // The framework's default SPI pins (variants/esp32c3).  12-17 hold the
        // SPI flash, and 2, 8 and 9 are strapping pins.
        this->TXPin = 10;
        this->RXPin = 3;
        this->MOSIPin = 6;
        this->MISOPin = 5;
        this->SCKPin = 4;
        this->CSNPin = 7;
        break;
#ifdef CONFIG_IDF_TARGET_ESP32C5
      case esp_chip_model_t::CHIP_ESP32C5:
        // The framework's default SPI pins (variants/esp32c5).  TX and RX take
        // 4 and 5, the LP UART's pins, which nothing here uses; 2, 3, 7 and
        // 25-28 are strapping pins and 11-22 hold UART0, USB and the flash.
        this->TXPin = 4;
        this->RXPin = 5;
        this->MOSIPin = 8;
        this->MISOPin = 9;
        this->SCKPin = 10;
        this->CSNPin = 6;
        break;
#endif
#ifdef CONFIG_IDF_TARGET_ESP32C6
      case esp_chip_model_t::CHIP_ESP32C6:
      // Pinout applicable for the ESP32-C6-WROOM-1 module (ESP32-C6-DevKitC-1-N4)
        this->TXPin = 10;
        this->RXPin = 10;
        this->MOSIPin = 7;
        this->MISOPin = 2;
        this->SCKPin = 6;
        this->CSNPin = 0;
        break;
#endif
#ifdef CONFIG_IDF_TARGET_ESP32P4
      case esp_chip_model_t::CHIP_ESP32P4:
        // The SPI pins of the framework's variant for Waveshare's ESP32-P4-ETH
        // (variants/waveshare_p4_poe_eth); TX and RX take 2 and 3, which that
        // board leaves free.  The C6's SDIO link holds 14-19 and 54, Ethernet
        // 28-31, 34, 35 and 49-52, and 34-38 are strapping pins.  46 is on LDO
        // channel 4, which the core turns on when a pin of it is used.
        this->TXPin = 2;
        this->RXPin = 3;
        this->MOSIPin = 32;
        this->MISOPin = 33;
        this->SCKPin = 27;
        this->CSNPin = 46;
        break;
#endif
      default:
        // RX was on 12, a strapping pin (issue #34).  A board that saved its
        // radio settings keeps its pins.  35 is input only, which RX is; 36
        // and 39 take false interrupts.
        this->TXPin = 13;
        this->RXPin = 35;
        this->MOSIPin = 23;
        this->MISOPin = 19;
        this->SCKPin = 18;
        this->CSNPin = 5;
        break;
    }
    this->applyBoardPins();
}

static void warnBootPin(const char *name, uint8_t pin) {
  const char *effect = bootPinEffect(pin);
  if(effect) ESP_LOGW(TAG, "Radio %s is on GPIO%u, a strapping pin the radio can drive at reset.  %s", name, pin, effect);
}
static bool checkBoardRadio(float frequency) {
  if(!BoardProfile::integratedRadio) return true;
  uint8_t part = ELECHOUSE_cc1101.SpiReadStatus(CC1101_PARTNUM);
  uint8_t version = ELECHOUSE_cc1101.SpiReadStatus(CC1101_VERSION);
  uint8_t gdo0 = ELECHOUSE_cc1101.SpiReadReg(CC1101_IOCFG0);
  uint8_t gdo2 = ELECHOUSE_cc1101.SpiReadReg(CC1101_IOCFG2);
  uint8_t packet = ELECHOUSE_cc1101.SpiReadReg(CC1101_PKTCTRL0);
  uint8_t modem = ELECHOUSE_cc1101.SpiReadReg(CC1101_MDMCFG2);
  uint32_t word = (static_cast<uint32_t>(ELECHOUSE_cc1101.SpiReadReg(CC1101_FREQ2)) << 16) |
    (static_cast<uint32_t>(ELECHOUSE_cc1101.SpiReadReg(CC1101_FREQ1)) << 8) |
    ELECHOUSE_cc1101.SpiReadReg(CC1101_FREQ0);
  float actual = word * (26.0f / 65536.0f);
  Serial.printf("CC1101 PARTNUM=0x%02X VERSION=0x%02X IOCFG0=0x%02X IOCFG2=0x%02X PKTCTRL0=0x%02X MDMCFG2=0x%02X\n",
    part, version, gdo0, gdo2, packet, modem);
  Serial.printf("CC1101 frequency requested=%.3f readback=%.3f MHz; GDO0 TX=%u GDO2 RX=%u\n",
    frequency, actual, BoardProfile::tx, BoardProfile::rx);
  return part == 0 && version != 0 && version != 0xFF &&
    gdo0 == 0x0D && gdo2 == 0x0D && (packet & 0x30) == 0x30 &&
    (modem & 0x70) == 0x30 && actual > frequency - 0.01f && actual < frequency + 0.01f;
}

settings_save_result_t transceiver_config_t::apply() {
    if(!loadStatus.usable() || !validSettings(*this)) {
      radioInit=false;somfy.transceiver._available=false;return {settings_save_state_t::failed,"radio configuration"};
    }
    // A transmission in flight stops after its current frame so the radio can
    // be reconfigured.  Queued frames are already encoded and go out with the
    // new settings.
    RadioLock lock(portMAX_DELAY, true);
    if(!lock.locked){radioInit=false;somfy.transceiver._available=false;return {settings_save_state_t::failed,"radio configuration"};}
    // Reconfiguring the radio ends a frequency scan so TX may resume.
    const bool wasScanning = rxmode == 3;
    somfy.transceiver.disableReceive();
    // Every board withholds availability until initialization and its NVS
    // safety marker have completed; an early failure is never radio readiness.
    somfy.transceiver._available = false;
    if(wasScanning) {
      somfy.transceiver.resumeTx(TX_SUSPEND_SCAN);
      scanEmitPending = true;
    }
    this->applyBoardPins();
    radioBitLength = this->type;    
    if(this->enabled) {
      this->radioInit = false;
      if(!startupInitAllowed) {
        ESP_LOGW(TAG,"Radio initialization previously interrupted; save radio settings to retry");return {settings_save_state_t::failed,"radioInit"};
      }
      const auto before=writeRadioInitMarker(false);
      if(!before.saved()){startupInitAllowed=false;return before;}
      if(!BoardProfile::prepareRadio(this->frequency))return {};
      ESP_LOGD(TAG, "Applying radio settings - Setting Data Pins RX:%u TX:%u", this->RXPin, this->TXPin);
      // The pins the CC1101 drives: at power-up GDO0 puts out a clock, GDO2
      // CHIP_RDYn, and SO follows CHIP_RDYn while CSN is low.  Once it is
      // set up, GDO2 puts out what it hears.  The web UI checks the same
      // (data-src/js/pins.js).
      warnBootPin("TX", this->TXPin);
      if(this->RXPin != this->TXPin) warnBootPin("RX", this->RXPin);
      warnBootPin("MISO", this->MISOPin);
      //if(this->TXPin != this->RXPin)
      //  pinMode(this->TXPin, OUTPUT);
      //pinMode(this->RXPin, INPUT);
      // Essentially these call only preform the two functions above.
      if(this->TXPin == this->RXPin)
        ELECHOUSE_cc1101.setGDO0(this->TXPin); // This pin may be shared.
      else
        ELECHOUSE_cc1101.setGDO(this->TXPin, this->RXPin); // GDO0, GDO2
      ESP_LOGD(TAG, "Setting SPI Pins SCK:%u MISO:%u MOSI:%u CSN:%u", this->SCKPin, this->MISOPin, this->MOSIPin, this->CSNPin);
      ELECHOUSE_cc1101.setSpiPin(this->SCKPin, this->MISOPin, this->MOSIPin, this->CSNPin);
      ESP_LOGD(TAG, "Radio Pins Configured!");
      // Select the configured bus explicitly; the vendored driver preserves
      // Arduino 3 SPI ownership and begins/ends SPI around register accesses.
      if(BoardProfile::integratedRadio)
        SPI.begin(this->SCKPin, this->MISOPin, this->MOSIPin, this->CSNPin);
      ELECHOUSE_cc1101.Init();
      ELECHOUSE_cc1101.setCCMode(0);                            // set config for internal transmission mode.
      ELECHOUSE_cc1101.setMHZ(this->frequency);                 // Here you can set your basic frequency. The lib calculates the frequency automatically (default = 433.92).The cc1101 can: 300-348 MHZ, 387-464MHZ and 779-928MHZ. Read More info from datasheet.
      ELECHOUSE_cc1101.setRxBW(this->rxBandwidth);              // Set the Receive Bandwidth in kHz. Value from 58.03 to 812.50. Default is 812.50 kHz.
      ELECHOUSE_cc1101.setDeviation(this->deviation);           // Set the Frequency deviation in kHz. Value from 1.58 to 380.85. Default is 47.60 kHz.
      ELECHOUSE_cc1101.setPA(this->txPower);                    // Set TxPower. The following settings are possible depending on the frequency band.  (-30  -20  -15  -10  -6    0    5    7    10   11   12) Default is max!
      ELECHOUSE_cc1101.setModulation(2);                        // Set modulation mode. 0 = 2-FSK, 1 = GFSK, 2 = ASK/OOK, 3 = 4-FSK, 4 = MSK.
      ELECHOUSE_cc1101.setManchester(1);                        // Enables Manchester encoding/decoding. 0 = Disable. 1 = Enable.
      ELECHOUSE_cc1101.setPktFormat(3);                         // Format of RX and TX data. 
                                                                // 0 = Normal mode, use FIFOs for RX and TX. 
                                                                // 1 = Synchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins. 
                                                                // 2 = Random TX mode; sends random data using PN9 generator. Used for test. Works as normal mode, setting 0 (00), in RX. 
                                                                // 3 = Asynchronous serial mode, Data in on GDO0 and data out on either of the GDOx pins.
      ELECHOUSE_cc1101.setDcFilterOff(0);                       // Disable digital DC blocking filter before demodulator. Only for data rates â‰¤ 250 kBaud The recommended IF frequency changes when the DC blocking is disabled. 
                                                                // 1 = Disable (current optimized). 
                                                                // 0 = Enable (better sensitivity).
      ELECHOUSE_cc1101.setCrc(0);                               // 1 = CRC calculation in TX and CRC check in RX enabled. 0 = CRC disabled for TX and RX.
      ELECHOUSE_cc1101.setCRC_AF(0);                            // Enable automatic flush of RX FIFO when CRC is not OK. This requires that only one packet is in the RXIFIFO and that packet length is limited to the RX FIFO size.
      ELECHOUSE_cc1101.setSyncMode(4);                          // Combined sync-word qualifier mode. 
                                                                // 0 = No preamble/sync. 
                                                                // 1 = 16 sync word bits detected. 
                                                                // 2 = 16/16 sync word bits detected. 
                                                                // 3 = 30/32 sync word bits detected. 
                                                                // 4 = No preamble/sync, carrier-sense above threshold. 
                                                                // 5 = 15/16 + carrier-sense above threshold. 
                                                                // 6 = 16/16 + carrier-sense above threshold. 
                                                                // 7 = 30/32 + carrier-sense above threshold.
      ELECHOUSE_cc1101.setAdrChk(0);                            // Controls address check configuration of received packages. 
                                                                // 0 = No address check. 
                                                                // 1 = Address check, no broadcast. 
                                                                // 2 = Address check and 0 (0x00) broadcast. 
                                                                // 3 = Address check and 0 (0x00) and 255 (0xFF) broadcast.
    
      
      const bool detected = ELECHOUSE_cc1101.getCC1101();
      // Always collect the integrated board readback, even if detection fails.
      const bool boardRadioValid = checkBoardRadio(this->frequency);
      if(BoardProfile::integratedRadio)
        Serial.printf("CC1101 detection=%s register validation=%s\n",
          detected ? "PASS" : "FAIL", boardRadioValid ? "PASS" : "FAIL");
      const auto after=writeRadioInitMarker(true);
      if(!after.saved()) {
        radioInit=false;startupInitAllowed=false;return after;
      }
      if (!detected || !boardRadioValid) {
          ESP_LOGE(TAG, "Error setting up the radio");
          this->radioInit = false;
      }
      else {
          ESP_LOGI(TAG, "Successfully set up the radio");
          this->radioInit = true;
          somfy.transceiver._available = true;
          somfy.transceiver.enableReceive();
      }
      
    }
    else {
      if(this->radioInit) ELECHOUSE_cc1101.setSidle();
      somfy.transceiver.disableReceive();
      this->radioInit = false;
    }
    //somfy.transceiver.printBuffer = this->printBuffer;
    return {};
}
bool Transceiver::loadReadOnly() {
    this->config.applyChipDefaults();
    return this->config.loadReadOnly();
}
bool Transceiver::begin() {
    if(!radioMutex) radioMutex = xSemaphoreCreateRecursiveMutex();
    if(!txQueueMutex) txQueueMutex = xSemaphoreCreateMutex();
    if(!radioMutex || !txQueueMutex){this->_available=false;return false;}
    if(this->config.loadStatus.usable())this->config.apply();
    else this->_available=false;
    ESP_LOGD(TAG,"Initializing RX Queue");
    { RxTaskLock receiveLock;rx_queue.init(); }
    this->startTxTask();
    return true;
}
void Transceiver::loop() {
  somfy_rx_t rx;
  somfy_tx_event_t evt;
  for(;;) {
    {
      TxQueueLock lock;
      if(!this->sched.popEvent(evt)) break;
    }
    if(evt.type == tx_event_t::dropped)
      ESP_LOGW(TAG, "Radio TX dropped %s for %d", translateSomfyCommand(evt.cmd).c_str(), evt.remoteAddress);
    else if(evt.type == tx_event_t::aborted)
      ESP_LOGW(TAG, "Radio TX cut short %s for %d", translateSomfyCommand(evt.cmd).c_str(), evt.remoteAddress);
    // Line the simulated shades up with what actually went on air.
    somfy.processTxEvent(evt);
  }
  if(scanEmitPending.exchange(false)) this->emitFrequencyScan();
  if (noiseDetected && rxmode != 3 && this->config.noiseDetection) {
    if (millis() - noiseStart > 100) {
      gpio_intr_enable((gpio_num_t)interruptPin);
      noiseDetected = false;
      noiseStart = 0;
      noiseCount = 0;
    }
  }
  if(rxmode == 3 && millis() - scanStart > SCAN_MAX_MS) {
    ESP_LOGW(TAG, "Frequency scan timed out");
    this->endFrequencyScan();
  }
  if(rxmode == 3) {
    if(this->receive(&rx))
      this->processFrequencyScan(true);
    else
      this->processFrequencyScan(false);
  }
  else if (this->receive(&rx)) {
    for(uint8_t i = 0; i < SOMFY_MAX_REPEATERS; i++) {
      if(somfy.repeaters[i] == frame.remoteAddress) {
        tx_queue.push(rx.cpt_synchro_hw, rx.payload, rx.bit_length);
        // We do not want to process this frame until a full frame beat has passed.
        tx_queue.delay_time = millis() + TX_QUEUE_DELAY;
        ESP_LOGD(TAG, "Queued repeater frame...");
        break;
      }
    }
    somfy.processFrame(this->frame, false);
  }
  else {
    somfy.processWaitingFrame();
    // Check to see if there is anything in the buffer
    // Repeater frames are handed to the radio task like any other transmission.
    // One stays here until the radio has room for it rather than being lost
    // while the radio is busy.  While TX is suspended nothing goes out anyway.
    if(tx_queue.length > 0 && millis() > tx_queue.delay_time && !rxInProgress() &&
      (this->txSuspended() || this->txHasRoom())) {
      this->serviceRepeater();
    }
  }
}
uint32_t Transceiver::submitTx(somfy_tx_job_t &job, bool priority, uint16_t delayMs) {
  job.ticket=0;
  if(!somfy.modelLoad.usable())return 0;
  if(!supportedBitLength(job.tx.bit_length)) {
    ESP_LOGE(TAG, "Dropping unsupported %u-bit transmission", job.tx.bit_length);
    return 0;
  }
  if(!txTaskHandle) {
    ESP_LOGE(TAG, "Radio task not started, dropping transmission");
    return 0;
  }
  // Read published readiness without waiting behind a transmission. A queued
  // job checks it again under the radio lock immediately before sending.
  if(!this->available()) return 0;
  uint32_t ticket;
  {
    TxQueueLock lock;
    if(counterTxBlocked(this->counterEdits.blockedAddress(),job))return 0;
    ticket = this->sched.submit(job, priority, delayMs, millis());
  }
  // The radio task may be idle, or waiting on a delayed job this one is ahead of.
  if(ticket) xTaskNotifyGive(txTaskHandle);
  return ticket;
}
RadioUpdateActivity Transceiver::updateActivityLocked() const {
  RadioUpdateActivity out;out.observed=true;
  out.ready=!this->config.enabled || this->available();
  out.schedulerIdle=!this->sched.hasWork();out.suspended=this->sched.suspended();
  out.txSealed=this->sched.sealedForRestart();out.counterBusy=this->counterEdits.active();
  out.receivedFrames=rx_queue.length;out.repeaterFrames=tx_queue.length;
  out.receivingFrame=somfy_rx.cpt_synchro_hw || somfy_rx.cpt_bits || somfy_rx.status==receiving_data;
  out.scanning=rxmode==3;out.receiverPaused=rxPausedForInstall || this->installActive;
  return out;
}
bool Transceiver::updateActivity(RadioUpdateActivity &out) const {
  if(!radioUpdateResourcesReady())return false;
  RadioLock radio(0);if(!radio.locked)return false;
  TxQueueLock queue(0);if(!queue.locked)return false;
  RxTaskLock receiveLock;out=this->updateActivityLocked();return true;
}
bool Transceiver::pauseForInstall(RadioInstallToken &out){return pauseForAdministration(out,true);}
bool Transceiver::pauseForRestore(RadioInstallToken &out){return pauseForAdministration(out,false);}
void Transceiver::serviceUntrustedReceive(){
  if(somfy.modelLoad.usable())return;
  RxTaskLock receiveLock;
  if(rxPausedForInstall)return;
  // Quarantined input cannot apply identities, counters, movement or repeaters.
  // Memory-only and bounded by MAX_RX_BUFFER/MAX_TIMINGS; no decode or driver.
  rx_queue.init();somfy_rx.clear();
}
bool Transceiver::pauseForAdministration(RadioInstallToken &out,bool requireReady) {
  if(out.generation || !radioUpdateResourcesReady())return false; // Never overwrite a retained capability.
  RadioLock radio(0);if(!radio.locked)return false;
  TxQueueLock queue(0);if(!queue.locked)return false;
  {
    RxTaskLock receiveLock;
    const auto activity=this->updateActivityLocked();
    if(this->installGeneration==UINT64_MAX || !activity.quiescent() || (requireReady && !activity.ready) || !this->sched.sealForRestart())return false;
    rxPausedForInstall=true;this->_administrationPaused=true;this->installActive=true;this->installResumeReceive=rxmode==1;
    this->installConfigurationRevision=this->configurationRevision;
    out.generation=++this->installGeneration;
  }
  // No ISR critical section spans GPIO driver calls.
  this->disableReceive();return true;
}
bool Transceiver::installPauseHeld(const RadioInstallToken &token) const {
  if(!token.generation || !radioUpdateResourcesReady())return false;
  RadioLock radio(0);if(!radio.locked)return false;
  TxQueueLock queue(0);if(!queue.locked)return false;
  RxTaskLock receiveLock;
  return this->installActive && this->_administrationPaused.load() && rxPausedForInstall && this->installGeneration==token.generation && this->sched.sealedForRestart();
}
RadioInstallResume Transceiver::resumeAfterInstall(const RadioInstallToken &token) {
  if(!radioUpdateResourcesReady())return RadioInstallResume::busy;
  RadioLock radio(0);if(!radio.locked)return RadioInstallResume::busy;
  TxQueueLock queue(0);if(!queue.locked)return RadioInstallResume::busy;
  bool resume;
  {
    RxTaskLock receiveLock;
    if(!token.generation || !this->installActive || !rxPausedForInstall || token.generation!=this->installGeneration || !this->sched.sealedForRestart())return RadioInstallResume::stale;
    resume=this->config.enabled && (this->configurationRevision!=this->installConfigurationRevision || this->installResumeReceive);
    rxPausedForInstall=false;
  }
  if(resume)this->enableReceive();
  this->installActive=false;this->installResumeReceive=false;this->installConfigurationRevision=0;this->sched.reopenAfterRefusedRestart();
  this->_administrationPaused=false;
  return resume && !this->available() ? RadioInstallResume::unavailable : RadioInstallResume::resumed;
}
bool Transceiver::sealForRestart() {
  TxQueueLock lock;return !tx_queue.length && this->sched.sealForRestart();
}
void Transceiver::reopenAfterRefusedRestart() {
  TxQueueLock lock;this->sched.reopenAfterRefusedRestart();
}
bool Transceiver::restartQuiescent() {
  TxQueueLock lock;return !tx_queue.length && !this->sched.hasWork();
}
bool Transceiver::counterEditUncertain() {
  TxQueueLock lock;return this->counterEdits.uncertain();
}
bool Transceiver::txSuspended() {
  TxQueueLock lock;
  return this->sched.suspended();
}
bool Transceiver::txHasRoom() {
  // Without a radio task submitTx() fails straight away so there is no point waiting.
  if(!txTaskHandle) return true;
  TxQueueLock lock;
  return this->sched.hasRoom();
}
void Transceiver::suspendTx(uint8_t reason) {
  {
    // Suspends and flushes in one go so nothing is queued in between.
    TxQueueLock lock;
    this->sched.suspend(reason, millis());
  }
  if(txTaskHandle) xTaskNotifyGive(txTaskHandle);
}
void Transceiver::resumeTx(uint8_t reason) {
  TxQueueLock lock;
  this->sched.resume(reason);
}
// cpt_synchro_hw is updated by the RX interrupt and read here from the radio task.
static bool rxInProgress() { RxTaskLock receiveLock;return somfy_rx.cpt_synchro_hw!=0; }
bool Transceiver::transmitJob(somfy_tx_job_t &job) {
  somfy_tx_t &tx = job.tx;
  bool complete = true;
  this->beginTransmit();
  this->sendFrame(tx.payload, tx.hwsync, tx.bit_length);
  for(uint8_t i = 0; i < job.repeats; i++) {
    bool keep;
    {
      TxQueueLock lock;
      keep = this->sched.keepSending(job.ticket);
    }
    // Stop early when flushed or when someone is waiting to reconfigure the
    // radio so they get it within a frame.
    if(!keep || txPreemptWaiters > 0) {
      complete = false;
      break;
    }
    // For each 80-bit frame we need to adjust the byte encoding for the
    // silence.
    if(tx.bit_length == 80) job.frame.encode80BitFrame(tx.payload, (uint8_t)(job.firstRepeat + i));
    this->sendFrame(tx.payload, tx.bit_length == 56 ? 7 : 6, tx.bit_length);
  }
  this->endTransmit();
  return complete;
}
void Transceiver::runJob(somfy_tx_job_t &job) {
  if(job.repeater) {
    // Don't step on a remote that is transmitting right now.
    for(uint32_t waited = 0; rxInProgress() && waited < TX_RX_IDLE_WAIT; waited += 5)
      vTaskDelay(pdMS_TO_TICKS(5));
  }
  {
    RadioLock lock;
    bool start;
    {
      // The queue may have been flushed while we waited.
      TxQueueLock qlock;
      start = this->sched.start(job.ticket, millis());
    }
    if(!start) return;
    const bool complete = this->available() && this->transmitJob(job);
    TxQueueLock qlock;
    this->sched.ended(job.ticket, complete, millis());
  }
  // Give the motors a full frame beat between separate transmissions.
  vTaskDelay(pdMS_TO_TICKS(TX_QUEUE_DELAY));
}
void Transceiver::txTask(void *arg) {
  Transceiver *trans = static_cast<Transceiver *>(arg);
  somfy_tx_job_t job;
  for(;;) {
    uint32_t waitMs;
    bool haveJob;
    {
      TxQueueLock lock;
      haveJob = trans->sched.next(millis(), job, waitMs);
    }
    if(haveJob) trans->runJob(job);
    // submitTx() and suspendTx() wake us for a new job or a flush.
    else ulTaskNotifyTake(pdTRUE, waitMs == RADIO_WAIT_FOREVER ? portMAX_DELAY : pdMS_TO_TICKS(waitMs) + 1);
  }
}
void Transceiver::startTxTask() {
  if(txTaskHandle) return;
#if CONFIG_FREERTOS_UNICORE
  xTaskCreate(Transceiver::txTask, "radio_tx", TX_TASK_STACK, this, TX_TASK_PRIORITY, &txTaskHandle);
#else
  // Run beside loop() so the network stack on the other core is never held up.
  xTaskCreatePinnedToCore(Transceiver::txTask, "radio_tx", TX_TASK_STACK, this, TX_TASK_PRIORITY, &txTaskHandle, ARDUINO_RUNNING_CORE);
#endif
  if(!txTaskHandle) ESP_LOGE(TAG, "Unable to start radio TX task");
}
somfy_frame_t& Transceiver::lastFrame() { return this->frame; }
void Transceiver::beginTransmit() {
    if(this->available()) {
      this->disableReceive();
      pinMode(this->config.TXPin, OUTPUT);
      digitalWrite(this->config.TXPin, 0);
      ELECHOUSE_cc1101.SetTx();
    }
}
void Transceiver::endTransmit() {
    if(this->available()) {
      ELECHOUSE_cc1101.setSidle();
      //delay(100);
      this->enableReceive();
    }
}

// Loop-owned explicit counter lifecycle; locks only the scheduler/policy state.
CounterEditAdmission Transceiver::reserveCounterEdit(uint32_t address,bool idle,uint16_t relatedGroups,bool restart) {
  TxQueueLock lock;
  // The raw repeater buffer is loop-owned and may not yet be in the scheduler.
  bool pending=tx_queue.length!=0 || this->sched.suspended() || this->sched.hasPendingFor(address) || this->sched.hasRepeaterWork();
  for(unsigned i=0;i<SOMFY_MAX_GROUPS;++i)if(relatedGroups&(uint16_t(1)<<i))
    pending=pending || this->sched.hasPendingFor(somfy.groups[i].getRemoteAddress());
  return this->counterEdits.reserve(address,idle,pending,restart);
}
void Transceiver::finishCounterEdit(CounterEditResult result,ModelCommitResult model) {
  TxQueueLock lock;this->counterEdits.finish(result,model);
}
CounterEditAdmission Transceiver::reserveCounterFileRecovery(bool restart) {
  TxQueueLock lock;return this->counterEdits.reserveFileRecovery(restart);
}
void Transceiver::finishCounterFileRecovery(ModelCommitResult model) {
  TxQueueLock lock;this->counterEdits.finishFileRecovery(model);
}
uint32_t Transceiver::counterFileRecoveryAddress() {
  TxQueueLock lock;return this->counterEdits.fileRecoveryAddress();
}
CounterQuarantine Transceiver::counterQuarantine() {
  TxQueueLock lock;return this->counterEdits.quarantine();
}
bool Transceiver::counterBlocksIdentity(uint32_t address) {
  TxQueueLock lock;return this->counterEdits.blocks(address);
}

// Loop-only raw producer. Retire only a refused head; preserve later frames.
void Transceiver::serviceRepeater() {
  somfy_tx_job_t job;
  job.repeater=true;tx_queue.peek(&job.tx);
  somfy_tx_t &tx=job.tx;
  char frame_buf[128];
  int fpos=snprintf(frame_buf,sizeof(frame_buf),"Sending frame %d - %d-BIT [",tx.hwsync,tx.bit_length);
  for(uint8_t j=0;j<10;j++)
    fpos+=snprintf(frame_buf+fpos,sizeof(frame_buf)-fpos,"%d%s",tx.payload[j],j<9?", ":"");
  snprintf(frame_buf+fpos,sizeof(frame_buf)-fpos,"]");
  ESP_LOGD(TAG,"%s",frame_buf);
  somfy_frame_t decoded;
  const bool blocked=this->blockedCounterAddress()!=0;
  const bool counterRefused=blocked && (!decodeCounterRepeater(job,decoded) || !counterCommandAllowed(decoded.remoteAddress,decoded.cmd));
  if(counterRefused || this->submitTx(job) || this->txSuspended()) {
    somfy_tx_t sent;tx_queue.pop(&sent);
  }
  tx_queue.delay_time=millis()+TX_QUEUE_DELAY;
}

uint32_t Transceiver::blockedCounterAddress() {
  TxQueueLock lock;return this->counterEdits.blockedAddress();
}
