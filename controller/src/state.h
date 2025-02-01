#pragma once
#include <stdbool.h>

typedef struct
{
    float frametime;
    bool autorunActive;
    // movement
    bool movingForward;
    bool movingBackward;
    bool movingLeft;
    bool movingRight;
    // sensors
    bool sensorLeftOn;
    bool sensorRightOn;
    bool sensorRearOn;
    float headSensorDist;
    float headSensorHeading;
    bool headSensorHeadingChanged;
    // lights
    bool frontLightEnabled;
    bool rearLightEnabled;
} ApplicationState;
