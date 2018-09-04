#include "mgos_dallas_esp32.h"
#include "DallasESP32.h"

Dallas *mgos_dallas_create_esp32(uint8_t pin, uint8_t rmt_rx, uint8_t rmt_tx) {
  return new DallasESP32(pin, rmt_rx, rmt_tx);
}