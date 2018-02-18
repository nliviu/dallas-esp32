#include <mgos.h>

#include "OnewireESP32.h"
#include "onewire_rmt.h"

OnewireESP32::OnewireESP32(uint8_t pin, uint8_t rmt_rx, uint8_t rmt_tx)
: _ow(onewire_rmt_create(pin, rmt_rx, rmt_tx))
{
}

OnewireESP32::~OnewireESP32()
{
    LOG(LL_INFO, ("Delete _ow"));
    if (_ow) {
        onewire_rmt_close(_ow);
    }
}

uint8_t OnewireESP32::reset(void)
{
    return onewire_rmt_reset(_ow);
}

void OnewireESP32::select(const uint8_t rom[8])
{
    onewire_rmt_select(_ow, rom);
}

void OnewireESP32::skip(void)
{
    onewire_rmt_skip(_ow);
}

void OnewireESP32::write(uint8_t v, uint8_t power)
{
    onewire_rmt_write(_ow, v);
    (void) power;
}

void OnewireESP32::write_bytes(const uint8_t *buf, uint16_t count, bool power)
{
    onewire_rmt_write_bytes(_ow, buf, count);
    (void) power;
}

uint8_t OnewireESP32::read(void)
{
    return onewire_rmt_read(_ow);
}

void OnewireESP32::read_bytes(uint8_t *buf, uint16_t count)
{
    onewire_rmt_read_bytes(_ow, buf, count);
}

void OnewireESP32::write_bit(uint8_t v)
{
    onewire_rmt_write_bit(_ow, v);
}

uint8_t OnewireESP32::read_bit(void)
{
    return (uint8_t) onewire_rmt_read_bit(_ow);
}

void OnewireESP32::depower(void)
{

}

void OnewireESP32::reset_search()
{
    onewire_rmt_search_clean(_ow);
}

void OnewireESP32::target_search(uint8_t family_code)
{
    onewire_rmt_target_setup(_ow, family_code);
}

uint8_t OnewireESP32::search(uint8_t *newAddr, bool search_mode)
{
    return (uint8_t) onewire_rmt_next(_ow, newAddr, !search_mode);
}
