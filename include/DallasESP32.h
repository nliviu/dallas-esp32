#pragma once
#include "Dallas.h"

class DallasESP32 : public Dallas
{
public:
    DallasESP32(uint8_t pin, uint8_t rmt_rx, uint8_t rmt_tx);

    //~DallasESP32();
};
