#pragma once

#include <Arduino.h>

class Led
{
public:
    static void begin();
    static void loop();

    static void setColor(uint8_t r, uint8_t g, uint8_t b);
    static void off();

    static void test();
};