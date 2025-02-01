#include "Autorun.hpp"
#include "Hardware.hpp"
#include <stdarg.h>

static uint32_t s_LastTimestamp;
static uint32_t s_ActionRemainingTime;
static uint8_t s_CurrentAction;
static uint8_t s_ActionsCount;
static bool s_IsLooping;
static bool s_IsRunning;
static AutorunAction *s_Actions;

void Autorun::Stop() noexcept
{
    s_IsRunning = false;
}

void Autorun::Start() noexcept
{
    s_IsRunning = true;
    s_LastTimestamp = millis();
    s_CurrentAction = -1;
    NextAction();
}

void Autorun::Restart() noexcept
{
    s_CurrentAction = 0;
    Start();
}

void Autorun::NextAction() noexcept
{
    if (!s_IsRunning)
        return;

    s_CurrentAction++;
    if (s_CurrentAction >= s_ActionsCount)
    {
        if (s_IsLooping)
        {
            s_CurrentAction = 0;
        }
        else
        {
            Stop();
            return;
        }
    }

    s_ActionRemainingTime = s_Actions[s_CurrentAction].ExecuteTime;
}

void Autorun::Setup(AutorunAction *sequence, uint8_t actionsCount, bool looping) noexcept
{
    s_Actions = sequence;
    s_ActionsCount = actionsCount;
    s_IsLooping = looping;
}

void Autorun::Advance() noexcept
{
    if (!s_IsRunning)
        return;

    const uint32_t timestamp = millis();
    const uint32_t timeDelta = timestamp - s_LastTimestamp;
    s_LastTimestamp = timestamp;

    const AutorunAction *currentAction = &s_Actions[s_CurrentAction];

    switch (currentAction->Type)
    {
    case AUTORUN_MOVE:
        Hardware::Engine.Configure((Direction)currentAction->Args[0], currentAction->Args[1]);
        break;
    case AUTORUN_STOP:
        Hardware::Engine.Stop();
        break;
    };

    if (s_ActionRemainingTime == -1)
        return;

    if (s_ActionRemainingTime <= 0)
    {
        NextAction();
    }
    else
    {
        s_ActionRemainingTime -= timeDelta;
    }
}

bool Autorun::IsRunning() noexcept
{
    return s_IsRunning;
}

uint8_t Autorun::CurrentAction() noexcept
{
    return s_CurrentAction;
}
