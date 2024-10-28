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
  union {
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
  s_Config.engineSelectors[ENGINE_LEFT] = 9;
  s_Config.engineSelectors[ENGINE_RIGHT] = 10;
  s_Config.directionSelectors[ENGINE_LEFT] = 3;
  s_Config.directionSelectors[ENGINE_RIGHT] = 2;

  pinMode(9, OUTPUT);
  pinMode(10, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(2, OUTPUT);
  
  Serial.begin(SERIAL_RATE);

  lcd.begin(16, 2);
}

uint8_t readByteIfAvailable()
{
  // busy wait for serial available
  while (!Serial.available()) { }
  
  return Serial.read();
}

void engineSetState(Engine engine, const EngineState* newState, uint8_t settings)
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
    {
      if (com.argsCount != 2)
      {
        Serial.write("[Status] COM_SET_SPEED Invalid command length.\n");
        break;
      }

      const uint8_t engine = com.args[0];
      EngineState state = {0};
      state.speed = com.args[1];
      engineSetState(engine, &state, ENG_SET_NO_DIR);

      Serial.write("[Status] Executed COM_SET_SPEED\n");
      
      break;
    }
    case COM_SET_DIR:
    {
      if (com.argsCount != 2)
      {
        Serial.write("[Status] COM_SET_DIR Invalid command length.\n");
        break;
      }

      const uint8_t engine = com.args[1];
      EngineState state = {0};
      state.direction = (Direction)com.args[1];
      engineSetState(engine, &state, ENG_SET_NO_SPEED);

      Serial.write("[Status] Executed COM_SET_DIR.\n");
      
      break;
    }
    case COM_STOP:
    {
      analogWrite(9, 0);
      analogWrite(10, 0);
      Serial.write("[Status] Executed COM_STOP.\n");
      break;
    }
    case COM_GET_STATE:
    {
      Serial.write("[Status] Executed COM_GET_STATE.\n");

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

      break;
    }
    case COM_PRINT_LCD:
    {
      if (!HAS_CONT_BIT(com.opcode))
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        s_LcdCursorPos = 0;
      }
      
      for (uint8_t i = 0; i < com.argsCount; i++)
      {
        char character = (char)com.args[i];
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

      break;
    }
    default:
      Serial.write("Invalid opcode: ");
      Serial.write(com.opcode);
      Serial.write("\n");
    }
}
