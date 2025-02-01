#include <stdlib.h>
#include <avr/wdt.h>
#include <Arduino.h>
#include "Hardware.hpp"
#include "Autorun.hpp"
#include "communication.hpp"
#include "pinconfig.hpp"

#define countof(x) (sizeof(x) / sizeof(x[0]))

#define BREAK_ON_INVALID_STATE true
#define SET_STATUS_OK() SetStatus(STATUS_OK)
#define SET_STATUS_AND_RETURN(status, ...) \
    do                                     \
    {                                      \
        SetStatus(status);                 \
        return __VA_ARGS__;                \
    } while (0)
#define CHECK_STATE_VALID(...)                                 \
    do                                                         \
    {                                                          \
        if (BREAK_ON_INVALID_STATE && (s_Status != STATUS_OK)) \
            return __VA_ARGS__;                                \
    } while (0)
#define CHECK_ARGS_COUNT(command, expected, ...)                         \
    do                                                                   \
    {                                                                    \
        if (command->header.argsCount != expected)                       \
        {                                                                \
            SET_STATUS_AND_RETURN(STATUS_INV_ARGS_COUNT, ##__VA_ARGS__); \
        }                                                                \
    } while (0)

#define SDA 20
#define SCL 21

#define LCD_WIDTH 16
#define LCD_HEIGHT 2

#define COM_SERIAL_RATE 9600
#define BT_SERIAL_RATE 115200

#define MIN_HEAD_ANGLE 0
#define MAX_HEAD_ANGLE 180

#pragma pack(push, 1)
typedef struct
{
    float HeadDistanceMm;
    float Temperature;
    bool CollisionLeft;
    bool CollisionRight;
    bool CollisionRear;
} SensorState;
#pragma pack(pop)

typedef void (*CommandHandlerFn)(const Command *);

static CommandHandlerFn s_CommandHandlers[_OP_ENUM_MAX];
static Status s_Status;

static bool s_CurrentHeadMoveDir;
static bool s_ScanModeActive;

static AutorunAction s_AutoMoveSequence[] = {
    {AutorunActionType::AUTORUN_MOVE, AUTORUN_TIME_INFINITE, {DIR_FORWARD, 255}},
    {AutorunActionType::AUTORUN_MOVE, 500, {DIR_LEFT, 255}},
    {AutorunActionType::AUTORUN_MOVE, 650, {DIR_FORWARD, 255}},
    {AutorunActionType::AUTORUN_MOVE, 500, {DIR_RIGHT, 255}},
    {AutorunActionType::AUTORUN_MOVE, 650, {DIR_FORWARD, 255}},
    {AutorunActionType::AUTORUN_MOVE, 500, {DIR_RIGHT, 255}},
    {AutorunActionType::AUTORUN_MOVE, 650, {DIR_FORWARD, 255}},
    {AutorunActionType::AUTORUN_MOVE, 500, {DIR_LEFT, 255}},
    {AutorunActionType::AUTORUN_STOP, AUTORUN_TIME_INSTANT, {}},
};

static bool TryAcceptCommand(Command *command)
{
    if (Serial.available() < sizeof(CommandHeader))
        return false;

    Serial.readBytes((uint8_t *)&command->header, sizeof(CommandHeader));
    size_t argsSize = command->header.argsCount * sizeof(uint8_t);
    if (argsSize > 0)
        Serial.readBytes((uint8_t *)command->args, argsSize);

    return true;
}

static void SendResult(const Result *result)
{
    Serial.write((const uint8_t *)&result->header, sizeof(ResultHeader));
    if (result->header.dataSize > 0)
        Serial.write((const uint8_t *)result->data, result->header.dataSize);
}

static void WriteStatusString(Status status)
{
    switch (status)
    {
    case STATUS_OK:
        Hardware::Lcd.setCursor(8, 0);
        Hardware::Lcd.write("OK");
        break;
    case STATUS_INV_OPCODE:
        Hardware::Lcd.setCursor(8, 0);
        Hardware::Lcd.write("INV_OP");
        break;
    case STATUS_INV_ARGS_COUNT:
        Hardware::Lcd.setCursor(8, 0);
        Hardware::Lcd.write("INV_ARG_CNT");
        break;
    case STATUS_INV_STATE:
        Hardware::Lcd.setCursor(8, 0);
        Hardware::Lcd.write("INV_STATE");
        break;
    case STATUS_NO_HANDLER:
        Hardware::Lcd.setCursor(8, 0);
        Hardware::Lcd.write("NO_HNDLR");
        break;
    default:
        Hardware::Lcd.setCursor(8, 0);
        Hardware::Lcd.write("UNK");
    }
}

static void SetStatus(Status status)
{
    s_Status = status;

    Hardware::Lcd.setCursor(0, 0);
    Hardware::Lcd.write("Status:");
    WriteStatusString(status);
}

static CommandHandlerFn GetCommandHandler(const Command *command)
{
    if (command->header.opcode >= _OP_ENUM_MAX)
        SET_STATUS_AND_RETURN(STATUS_INV_OPCODE, NULL);

    CommandHandlerFn handler = s_CommandHandlers[command->header.opcode];
    if (handler == NULL)
        SET_STATUS_AND_RETURN(STATUS_NO_HANDLER, NULL);

    SET_STATUS_AND_RETURN(STATUS_OK, handler);
}

static void MoveHandler(const Command *command)
{
    CHECK_STATE_VALID();
    CHECK_ARGS_COUNT(command, 2);

    Hardware::Engine.Configure((Direction)command->args[0], command->args[1]);

    SET_STATUS_OK();
}

static void StopHandler(const Command *command)
{
    CHECK_STATE_VALID();

    Hardware::Engine.Stop();

    SET_STATUS_OK();
}

static void PrintLCDHandler(const Command *command)
{
    CHECK_STATE_VALID();

    // clear user row
    for (int i = 0; i < LCD_WIDTH; i++)
    {
        Hardware::Lcd.setCursor(i, 1);
        Hardware::Lcd.write(" ");
    }

    Hardware::Lcd.setCursor(0, 1);
    Hardware::Lcd.write(command->args, command->header.argsCount);

    SET_STATUS_OK();
}

static void GetSensorStateHandler(const Command *command)
{
    (void)command;

    CHECK_STATE_VALID();

    const float temperature = Hardware::Thermometer.GetTemperatureCelsius();
    const SensorState sensorState = {
        Hardware::HeadSensor.GetDistance(temperature),
        temperature,
        Hardware::SensorLeft.HasCollision(),
        Hardware::SensorRight.HasCollision(),
        Hardware::SensorRear.HasCollision(),
    };

    const Result result = MakeResultData(STATUS_OK, command, &sensorState, sizeof(SensorState));
    SendResult(&result);

    SET_STATUS_OK();
}

static void MoveHeadSensorHandler(const Command *command)
{
    // CHECK_STATE_VALID();
    CHECK_ARGS_COUNT(command, 1);

    Hardware::HeadSensor.SetRotation(command->args[0]);

    SET_STATUS_OK();
}

static void ConfigureLightHandler(const Command *command)
{
    CHECK_STATE_VALID();
    CHECK_ARGS_COUNT(command, 2);

    const uint8_t lightPin = command->args[0];
    const uint8_t isEnabled = command->args[1];
    digitalWrite(lightPin, isEnabled);

    SET_STATUS_OK();
}

static void ConfigureBuzzerHandler(const Command *command)
{
    CHECK_STATE_VALID();
    CHECK_ARGS_COUNT(command, 1);

    digitalWrite(PIN_BUZZER, command->args[0]);

    SET_STATUS_OK();
}

static void ResetHandler(const Command *command)
{
    (void)command;

    // we do not check if the state is valid because reset command
    // is used precisely when it is not

    // purposefully trigger watchdog timeout which forces device to restart
    wdt_disable();
    wdt_enable(WDTO_15MS);

    // TODO Make sure that this is not optimized away
    while (true)
    {
    }
}

static void AutoRunHandler(const Command *command)
{
    // CHECK_STATE_VALID();
    CHECK_ARGS_COUNT(command, 1);

    const bool isActive = (bool)command->args[0];

    if (isActive)
    {
        Autorun::Setup(s_AutoMoveSequence, 8, false);
        Autorun::Start();
    }
    else
    {
        Autorun::Stop();
        Autorun::Restart();
    }

    SET_STATUS_OK();
}

static void SetupCommandHandlersTable()
{
    // just a safety measure to prevent garbage somehow getting into handler pointers
    // should be optimized out if unnecessary
    memset(s_CommandHandlers, 0, sizeof(s_CommandHandlers));

    s_CommandHandlers[OP_MOVE] = MoveHandler;
    s_CommandHandlers[OP_STOP] = StopHandler;
    s_CommandHandlers[OP_PRINT_LCD] = PrintLCDHandler;
    s_CommandHandlers[OP_GET_SENSOR_STATE] = GetSensorStateHandler;
    s_CommandHandlers[OP_MOVE_HEAD_SENSOR] = MoveHeadSensorHandler;
    s_CommandHandlers[OP_CONFIGURE_LIGHT] = ConfigureLightHandler;
    s_CommandHandlers[OP_CONFIGURE_BUZZER] = ConfigureBuzzerHandler;
    s_CommandHandlers[OP_RESET] = ResetHandler;
    s_CommandHandlers[OP_AUTORUN] = AutoRunHandler;
}

static void SetupLights()
{
    pinMode(4, OUTPUT);
    pinMode(5, OUTPUT);
    pinMode(6, OUTPUT);
    pinMode(12, OUTPUT);
}

static void SetupBuzzer()
{
    pinMode(11, OUTPUT);
}

void setup()
{
    SetupCommandHandlersTable();

    Serial.begin(BT_SERIAL_RATE);
    Hardware::Lcd.begin(LCD_WIDTH, LCD_HEIGHT);
    Hardware::HeadSensor.Setup();
    Hardware::Engine.Setup();
    Hardware::SensorLeft.Setup();
    Hardware::SensorRight.Setup();
    Hardware::SensorRear.Setup();
    Hardware::Thermometer.Setup();

    SetupLights();
    SetupBuzzer();

    SET_STATUS_OK();
}

void loop()
{
    Command command;
    if (TryAcceptCommand(&command))
    {
        CommandHandlerFn handler = GetCommandHandler(&command);
        if (handler != NULL)
            handler(&command);
    }

    Autorun::Advance();
    if (Autorun::CurrentAction() == 0 && (Hardware::SensorLeft.HasCollision() || Hardware::SensorRight.HasCollision()))
        Autorun::NextAction();

    // prevent collision during manual control
    if (!Autorun::IsRunning())
    {
        if (Hardware::Engine.GetDirection() == DIR_FORWARD && (Hardware::SensorLeft.HasCollision() || Hardware::SensorRight.HasCollision()))
        {
            Hardware::Engine.Stop();
        }

        if (Hardware::Engine.GetDirection() == DIR_BACKWARD && Hardware::SensorRear.HasCollision())
        {
            Hardware::Engine.Stop();
        }
    }
}
