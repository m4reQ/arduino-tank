#include "HeadSensor.hpp"

void HeadSensor::Setup() noexcept
{
    m_Servo.attach(m_ServoPin);
    m_Sensor.begin(m_SensorTriggerPin, m_SensorEchoPin);
}

void HeadSensor::RotateLeft(uint8_t angleDeg) noexcept
{
    SetRotation(max(m_Rotation + angleDeg, 0));
}

void HeadSensor::RotateRight(uint8_t angleDeg) noexcept
{
    SetRotation(min(m_Rotation + angleDeg, 255));
}

float HeadSensor::GetDistance(float temperature) noexcept
{
    double result;
    m_Sensor.measureDistanceMm(temperature, &result);

    return (float)result;
}

void HeadSensor::SetRotation(uint8_t rotation) noexcept
{
    m_Rotation = rotation;
    m_Servo.write(m_Rotation);
}
