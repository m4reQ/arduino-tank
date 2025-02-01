#include <stdio.h>
#include <stdbool.h>
#include <raylib.h>
#include <raymath.h>
#include "byteswap.h"
#include "controller.h"
#include "gui.h"

#define BT_ADDR_IE30 0x002500002CB2
#define BT_ADDR_IE31 0x002500002F1E
#define BT_ADDR_IE34 0x002500002475
#define BT_ADDR_IE38 0x00250000326A

#define HEAD_SENSOR_SPEED 60.0f
#define HEAD_SENSOR_DIR_MIN -90.0f
#define HEAD_SENSOR_DIR_MAX 90.0f

#define WIN_WIDTH 1080
#define WIN_HEIGHT 720

typedef enum
{
    DIR_BACKWARD = 0b00,
    DIR_FORWARD = 0b11,
    DIR_LEFT = 0b01,
    DIR_RIGHT = 0b10,
    DIR_CURRENT = 0b11111111,
} Direction;

typedef enum
{
    LIGHT_FRONT = 6,
    LIGHT_REAR = 5,
    LIGHT_LEFT = 4,
    LIGHT_RIGHT = 12,
} Light;

#pragma pack(push, 1)
typedef struct
{
    float headDistanceMm;
    float temperature;
    uint8_t sensorLeft;
    uint8_t sensorRight;
    uint8_t sensorRear;
} SensorState;
#pragma pack(pop)

static ApplicationState s_State;

static void SendMoveCommand(Direction dir)
{
    Controller_SendCommand(OP_MOVE, 2, (uint8_t)dir, 255);
}

static void SendLightsConfigureCommand(Light light, bool isEnabled)
{
    Controller_SendCommand(OP_CONFIGURE_LIGHT, 2, (uint8_t)light, isEnabled ? 255 : 0);
}

static void SendStopCommand(void)
{
    Controller_SendCommand(OP_STOP, 0);
}

static void UpdateKeyState(float frametime)
{
    if (IsKeyPressed(KEY_SPACE))
        Controller_SendCommand(OP_CONFIGURE_BUZZER, 1, 255);

    if (IsKeyReleased(KEY_SPACE))
        Controller_SendCommand(OP_CONFIGURE_BUZZER, 1, 0);

    if (IsKeyPressed(KEY_W))
    {
        s_State.movingForward = true;
        SendMoveCommand(DIR_FORWARD);
    }
    else if (IsKeyPressed(KEY_S))
    {
        s_State.movingBackward = true;
        SendMoveCommand(DIR_BACKWARD);
    }
    else if (IsKeyPressed(KEY_A))
    {
        s_State.movingLeft = true;
        SendMoveCommand(DIR_LEFT);
    }
    else if (IsKeyPressed(KEY_D))
    {
        s_State.movingRight = true;
        SendMoveCommand(DIR_RIGHT);
    }

    if (IsKeyReleased(KEY_W))
    {
        s_State.movingForward = false;
        SendStopCommand();
    }
    else if (IsKeyReleased(KEY_S))
    {
        s_State.movingBackward = false;
        SendStopCommand();
    }
    else if (IsKeyReleased(KEY_A))
    {
        s_State.movingLeft = false;
        SendStopCommand();
    }
    else if (IsKeyReleased(KEY_D))
    {
        s_State.movingRight = false;
        SendStopCommand();
    }

    // lights control
    if (IsKeyPressed(KEY_O))
    {
        s_State.frontLightEnabled = !s_State.frontLightEnabled;
        SendLightsConfigureCommand(LIGHT_FRONT, s_State.frontLightEnabled);
    }

    if (IsKeyPressed(KEY_L))
    {
        s_State.rearLightEnabled = !s_State.rearLightEnabled;
        SendLightsConfigureCommand(LIGHT_REAR, s_State.rearLightEnabled);
    }

    // head sensor movement
    if (IsKeyDown(KEY_E))
    {
        s_State.headSensorHeading = Clamp(
            s_State.headSensorHeading - HEAD_SENSOR_SPEED * frametime,
            HEAD_SENSOR_DIR_MIN,
            HEAD_SENSOR_DIR_MAX);
        s_State.headSensorHeadingChanged = true;
    }

    if (IsKeyDown(KEY_Q))
    {
        s_State.headSensorHeading = Clamp(
            s_State.headSensorHeading + HEAD_SENSOR_SPEED * frametime,
            HEAD_SENSOR_DIR_MIN,
            HEAD_SENSOR_DIR_MAX);
        s_State.headSensorHeadingChanged = true;
    }

    if (IsKeyPressed(KEY_P))
        Controller_SendCommand(OP_GET_SENSOR_STATE, 0);

    if (IsKeyPressed(KEY_R))
    {
        Controller_SendCommand(OP_RESET, 0);
        Controller_DisconnectDevice();

        WaitTime(1.5);

        while (!Controller_ConnectDevice(BT_ADDR_IE30, 1))
            fprintf(stderr, "Failed to connect to device after reset. Retrying...");
    }

    if (IsKeyPressed(KEY_I))
    {
        s_State.autorunActive = !s_State.autorunActive;
        Controller_SendCommand(OP_AUTORUN, 1, (uint8_t)s_State.autorunActive);
    }
}

static const char *StatusToString(uint8_t status)
{
    switch (status)
    {
    case STATUS_SUCCESS:
        return "SUCCESS";
    case STATUS_INV_OPCODE:
        return "INV_OPCODE";
    case STATUS_INV_ARGS_COUNT:
        return "INV_ARGS_COUNT";
    case STATUS_INV_STATE:
        return "INV_STATE";
    case STATUS_OPERATING:
        return "OPERATING";
    default:
        return "UNKNOWN";
    }
}

static const char *OpcodeToString(uint8_t opcode)
{
    switch (opcode)
    {
    case OP_MOVE:
        return "MOVE";
    case OP_STOP:
        return "STOP";
    case OP_PRINT_LCD:
        return "PRINT_LCD";
    case OP_MOVE_HEAD_SENSOR:
        return "MOVE_HEAD_SENSOR";
    case OP_CONFIGURE_LIGHT:
        return "CONFIGURE_LIGHT";
    case OP_CONFIGURE_BUZZER:
        return "CONFIGURE_BUZZER";
    default:
        return "UNKNOWN";
    }
}

static void OnResult(const Result *result)
{
    if (result->header.servicedOpcode == OP_GET_SENSOR_STATE)
    {
        SensorState *sensorState = result->data;
        s_State.sensorLeftOn = sensorState->sensorLeft;
        s_State.sensorRightOn = sensorState->sensorRight;
        s_State.sensorRearOn = sensorState->sensorRear;
        s_State.headSensorDist = sensorState->headDistanceMm;

        printf(
            "[SENSORS] %f degrees C, %d, %d, %d, %lf.\n",
            sensorState->temperature,
            sensorState->sensorLeft,
            sensorState->sensorRight,
            sensorState->sensorRear,
            sensorState->headDistanceMm);
    }

    const ResultHeader *header = &result->header;
    printf(
        "[RESULT] Status: %s, Opcode: %s, Command ID: %llu.\n",
        StatusToString(header->status),
        OpcodeToString(header->servicedOpcode),
        header->servicedID);
}

int main()
{
    if (!Controller_Initialize())
        return 1;

    if (!Controller_ConnectDevice(BT_ADDR_IE31, 1))
        return 1;

    GUI_Initialize(WIN_WIDTH, WIN_HEIGHT);

    while (!WindowShouldClose())
    {
        s_State.frametime = GetFrameTime();

        Result result = {0};
        while (Controller_GetNextResult(&result))
            OnResult(&result);

        UpdateKeyState(s_State.frametime);

        if (s_State.headSensorHeadingChanged)
        {
            Controller_SendCommand(OP_MOVE_HEAD_SENSOR, 1, (uint8_t)(s_State.headSensorHeading + 90));
            s_State.headSensorHeadingChanged = false;
        }

        GUI_Draw(&s_State);
    }

    GUI_Shutdown();
    Controller_Shutdown();

    return 0;
}
