#include "Engine.hpp"
#include <Arduino.h>

void Engine::Setup() noexcept
{
    pinMode(m_DirectionPinLeft, OUTPUT);
    pinMode(m_DirectionPinRight, OUTPUT);
    pinMode(m_SpeedPinLeft, OUTPUT);
    pinMode(m_SpeedPinRight, OUTPUT);
}

void Engine::Stop() noexcept
{
    SetSpeed(0);
}

void Engine::SetDirection(Direction direction) noexcept
{
    if (direction == DIR_CURRENT)
        direction = m_State.Direction;

    digitalWrite(m_DirectionPinLeft, (direction & 0b10) ? 255 : 0);
    digitalWrite(m_DirectionPinRight, (direction & 0b01) ? 255 : 0);

    m_State.Direction = direction;
}

void Engine::SetSpeed(uint8_t speed) noexcept
{
    digitalWrite(m_SpeedPinLeft, speed);
    digitalWrite(m_SpeedPinRight, speed);

    m_State.Speed = speed;
}

void Engine::Configure(Direction direction, uint8_t speed) noexcept
{
    SetDirection(direction);
    SetSpeed(speed);
}
