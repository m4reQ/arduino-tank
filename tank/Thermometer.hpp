#pragma once
#include <stdint.h>

class Thermometer
{
public:
    constexpr Thermometer(uint8_t pin) noexcept
        : m_Pin(pin) {}

    void Setup() noexcept;
    float GetTemperatureCelsius() noexcept;

private:
    uint8_t m_Pin;
};
