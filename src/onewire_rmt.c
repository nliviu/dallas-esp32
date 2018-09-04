/*
Adaptation of Paul Stoffregen's One wire library to the NodeMcu

Ported to ESP32 RMT peripheral for low-level signal generation by Arnim Laeuger.

The latest version of this library may be found at:
  http://www.pjrc.com/teensy/td_libs_OneWire.html

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Much of the code was inspired by Derek Yerger's code, though I don't
think much of that remains.  In any event that was..
    (copyleft) 2006 by Derek Yerger - Free to distribute freely.

The CRC code was excerpted and inspired by the Dallas Semiconductor
sample code bearing this copyright.
//---------------------------------------------------------------------------
// Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Dallas Semiconductor
// shall not be used except as stated in the Dallas Semiconductor
// Branding Policy.
//--------------------------------------------------------------------------
 */
// https://raw.githubusercontent.com/nodemcu/nodemcu-firmware/dev-esp32/components/platform/onewire.c

#include <stdbool.h>
#include <mgos.h>

#include "driver/rmt.h"
#include "driver/gpio.h"
#include "onewire_rmt.h"

// *****************************************************************************
// Onewire platform interface

// bus reset: duration of low phase [us]
#define OW_DURATION_RESET 480
// overall slot duration
#define OW_DURATION_SLOT 75
// write 1 slot and read slot durations [us]
#define OW_DURATION_1_LOW 2
#define OW_DURATION_1_HIGH (OW_DURATION_SLOT - OW_DURATION_1_LOW)
// write 0 slot durations [us]
#define OW_DURATION_0_LOW 65
#define OW_DURATION_0_HIGH (OW_DURATION_SLOT - OW_DURATION_0_LOW)
// sample time for read slot
#define OW_DURATION_SAMPLE (15 - 2)
// RX idle threshold
// needs to be larger than any duration occurring during write slots
#define OW_DURATION_RX_IDLE (OW_DURATION_SLOT + 2)

// Strong pull-up aka power mode is implemented by the pad's push-pull driver.
// Open-drain configuration is used for normal operation.
// power bus by disabling open-drain:
#define OW_POWER(g) GPIO.pin[g].pad_driver = 0
// de-power bus by enabling open-drain:
#define OW_DEPOWER(g) GPIO.pin[g].pad_driver = 1

// grouped information for RMT management

static struct {
  int tx, rx;
  RingbufHandle_t rb;
  int gpio;
} ow_rmt = {-1, -1, NULL, -1};

// default power mode for generic write operations
static const uint8_t owDefaultPower = 0;

static bool onewire_rmt_init(int gpio_num, int rx_channel, int tx_channel) {
  ow_rmt.tx = tx_channel;
  ow_rmt.rx = rx_channel;
// acquire an RMT module for TX and RX each
#ifdef OW_DEBUG
  ESP_LOGI("ow", "RMT TX channel: %d", ow_rmt.tx);
  ESP_LOGI("ow", "RMT RX channel: %d", ow_rmt.rx);
#endif
  LOG(LL_INFO, ("RMT RX channel: %d, TX channel: %d", ow_rmt.rx, ow_rmt.tx));
  rmt_config_t rmt_tx;
  rmt_tx.channel = ow_rmt.tx;
  rmt_tx.gpio_num = gpio_num;
  rmt_tx.mem_block_num = 1;
  rmt_tx.clk_div = 80;
  rmt_tx.tx_config.loop_en = false;
  rmt_tx.tx_config.carrier_en = false;
  rmt_tx.tx_config.idle_level = 1;
  rmt_tx.tx_config.idle_output_en = true;
  rmt_tx.rmt_mode = RMT_MODE_TX;
  if (rmt_config(&rmt_tx) == ESP_OK) {
    if (rmt_driver_install(rmt_tx.channel, 0,
                           ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM |
                               ESP_INTR_FLAG_SHARED) == ESP_OK) {
      rmt_config_t rmt_rx;
      rmt_rx.channel = ow_rmt.rx;
      rmt_rx.gpio_num = gpio_num;
      rmt_rx.clk_div = 80;
      rmt_rx.mem_block_num = 1;
      rmt_rx.rmt_mode = RMT_MODE_RX;
      rmt_rx.rx_config.filter_en = true;
      rmt_rx.rx_config.filter_ticks_thresh = 30;
      rmt_rx.rx_config.idle_threshold = OW_DURATION_RX_IDLE;
      if (rmt_config(&rmt_rx) == ESP_OK) {
        if (rmt_driver_install(rmt_rx.channel, 512,
                               ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM |
                                   ESP_INTR_FLAG_SHARED) == ESP_OK) {
          rmt_get_ringbuf_handle(ow_rmt.rx, &ow_rmt.rb);
          // don't set ow_rmt.gpio here
          // -1 forces a full pin set procedure in first call to
          // onewire_rmt_attach_pin()
          return true;
        }
      }
      rmt_driver_uninstall(rmt_tx.channel);
    }
  }

  return false;
}
// check rmt TX&RX channel assignment and eventually attach them to the
// requested pin

// flush any pending/spurious traces from the RX channel

static void onewire_flush_rmt_rx_buf(void) {
  void *p;
  size_t s;

  while ((p = xRingbufferReceive(ow_rmt.rb, &s, 0))) {
    vRingbufferReturnItem(ow_rmt.rb, p);
  }
}

// check rmt TX&RX channel assignment and eventually attach them to the
// requested pin

static bool onewire_rmt_attach_pin(int gpio_num) {
  if (ow_rmt.tx < 0 || ow_rmt.rx < 0) return false;

  if (gpio_num != ow_rmt.gpio) {
    // attach GPIO to previous pin
    if (gpio_num < 32) {
      GPIO.enable_w1ts = (0x1 << gpio_num);
    } else {
      GPIO.enable1_w1ts.data = (0x1 << (gpio_num - 32));
    }
    if (ow_rmt.gpio >= 0) {
      gpio_matrix_out(ow_rmt.gpio, SIG_GPIO_OUT_IDX, 0, 0);
    }

    // attach RMT channels to new gpio pin
    // ATTENTION: set pin for rx first since gpio_output_disable() will
    //            remove rmt output signal in matrix!
    rmt_set_pin(ow_rmt.rx, RMT_MODE_RX, gpio_num);
    rmt_set_pin(ow_rmt.tx, RMT_MODE_TX, gpio_num);
    // force pin direction to input to enable path to RX channel
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[gpio_num]);

    ow_rmt.gpio = gpio_num;
  }

  return true;
}

static rmt_item32_t onewire_encode_write_slot(uint8_t val) {
  rmt_item32_t item;

  item.level0 = 0;
  item.level1 = 1;
  if (val) {
    // write "1" slot
    item.duration0 = OW_DURATION_1_LOW;
    item.duration1 = OW_DURATION_1_HIGH;
  } else {
    // write "0" slot
    item.duration0 = OW_DURATION_0_LOW;
    item.duration1 = OW_DURATION_0_HIGH;
  }

  return item;
}

static bool onewire_write_bits(uint8_t gpio_num, uint8_t data, uint8_t num,
                               uint8_t power) {
  rmt_item32_t tx_items[num + 1];

  if (num > 8) {
    return false;
  }

  if (onewire_rmt_attach_pin(gpio_num) != true) {
    return false;
  }

  if (power) {
    // apply strong driver to power the bus
    OW_POWER(gpio_num);
  } else {
    // switch to open-drain mode, bus is powered by external pull-up
    OW_DEPOWER(gpio_num);
  }

  // write requested bits as pattern to TX buffer
  for (int i = 0; i < num; i++) {
    tx_items[i] = onewire_encode_write_slot(data & 0x01);
    data >>= 1;
  }
  // end marker
  tx_items[num].level0 = 1;
  tx_items[num].duration0 = 0;

  if (rmt_write_items(ow_rmt.tx, tx_items, num + 1, true) == ESP_OK) {
    return true;
  } else {
    return false;
  }
}

static rmt_item32_t onewire_encode_read_slot(void) {
  rmt_item32_t item;

  // construct pattern for a single read time slot
  item.level0 = 0;
  item.duration0 = OW_DURATION_1_LOW;  // shortly force 0
  item.level1 = 1;
  item.duration1 = OW_DURATION_1_HIGH;  // release high and finish slot

  return item;
}

static bool onewire_read_bits(uint8_t gpio_num, uint8_t *data, uint8_t num) {
  rmt_item32_t tx_items[num + 1];
  uint8_t read_data = 0;
  int res = true;

  if (num > 8) {
    return false;
  }

  if (onewire_rmt_attach_pin(gpio_num) != true) {
    return false;
  }

  OW_DEPOWER(gpio_num);

  // generate requested read slots
  for (int i = 0; i < num; i++) {
    tx_items[i] = onewire_encode_read_slot();
  }
  // end marker
  tx_items[num].level0 = 1;
  tx_items[num].duration0 = 0;

  onewire_flush_rmt_rx_buf();
  rmt_rx_start(ow_rmt.rx, true);
  if (rmt_write_items(ow_rmt.tx, tx_items, num + 1, true) == ESP_OK) {
    size_t rx_size;
    rmt_item32_t *rx_items =
        (rmt_item32_t *) xRingbufferReceive(ow_rmt.rb, &rx_size, portMAX_DELAY);

    if (rx_items) {
#ifdef OW_DEBUG
      for (int i = 0; i < rx_size / 4; i++) {
        ESP_LOGI("ow", "level: %d, duration %d", rx_items[i].level0,
                 rx_items[i].duration0);
        ESP_LOGI("ow", "level: %d, duration %d", rx_items[i].level1,
                 rx_items[i].duration1);
      }
#endif
      if (rx_size >= num * sizeof(rmt_item32_t)) {
        for (int i = 0; i < num; i++) {
          read_data >>= 1;
          // parse signal and identify logical bit
          if (rx_items[i].level1 == 1) {
            if ((rx_items[i].level0 == 0) &&
                (rx_items[i].duration0 < OW_DURATION_SAMPLE)) {
              // rising edge occured before 15us -> bit 1
              read_data |= 0x80;
            }
          }
        }
        read_data >>= 8 - num;
      }

      vRingbufferReturnItem(ow_rmt.rb, (void *) rx_items);
    } else {
      // time out occurred, this indicates an unconnected / misconfigured bus
      res = false;
    }

  } else {
    // error in tx channel
    res = false;
  }

  rmt_rx_stop(ow_rmt.rx);

  *data = read_data;
  return res;
}

struct onewire_search_state {
  int search_mode;
  int last_device;
  int last_discrepancy;
  int last_family_discrepancy;
  uint8_t rom[8];
  uint8_t crc8;
};

typedef struct {
  uint8_t power;
  uint8_t LastDeviceFlag;
  uint8_t LastDiscrepancy;
  uint8_t LastFamilyDiscrepancy;
  unsigned char ROM_NO[8];
} platform_onewire_bus_t;

struct mgos_rmt_onewire {
  int pin;
  uint8_t *res_rom;
  // struct onewire_search_state sst;
  platform_onewire_bus_t sst;
  int rmt_rx;
  int rmt_tx;
};

struct mgos_rmt_onewire *onewire_rmt_create(int pin, int rmt_rx, int rmt_tx) {
  int rx = rmt_rx;  // mgos_sys_config_get_onewire_rmt_rx_channel();
  int tx = rmt_tx;  // mgos_sys_config_get_onewire_rmt_tx_channel();
  if (-1 == rx || -1 == tx) {
    LOG(LL_INFO,
        ("onewire_rmt could not start - rx and/or tx channel not set."));
    return NULL;
  }
  bool driverOk = onewire_rmt_init(pin, rx, tx);
  if (false == driverOk) {
    LOG(LL_INFO,
        ("onewire_rmt could not start - rmt device could not be configured."));
    return NULL;
  }
  struct mgos_rmt_onewire *ow =
      (struct mgos_rmt_onewire *) calloc(1, sizeof(struct mgos_rmt_onewire));
  ow->pin = pin;
  ow->rmt_rx = rmt_rx;
  ow->rmt_tx = rmt_tx;
  return ow;
}

void onewire_rmt_close(struct mgos_rmt_onewire *ow) {
  if (NULL != ow) {
    esp_err_t resRx = rmt_driver_uninstall(ow->rmt_rx);
    esp_err_t resTx = rmt_driver_uninstall(ow->rmt_tx);
    free((void *) ow);
    ow_rmt.tx = -1;
    ow_rmt.rx = -1;
    ow_rmt.rb = NULL;
    ow_rmt.gpio = -1;
    // LOG(LL_INFO, ("CLOSE onewire_rmt: resRx=%d, resTx=%d", (int) resRx, (int)
    // resTx));
  }
}

bool onewire_rmt_reset(struct mgos_rmt_onewire *ow) {
  (void) ow;
  rmt_item32_t tx_items[1];
  bool _presence = false;
  int res = true;
  int gpio_num = ow->pin;

  if (onewire_rmt_attach_pin(gpio_num) != true) return false;

  OW_DEPOWER(gpio_num);

  tx_items[0].duration0 = OW_DURATION_RESET;
  tx_items[0].level0 = 0;
  tx_items[0].duration1 = 0;
  tx_items[0].level1 = 1;

  uint16_t old_rx_thresh;
  rmt_get_rx_idle_thresh(ow_rmt.rx, &old_rx_thresh);
  rmt_set_rx_idle_thresh(ow_rmt.rx, OW_DURATION_RESET + 60);

  onewire_flush_rmt_rx_buf();
  rmt_rx_start(ow_rmt.rx, true);
  if (rmt_write_items(ow_rmt.tx, tx_items, 1, true) == ESP_OK) {
    size_t rx_size;
    rmt_item32_t *rx_items = (rmt_item32_t *) xRingbufferReceive(
        ow_rmt.rb, &rx_size, 100 / portTICK_PERIOD_MS);

    if (rx_items) {
      if (rx_size >= 1 * sizeof(rmt_item32_t)) {
#ifdef OW_DEBUG
        for (int i = 0; i < rx_size / 4; i++) {
          ESP_LOGI("ow", "level: %d, duration %d", rx_items[i].level0,
                   rx_items[i].duration0);
          ESP_LOGI("ow", "level: %d, duration %d", rx_items[i].level1,
                   rx_items[i].duration1);
        }
#endif

        // parse signal and search for presence pulse
        if ((rx_items[0].level0 == 0) &&
            (rx_items[0].duration0 >= OW_DURATION_RESET - 2))
          if ((rx_items[0].level1 == 1) && (rx_items[0].duration1 > 0))
            if (rx_items[1].level0 == 0) _presence = true;
      }

      vRingbufferReturnItem(ow_rmt.rb, (void *) rx_items);
    } else {
      // time out occurred, this indicates an unconnected / misconfigured bus
      res = false;
    }

  } else {
    // error in tx channel
    res = false;
  }

  rmt_rx_stop(ow_rmt.rx);
  rmt_set_rx_idle_thresh(ow_rmt.rx, old_rx_thresh);

  //*presence = _presence;
  // return res;
  (void) res;
  return _presence;
}

// This table comes from Dallas sample code where it is freely reusable,
// though Copyright (C) 2000 Dallas Semiconductor Corporation
/*static const uint8_t crc_table[] = {
    0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31,
    65, 157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96,
    130, 220, 35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128,
    222, 60, 98, 190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158,
    29, 67, 161, 255, 70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56,
    102, 229, 187, 89, 7, 219, 133, 103, 57, 186, 228, 6, 88, 25, 71,
    165, 251, 120, 38, 196, 154, 101, 59, 217, 135, 4, 90, 184, 230, 167,
    249, 27, 69, 198, 152, 122, 36, 248, 166, 68, 26, 153, 199, 37, 123,
    58, 100, 134, 216, 91, 5, 231, 185, 140, 210, 48, 110, 237, 179, 81,
    15, 78, 16, 242, 172, 47, 113, 147, 205, 17, 79, 173, 243, 112, 46,
    204, 146, 211, 141, 111, 49, 178, 236, 14, 80, 175, 241, 19, 77, 206,
    144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238, 50, 108, 142, 208,
    83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115, 202, 148, 118,
    40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139, 87, 9,
    235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22, 233,
    183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
    116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107,
    53
};

uint8_t onewire_rmt_crc8(const uint8_t *rom, int len)
{
    uint8_t res = 0x00;
    while (len-- > 0) {
        res = crc_table[res ^ *rom++];
    }
    return res;
}
 */

/*
 * Setup the search to find the device type 'family_code'
 * on the next call onewire_next() if it is present
 * Note if no devices of the desired family are currently
 * on the 1-Wire, then another type will be found.
 */
void onewire_rmt_target_setup(struct mgos_rmt_onewire *ow,
                              const uint8_t family_code) {
  // set the search state to find SearchFamily type devices
  ow->sst.ROM_NO[0] = family_code;
  uint8_t i;
  for (i = 1; i < 8; i++) {
    ow->sst.ROM_NO[i] = 0;
  }
  ow->sst.LastDiscrepancy = 64;
  ow->sst.LastFamilyDiscrepancy = 0;
  ow->sst.LastDeviceFlag = false;
}

//
// Perform a search. If this function returns a '1' then it has
// enumerated the next device and you may retrieve the ROM from the
// OneWire::address variable. If there are no devices, no further
// devices, or something horrible happens in the middle of the
// enumeration then a 0 is returned.  If a new device is found then
// its address is copied to newAddr.  Use OneWire::reset_search() to
// start over.
//
// --- Replaced by the one from the Dallas Semiconductor web site ---
//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return true  : device found, ROM number in ROM_NO buffer
//        false : device not found, end of search
//

bool onewire_rmt_next(struct mgos_rmt_onewire *ow, uint8_t *rom, int mode) {
  (void) mode;
  uint8_t id_bit_number;
  uint8_t last_zero, rom_byte_number, search_result;
  uint8_t id_bit, cmp_id_bit;

  unsigned char rom_byte_mask, search_direction;

  // initialize for search
  id_bit_number = 1;
  last_zero = 0;
  rom_byte_number = 0;
  rom_byte_mask = 1;
  search_result = 0;

  // if the last call was not the last one
  if (!ow->sst.LastDeviceFlag) {
    // 1-Wire reset
    // uint8_t presence;
    if (onewire_rmt_reset(ow) != true) {
      // reset the search
      ow->sst.LastDiscrepancy = 0;
      ow->sst.LastDeviceFlag = false;
      ow->sst.LastFamilyDiscrepancy = 0;
      return false;
    }

    // issue the search command
    onewire_write_bits(ow->pin, 0xF0, 8, owDefaultPower);

    // loop to do the search
    do {
      // read a bit and its complement
      if (onewire_read_bits(ow->pin, &id_bit, 1) != true) {
        break;
      }
      if (onewire_read_bits(ow->pin, &cmp_id_bit, 1) != true) {
        break;
      }

      // check for no devices on 1-wire
      if ((id_bit == 1) && (cmp_id_bit == 1)) {
        break;
      } else {
        // all devices coupled have 0 or 1
        if (id_bit != cmp_id_bit)
          search_direction = id_bit;  // bit write value for search
        else {
          // if this discrepancy if before the Last Discrepancy
          // on a previous next then pick the same as last time
          if (id_bit_number < ow->sst.LastDiscrepancy)
            search_direction =
                ((ow->sst.ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
          else
            // if equal to last pick 1, if not then pick 0
            search_direction = (id_bit_number == ow->sst.LastDiscrepancy);

          // if 0 was picked then record its position in LastZero
          if (search_direction == 0) {
            last_zero = id_bit_number;

            // check for Last discrepancy in family
            if (last_zero < 9) {
              ow->sst.LastFamilyDiscrepancy = last_zero;
            }
          }
        }

        // set or clear the bit in the ROM byte rom_byte_number
        // with mask rom_byte_mask
        if (search_direction == 1) {
          ow->sst.ROM_NO[rom_byte_number] |= rom_byte_mask;
        } else {
          ow->sst.ROM_NO[rom_byte_number] &= ~rom_byte_mask;
        }

        // serial number search direction write bit
        onewire_write_bits(ow->pin, search_direction, 1, owDefaultPower);

        // increment the byte counter id_bit_number
        // and shift the mask rom_byte_mask
        id_bit_number++;
        rom_byte_mask <<= 1;

        // if the mask is 0 then go to new SerialNum byte rom_byte_number and
        // reset mask
        if (rom_byte_mask == 0) {
          rom_byte_number++;
          rom_byte_mask = 1;
        }
      }
    } while (rom_byte_number < 8);  // loop until through all ROM bytes 0-7

    // if the search was successful then
    if (!(id_bit_number < 65)) {
      // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
      ow->sst.LastDiscrepancy = last_zero;

      // check for last device
      if (ow->sst.LastDiscrepancy == 0) {
        ow->sst.LastDeviceFlag = true;
      }

      search_result = true;
    }
  }

  // if no device found then reset counters so next 'search' will be like a
  // first
  if (!search_result || !ow->sst.ROM_NO[0]) {
    ow->sst.LastDiscrepancy = 0;
    ow->sst.LastDeviceFlag = false;
    ow->sst.LastFamilyDiscrepancy = 0;
    search_result = false;
  } else {
    for (rom_byte_number = 0; rom_byte_number < 8; rom_byte_number++) {
      rom[rom_byte_number] = ow->sst.ROM_NO[rom_byte_number];
    }
  }
  return search_result;
}

void onewire_rmt_select(struct mgos_rmt_onewire *ow, const uint8_t *rom) {
  // onewire_write(ow, 0x55);
  onewire_write_bits(ow->pin, 0x55, 8, owDefaultPower);
  for (int i = 0; i < 8; i++) {
    // onewire_write(ow, rom[i]);
    onewire_write_bits(ow->pin, rom[i], 8, owDefaultPower);
  }
}

void onewire_rmt_skip(struct mgos_rmt_onewire *ow) {
  // onewire_write(ow, 0xCC);
  onewire_write_bits(ow->pin, 0xCC, 8, owDefaultPower);
}

void onewire_rmt_search_clean(struct mgos_rmt_onewire *ow) {
  memset(&ow->sst, 0, sizeof(ow->sst));
}

bool onewire_rmt_read_bit(struct mgos_rmt_onewire *ow) {
  uint8_t bit = 0;
  if (onewire_read_bits(ow->pin, &bit, 1)) {
    return bit & 0x01;
  }
  return false;
}

uint8_t onewire_rmt_read(struct mgos_rmt_onewire *ow) {
  uint8_t res = 0;
  if (onewire_read_bits(ow->pin, &res, 8) != true) {
    return 0;
  }
  return res;
}

void onewire_rmt_read_bytes(struct mgos_rmt_onewire *ow, uint8_t *buf,
                            int len) {
  for (uint16_t i = 0; i < len; i++) {
    if (onewire_read_bits(ow->pin, buf, 8) != true) {
      return;  // PLATFORM_ERR;
    }
    buf++;
  }
}

void onewire_rmt_write_bit(struct mgos_rmt_onewire *ow, int bit) {
  uint8_t data = 0x01 & bit;
  onewire_write_bits(ow->pin, data, 1, owDefaultPower);
}

void onewire_rmt_write(struct mgos_rmt_onewire *ow, const uint8_t data) {
  onewire_write_bits(ow->pin, data, 8, owDefaultPower);
}

void onewire_rmt_write_bytes(struct mgos_rmt_onewire *ow, const uint8_t *buf,
                             int len) {
  for (uint16_t i = 0; i < len; i++) {
    if (onewire_write_bits(ow->pin, buf[i], 8, owDefaultPower) != true) {
      return;  // PLATFORM_ERR;
    }
  }

  return;  // PLATFORM_OK;
}
