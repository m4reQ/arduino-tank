#pragma once
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include "Engine.hpp"
#include "Sensor.hpp"
#include "HeadSensor.hpp"
#include "Thermometer.hpp"

namespace Hardware
{
    extern hd44780_I2Cexp Lcd;
    extern Engine Engine;
    extern Sensor SensorRight;
    extern Sensor SensorLeft;
    extern Sensor SensorRear;
    extern HeadSensor HeadSensor;
    extern Thermometer Thermometer;
}
