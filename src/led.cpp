#include "led.h"
#include "config.h"
#include "settings.h"

const int RED_CHANNEL = 0;
const int GREEN_CHANNEL = 1;
const int BLUE_CHANNEL = 2;

void Led::begin()
{
    pinMode(Settings::activePins().statusLedPin, OUTPUT);

    ledcSetup(RED_CHANNEL, 5000, 8);
    ledcSetup(GREEN_CHANNEL, 5000, 8);
    ledcSetup(BLUE_CHANNEL, 5000, 8);

    ledcAttachPin(Settings::activePins().ledRedPin, RED_CHANNEL);
    ledcAttachPin(Settings::activePins().ledGreenPin, GREEN_CHANNEL);
    ledcAttachPin(Settings::activePins().ledBluePin, BLUE_CHANNEL);

    off();
}

void Led::setColor(uint8_t r, uint8_t g, uint8_t b)
{
#if RGB_COMMON_CATHODE
    ledcWrite(RED_CHANNEL, r);
    ledcWrite(GREEN_CHANNEL, g);
    ledcWrite(BLUE_CHANNEL, b);
#else
    ledcWrite(RED_CHANNEL, 255 - r);
    ledcWrite(GREEN_CHANNEL, 255 - g);
    ledcWrite(BLUE_CHANNEL, 255 - b);
#endif
}

void Led::off()
{
    setColor(0,0,0);
}

void Led::loop()
{
}

void Led::test()
{
    setColor(255,0,0);
    delay(1000);

    setColor(0,255,0);
    delay(1000);

    setColor(0,0,255);
    delay(1000);

    setColor(255,255,255);
    delay(1000);

    off();
}
