#pragma once
#include <stdint.h>

#define AUTORUN_TIME_INFINITE -1
#define AUTORUN_TIME_INSTANT 0

enum AutorunActionType : uint8_t
{
    AUTORUN_MOVE,
    AUTORUN_STOP,
    AUTORUN_CONFIGURE_LIGHTS,
    AUTORUN_CONFIGURE_BUZZER,
};

struct AutorunAction
{
    AutorunActionType Type;
    uint32_t ExecuteTime;
    uint8_t Args[16];
};

namespace Autorun
{
    void Setup(AutorunAction *sequence, uint8_t actionsCount, bool looping) noexcept;
    bool IsRunning() noexcept;
    uint8_t CurrentAction() noexcept;
    void Start() noexcept;
    void Stop() noexcept;
    void Restart() noexcept;
    void NextAction() noexcept;
    void Advance() noexcept;
}
