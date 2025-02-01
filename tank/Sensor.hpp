#pragma once
#include <stdint.h>

class Sensor
{
public:
    constexpr Sensor(uint8_t sensorPin)
        : m_Pin(sensorPin) {}

    void Setup() const noexcept;
    bool HasCollision() const noexcept;

private:
    uint8_t m_Pin;
};
