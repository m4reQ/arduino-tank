#pragma once
#include <stdint.h>

enum Direction : uint8_t
{
    DIR_BACKWARD = 0b00,
    DIR_FORWARD = 0b11,
    DIR_LEFT = 0b01,
    DIR_RIGHT = 0b10,
    DIR_CURRENT = 0b11111111,
};

#pragma pack(push, 1)
struct EngineState
{
    uint8_t Speed;
    Direction Direction;
};
#pragma pop

class Engine
{
public:
    constexpr Engine(uint8_t speedPinLeft, uint8_t speedPinRight, uint8_t directionPinLeft, uint8_t directionPinRight) noexcept
        : m_SpeedPinLeft(speedPinLeft),
          m_SpeedPinRight(speedPinRight),
          m_DirectionPinLeft(directionPinLeft),
          m_DirectionPinRight(directionPinRight) {}

    void Setup() noexcept;
    void Stop() noexcept;
    void SetDirection(Direction direction) noexcept;
    void SetSpeed(uint8_t speed) noexcept;
    void Configure(Direction direction, uint8_t speed) noexcept;
    constexpr Direction GetDirection() const noexcept { return m_State.Direction; }
    constexpr uint8_t GetSpeed() const noexcept { return m_State.Speed; }
    constexpr const EngineState *GetState() const noexcept { return &m_State; }

private:
    uint32_t m_SpeedPinLeft;
    uint32_t m_SpeedPinRight;
    uint32_t m_DirectionPinLeft;
    uint32_t m_DirectionPinRight;
    EngineState m_State{};
};
