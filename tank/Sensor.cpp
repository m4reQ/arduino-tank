#include "Sensor.hpp"
#include <Arduino.h>

void Sensor::Setup() const noexcept
{
    pinMode(m_Pin, INPUT);
}

bool Sensor::HasCollision() const noexcept
{
    return digitalRead(m_Pin) == 0;
}
