#ifndef CONTROLLER_H
#define CONTROLLER_H
#include <stdint.h>

enum CONTROLLER
{
    A = (1 << 0), // A Button
    B = (1 << 1), // B Button
    Select = (1 << 2), // Select Button
    Start = (1 << 3), // Start Button
    Up = (1 << 4), // Up Button
    Down = (1 << 5), // Down Button
    Left = (1 << 6), // Left Button
    Right = (1 << 7)  // Right Button
};

static uint8_t (*_controllerRead)() = nullptr;

void initController();
uint8_t controllerRead();
bool isDownPressed(CONTROLLER button);

static uint8_t gpioRead();
static uint8_t NESControllerRead();
static uint8_t SNESControllerRead();
static uint8_t PSXControllerRead();
static uint8_t PSXTransferByte(uint8_t byte);

#endif