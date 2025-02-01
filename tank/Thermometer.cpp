#include "Thermometer.hpp"
#include <Arduino.h>

#define TEMP_MIN -40.0f
#define TEMP_MAX 125.0f

void Thermometer::Setup() noexcept
{
    pinMode(m_Pin, INPUT);
}

float Thermometer::GetTemperatureCelsius() noexcept
{
    const int sensorValue = analogRead(m_Pin);
    const float part = (float)sensorValue / 1024.0f;
    return part * (abs(TEMP_MIN) + abs(TEMP_MAX)) - abs(TEMP_MIN);
}
