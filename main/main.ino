#include <stdint.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#define SERIAL_RATE 9600

#define GET_OPCODE(x) ((x) & 127)
#define HAS_CONT_BIT(x) ((x) & (1 << 7))

#define ENG_SET_NO_DIR 1
#define ENG_SET_NO_SPEED 2

static hd44780_I2Cexp lcd(0x20, I2Cexp_MCP23008, 7, 6, 5, 4, 3, 2, 1, HIGH);

// 0b0 0000000
//   ^ ------- opcode
//   continuation bit
// instruction structure: opcode[byte], args count[byte], args[byte]...

enum Opcode : uint8_t
{
  COM_SET_SPEED = 1,
  COM_SET_DIR = 2,
  COM_STOP = 3,
  COM_GET_STATE = 4,
  COM_PRINT_LCD = 5,
  COM_CONFIGURE = 6,
};

enum Direction : uint8_t
{
  DIR_BACKWARD = 0,
  DIR_FORWARD = 255,
};

enum Engine
{
  ENGINE_LEFT = 0,
  ENGINE_RIGHT = 1,
};

struct SerialCommand
{
  Opcode opcode;
  uint8_t argsCount;
  uint8_t args[16];
};

struct EngineState
{
  uint8_t speed;
  Direction direction;
};

struct TankState
{
  union
  {
    struct
    {
      // Don't change order
      EngineState engineLeft;
      EngineState engineRight;
    };
    EngineState engines[2];
  } engineStates;
};

struct TankConfig
{
  uint8_t engineSelectors[2];
  uint8_t directionSelectors[2];
};

static TankState s_State;
static TankConfig s_Config;
static uint8_t s_LcdCursorPos = 0;

void setup()
{
  Serial.begin(SERIAL_RATE);
}

uint8_t readByteIfAvailable()
{
  // busy wait for serial available
  while (!Serial.available())
  {
  }

  return Serial.read();
}

void engineSetState(Engine engine, const EngineState *newState, uint8_t settings)
{
  if ((settings & ENG_SET_NO_DIR) != ENG_SET_NO_DIR)
  {
    const uint8_t output = s_Config.directionSelectors[engine];
    const uint8_t direction = newState->direction;
    digitalWrite(output, direction);

    s_State.engineStates.engines[engine].direction = direction;
  }

  if ((settings & ENG_SET_NO_SPEED) != ENG_SET_NO_SPEED)
  {
    const uint8_t output = s_Config.engineSelectors[engine];
    const uint8_t speed = newState->speed;
    analogWrite(output, speed);

    s_State.engineStates.engines[engine].speed = speed;
  }
}

void handleComSetSpeed(const SerialCommand &command)
{
  if (command->argsCount != 2)
  {
    Serial.write("[Status] COM_SET_SPEED Invalid command length.\n");
    return;
  }

  const uint8_t engine = command->args[0];
  EngineState state = {0};
  state.speed = command->args[1];
  engineSetState(engine, &state, ENG_SET_NO_DIR);

  Serial.write("[Status] Executed COM_SET_SPEED\n");
}

void handleComSetDir(const SerialCommand &command)
{
  if (command->argsCount != 2)
  {
    Serial.write("[Status] COM_SET_DIR Invalid command length.\n");
    return;
  }

  const uint8_t engine = command->args[1];
  EngineState state = {0};
  state.direction = (Direction)command->args[1];
  engineSetState(engine, &state, ENG_SET_NO_SPEED);

  Serial.write("[Status] Executed COM_SET_DIR.\n");
}

void handleComStop()
{
  analogWrite(9, 0);
  analogWrite(10, 0);
  Serial.write("[Status] Executed COM_STOP.\n");
}

void handleComGetState()
{
  Serial.write("[State] Eng. left (spd: ");
  Serial.write(s_State.engineStates.engineLeft.speed);
  Serial.write(", dir: ");
  Serial.write(s_State.engineStates.engineLeft.direction);
  Serial.write(") ");
  Serial.write("Eng. right (spd: ");
  Serial.write(s_State.engineStates.engineRight.speed);
  Serial.write(", dir: ");
  Serial.write(s_State.engineStates.engineRight.direction);
  Serial.write("\n");

  Serial.write("[Status] Executed COM_GET_STATE.\n");
}

void handleComPrintLcd(const SerialCommand *command)
{
  if (!HAS_CONT_BIT(command->opcode))
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    s_LcdCursorPos = 0;
  }

  for (uint8_t i = 0; i < command->argsCount; i++)
  {
    char character = (char)command->args[i];
    if (character == '\n')
    {
      s_LcdCursorPos++;
      lcd.setCursor(s_LcdCursorPos, 0);
    }
    else
    {
      lcd.print(character);
    }
  }

  Serial.write("[Status] Executed COM_PRINT_LCD.\n");
}

void handleComConfigure(const SerialCommand *command)
{
  if (command->argsCount < 6)
  {
    Serial.write("[Status] COM_CONFIGURE expected 7 arguments.\n");
    return;
  }

  // COM_CONFIGURE args:
  // 0 -> engineSelector[left]
  // 1 -> engineSelector[right]
  // 2 -> directionSelector[left]
  // 3 -> directionSelector[right]
  // 4 -> lcd width
  // 5 -> lcd height

  s_Config.engineSelectors[ENGINE_LEFT] = command->args[0];
  pinMode(command->args[0], OUTPUT);

  s_Config.engineSelectors[ENGINE_RIGHT] = command->args[1];
  pinMode(command->args[1], OUTPUT);

  s_Config.directionSelectors[ENGINE_LEFT] = command->args[2];
  pinMode(command->args[2], OUTPUT);

  s_Config.directionSelectors[ENGINE_RIGHT] = command->args[3];
  pinMode(command->args[3], OUTPUT);

  lcd.begin(command->args[4], command->args[5]);

  Serial.write("[Status] Executed COM_CONFIGURE.\n");
}

void loop()
{
  SerialCommand com = {0};
  com.opcode = readByteIfAvailable();
  com.argsCount = readByteIfAvailable();

  if (com.argsCount > 16)
  {
    Serial.write("[Status] Max arguments count exceeded. Max is 16.\n");
    return;
  }

  for (size_t i = 0; i < com.argsCount; i++)
    com.args[i] = readByteIfAvailable();

  switch (GET_OPCODE(com.opcode))
  {
  case COM_SET_SPEED:
    handleComSetSpeed(&com);
    break;
  case COM_SET_DIR:
    handleComSetDir(&com);
    break;
  case COM_STOP:
    handleComStop();
    break;
  case COM_GET_STATE:
    handleComGetState();
    break;
  case COM_PRINT_LCD:
    handleComPrintLcd(&com);
    break;
  case COM_CONFIGURE:
    handleComConfigure(&com);
    break;
  default:
    Serial.write("Invalid opcode: ");
    Serial.write(com.opcode);
    Serial.write("\n");
  }
}
