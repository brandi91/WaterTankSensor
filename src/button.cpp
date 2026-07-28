#include "button.h"
#include "config.h"
#include "logger.h"

bool Button::lastStableState = HIGH;
bool Button::lastReading = HIGH;
bool Button::longPressSent = false;

unsigned long Button::lastChangeTime = 0;
unsigned long Button::pressedSince = 0;

ButtonEvent Button::event = ButtonEvent::None;

void Button::begin()
{
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    lastReading = digitalRead(PIN_BUTTON);
    lastStableState = lastReading;

    Logger::info("Button initialized on GPIO " + String(PIN_BUTTON));
}

void Button::loop()
{
    const bool reading = digitalRead(PIN_BUTTON);
    const unsigned long now = millis();

    if (reading != lastReading)
    {
        lastReading = reading;
        lastChangeTime = now;
    }

    if ((now - lastChangeTime) < BUTTON_DEBOUNCE_MS)
    {
        return;
    }

    if (reading != lastStableState)
    {
        lastStableState = reading;

        if (lastStableState == LOW)
        {
            pressedSince = now;
            longPressSent = false;

            Logger::info("Button raw: pressed");
        }
        else
        {
            Logger::info("Button raw: released");

            if (!longPressSent)
            {
                event = ButtonEvent::ShortPress;
            }
        }
    }

    if (
        lastStableState == LOW &&
        !longPressSent &&
        (now - pressedSince >= BUTTON_LONG_PRESS_MS)
    )
    {
        longPressSent = true;
        event = ButtonEvent::LongPress;
    }
}

ButtonEvent Button::getEvent()
{
    const ButtonEvent result = event;
    event = ButtonEvent::None;

    return result;
}