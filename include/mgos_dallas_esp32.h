#pragma once

#include "mgos_dallas_interface.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Initializes the Dallas driver with a GPIO `pin`
 * and the RMT channels `rmt_rx` and `rmt_tx`
 * Return value: handle opaque pointer.
 */
Dallas* mgos_dallas_create_esp32(uint8_t pin, uint8_t rmt_rx, uint8_t rmt_tx);

#ifdef __cplusplus
}
#endif

