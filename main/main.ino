#include <stdint.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#define SERIAL_RATE 9600

// pins
#define ENG_LEFT      9
#define ENG_RIGHT     10
#define ENG_LEFT_DIR 3
#define ENG_RIGHT_DIR  2

// commands
#define COM_SET_SPEED 1
#define COM_SET_DIR   2

static hd44780_I2Cexp lcd(0x20, I2Cexp_MCP23008, 7, 6, 5, 4, 3, 2, 1, HIGH);

// instruction structure: opcode[byte], args count[byte], args[byte]...
struct SerialCommand
{
    uint8_t opcode;
    uint8_t argsCount;
    uint8_t args[16];
};

void setup()
{
  pinMode(ENG_LEFT, OUTPUT);
  pinMode(ENG_RIGHT, OUTPUT);
  pinMode(ENG_LEFT_DIR, OUTPUT);
  pinMode(ENG_RIGHT_DIR, OUTPUT);
  
  Serial.begin(SERIAL_RATE);

  lcd.begin(16, 2);
}

uint8_t readByteIfAvailable()
{
  // busy wait for serial available
  while (!Serial.available()) { }
  
  return Serial.read();
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
    
    switch (com.opcode)
    {
    case COM_SET_SPEED:
    {
      if (com.argsCount != 2)
      {
        Serial.write("[Status] COM_SET_SPEED Invalid command length.\n");
        break;
      }

      const uint8_t engineNum = com.args[0];
      const uint8_t engineSpeed = com.args[1];

      analogWrite(engineNum, engineSpeed);

      Serial.write("[Status] Executed COM_SET_SPEED, args: ");
      Serial.write(engineNum);
      Serial.write(" ");
      Serial.write(engineSpeed);
      Serial.write("\n");
      
      break;
    }
    case COM_SET_DIR:
    {
      if (com.argsCount != 2)
      {
        Serial.write("[Status] COM_SET_DIR Invalid command length.\n");
        break;
      }

      const uint8_t dirPortNum = com.args[0];
      const uint8_t dir = com.args[1];

      digitalWrite(dirPortNum, dir);

      Serial.write("[Status] Executed COM_SET_DIR, args: ");
      Serial.write(dirPortNum);
      Serial.write(" ");
      Serial.write(dir);
      Serial.write("\n");
      
      break;
    }
    default:
      Serial.write("Invalid opcode: ");
      Serial.write(com.opcode);
      Serial.write("\n");
    }

    lcd.clear();
    lcd.setCursor(0, 0);

    lcd.print("E1 spd: %d, dir: (F|B)");
    lcd.print(
}
