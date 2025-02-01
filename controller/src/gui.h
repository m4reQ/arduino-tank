#pragma once
#include "state.h"
#include <stdint.h>

void GUI_Draw(const ApplicationState *state);
void GUI_Initialize(uint32_t width, uint32_t height);
void GUI_Shutdown(void);
