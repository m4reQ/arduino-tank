#pragma once
#include <stdint.h>
#include <Servo.h>
#include <HCSR04.h>

class HeadSensor
{
public:
    HeadSensor(uint8_t servoPin, uint8_t sensorTriggerPin, uint8_t sensorEchoPin) noexcept
        : m_ServoPin(servoPin), m_SensorTriggerPin(sensorTriggerPin), m_SensorEchoPin(sensorEchoPin) {}

    void Setup() noexcept;
    void RotateLeft(uint8_t angleDeg) noexcept;
    void RotateRight(uint8_t angleDeg) noexcept;
    void SetRotation(uint8_t rotation) noexcept;
    float GetDistance(float temperature) noexcept;

    constexpr uint8_t GetCurrentRotation() const noexcept { return m_Rotation; }

private:
    Servo m_Servo;
    HCSR04Sensor m_Sensor;
    uint8_t m_ServoPin;
    uint8_t m_SensorTriggerPin;
    uint8_t m_SensorEchoPin;
    uint8_t m_Rotation;
};
