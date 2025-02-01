#include "Hardware.hpp"
#include "pinconfig.hpp"

hd44780_I2Cexp Hardware::Lcd(0x20, I2Cexp_MCP23008, 7, 6, 5, 4, 3, 2, 1, HIGH);
Engine Hardware::Engine(PIN_ENG_SPD_LEFT, PIN_ENG_SPD_RIGHT, PIN_ENG_DIR_LEFT, PIN_ENG_DIR_RIGHT);
Sensor Hardware::SensorRight(PIN_SENSOR_RIGHT);
Sensor Hardware::SensorLeft(PIN_SENSOR_LEFT);
Sensor Hardware::SensorRear(PIN_SENSOR_REAR);
HeadSensor Hardware::HeadSensor(PIN_HEAD_SENSOR_SERVO, PIN_HEAD_SENSOR_TRIGGER, PIN_HEAD_SENSOR_ECHO);
Thermometer Hardware::Thermometer(PIN_THERMOMETER);
