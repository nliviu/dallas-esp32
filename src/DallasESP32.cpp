#include "DallasESP32.h"
#include "OnewireESP32.h"

DallasESP32::DallasESP32(uint8_t pin, uint8_t rmt_rx, uint8_t rmt_tx)
    : Dallas() {
  _ow = new OnewireESP32(pin, rmt_rx, rmt_tx);
  _ownOnewire = true;
}

DallasESP32::~DallasESP32() {
}
