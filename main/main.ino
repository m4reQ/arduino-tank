#include <Wire.h>
#include <Servo.h>
#include <HCSR04.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#define _countof(x) (sizeof(x) / sizeof(x[0]))

#define HAS_CONT_BIT(x) (x & 0b10000000)
#define GET_OPCODE(x) (x & 0b01111111)

#define PIN_ENG_SPD_LEFT 10
#define PIN_ENG_SPD_RIGHT 9
#define PIN_ENG_DIR_LEFT 3
#define PIN_ENG_DIR_RIGHT 2
#define PIN_SENSOR_LEFT 15
#define PIN_SENSOR_RIGHT 14
#define PIN_SENSOR_REAR 36
#define PIN_HEAD_SENSOR_SERVO 17
#define PIN_HEAD_SENSOR_TRIGGER 7
#define PIN_HEAD_SENSOR_ECHO 8
#define LCD_WIDTH 16
#define LCD_HEIGHT 2

#define SERIAL_RATE 9600
#define BT_SERIAL_RATE 115200

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
    ENGINE_LEFT = 0,
    ENGINE_RIGHT = 1,
} Engine;

typedef enum
{
    SENSOR_LEFT = 0,
    SENSOR_RIGHT = 1,
    SENSOR_BACK = 2,
} Sensor;

typedef enum
{
    STATUS_SUCCESS = 0,
    STATUS_INV_OPCODE = 1,
    STATUS_INV_ARGS_COUNT = 2,
    STATUS_INV_STATE = 3,
} Status;

typedef enum
{
    OP_MOVE = 1,
    OP_STOP = 2,
    OP_PRINT_LCD = 3,
    OP_GET_SENSOR_STATE = 4,
    OP_MOVE_HEAD_SENSOR = 5,
} Opcode;

#pragma push(pack, 1)
typedef struct
{
    uint8_t opcode; // use `Opcode` enum
    uint64_t id;
    uint8_t status; // use `Status` enum
    uint8_t dataLength;
} Result;

typedef struct
{
    uint8_t opcode; // use `Opcode` enum
    uint64_t id;
    uint8_t argsCount;
    uint8_t args[16];
} Command;

typedef struct
{
    uint8_t opcode;
    uint64_t id;
    uint8_t argsCount;
} CommandHeader;

typedef struct
{
    Direction currentDir;
    uint8_t currentSpeed;
} EngineState;

typedef struct
{
    uint8_t sensorLeft;
    uint8_t sensorRight;
    uint8_t sensorRear;
    uint8_t headDistanceMm;
} SensorState;
#pragma pop

static EngineState s_EngineState;
static uint8_t s_LcdCursorY = 0;
static hd44780_I2Cexp s_Lcd(0x20, I2Cexp_MCP23008, 7, 6, 5, 4, 3, 2, 1, HIGH);
static Servo s_Servo;

static bool tryAcceptCommand(Command *command)
{
    // value returned by this function doesn't indicate success or failure, rather request
    // that main loop should continue or not

    if (Serial.available() < sizeof(CommandHeader))
        return false;

    Serial.readBytes((uint8_t *)command, sizeof(CommandHeader));

    // assume instruction was completely sent and all arguments will be valid
    size_t argsSize = command->argsCount * sizeof(uint8_t);
    if (argsSize > 0)
        Serial.readBytes((uint8_t *)command->args, argsSize);

    return true;
}

static void sendResult(
    const Command *command,
    Status status,
    const void *data,
    uint8_t dataLength)
{
    const Result result = {
        .opcode = command->opcode,
        .id = command->id,
        .status = (uint8_t)status,
        .dataLength = dataLength};

    Serial.write((const uint8_t *)&result, sizeof(Result));

    if (dataLength > 0 && data != NULL)
        Serial.write((const uint8_t *)data, dataLength);
}

static bool checkArgsCount(const Command *command, uint8_t expected)
{
    if (command->argsCount == expected)
        return true;

    sendResult(command, STATUS_INV_ARGS_COUNT, &expected, sizeof(uint8_t));
    return false;
}

static void configureEngine(Direction direction, uint8_t speed)
{
    if (direction == DIR_CURRENT)
        direction = s_EngineState.currentDir;

    uint8_t dirLeft = (direction & 0b10) >> 1;
    uint8_t dirRight = direction & 0b01;

    digitalWrite(PIN_ENG_DIR_LEFT, dirLeft);
    analogWrite(PIN_ENG_SPD_LEFT, speed);

    digitalWrite(PIN_ENG_DIR_RIGHT, dirRight);
    analogWrite(PIN_ENG_SPD_RIGHT, speed);

    s_EngineState.currentDir = direction;
    s_EngineState.currentSpeed = speed;
}

static void printLcd(const uint8_t *characters, uint8_t count, bool clear)
{
    if (clear)
    {
        s_Lcd.clear();
        s_Lcd.setCursor(0, 0);

        s_LcdCursorY = 0;
    }

    for (uint8_t i = 0; i < count; i++)
    {
        char character = (char)characters[i];
        if (character == '\n')
        {
            s_LcdCursorY++;
            s_Lcd.setCursor(0, s_LcdCursorY);
        }
        else
        {
            s_Lcd.write(characters[i]);
        }
    }
}

void setup()
{
    // configure serial for bluetooth
    Serial.begin(BT_SERIAL_RATE);

    // configure engines (speed and direction)
    pinMode(PIN_ENG_SPD_LEFT, OUTPUT);
    pinMode(PIN_ENG_SPD_RIGHT, OUTPUT);
    pinMode(PIN_ENG_DIR_LEFT, OUTPUT);
    pinMode(PIN_ENG_DIR_RIGHT, OUTPUT);

    // configure and clear screen
    s_Lcd.begin(LCD_WIDTH, LCD_HEIGHT);
    s_Lcd.clear();

    // configure sensor pins
    pinMode(PIN_SENSOR_LEFT, INPUT);
    pinMode(PIN_SENSOR_RIGHT, INPUT);
    pinMode(PIN_SENSOR_REAR, INPUT);

    // configure head sensor
    s_Servo.attach(PIN_HEAD_SENSOR_SERVO);
    s_Servo.write(90);

    uint8_t sensorEchoPin = PIN_HEAD_SENSOR_ECHO;
    HCSR04.begin(PIN_HEAD_SENSOR_TRIGGER, &sensorEchoPin, 1);
}

void loop()
{
    Command com;
    if (!tryAcceptCommand(&com))
        return;

    switch (GET_OPCODE(com.opcode))
    {
    case OP_MOVE:
    {
        if (!checkArgsCount(&com, 2))
            return;

        configureEngine((Direction)com.args[0], com.args[1]);
        break;
    }
    case OP_STOP:
    {
        configureEngine(DIR_FORWARD, 0);
        break;
    }
    case OP_PRINT_LCD:
    {
        printLcd(com.args, com.argsCount, HAS_CONT_BIT(com.opcode));
        break;
    }
    case OP_GET_SENSOR_STATE:
    {
        SensorState sensorState = {
            .sensorLeft = (uint8_t)digitalRead(PIN_SENSOR_LEFT),
            .sensorRight = (uint8_t)digitalRead(PIN_SENSOR_RIGHT),
            .sensorRear = (uint8_t)digitalRead(PIN_SENSOR_REAR),
            .headDistanceMm = (uint8_t)HCSR04.measureDistanceMm(),
        };

        sendResult(&com, STATUS_SUCCESS, &sensorState, sizeof(SensorState));
        return;
    }
    case OP_MOVE_HEAD_SENSOR:
    {
        if (!checkArgsCount(&com, 1))
            return;

        s_Servo.write(com.args[0]);
        break;
    }
    default:
    {
        sendResult(&com, STATUS_INV_OPCODE, NULL, 0);
        return;
    }
    }

    sendResult(&com, STATUS_SUCCESS, NULL, 0);
    // otherwise result already sent by `checkArgsCount` or specific handlers
}
