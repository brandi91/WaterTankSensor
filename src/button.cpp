#include "button.h"

#include "config.h"
#include "logger.h"

bool Button::lastStableState = HIGH;
bool Button::lastReading = HIGH;

bool Button::webServerPressSent = false;
bool Button::configPortalPressSent = false;

unsigned long Button::lastChangeTime = 0;
unsigned long Button::pressedSince = 0;

ButtonEvent Button::event = ButtonEvent::None;

void Button::begin()
{
    pinMode(
        PIN_BUTTON,
        INPUT_PULLUP
    );

    lastReading =
        digitalRead(PIN_BUTTON);

    lastStableState =
        lastReading;

    Logger::info(
        "Button initialized on GPIO " +
        String(PIN_BUTTON)
    );
}

void Button::loop()
{
    const bool reading =
        digitalRead(PIN_BUTTON);

    const unsigned long now =
        millis();

    /*
     * Entprellung:
     * Jede rohe Zustandsänderung startet den
     * Entprell-Timer neu.
     */
    if (reading != lastReading)
    {
        lastReading = reading;
        lastChangeTime = now;
    }

    if (
        now - lastChangeTime <
        BUTTON_DEBOUNCE_MS
    )
    {
        return;
    }

    /*
     * Der entprellte Zustand hat sich geändert.
     */
    if (reading != lastStableState)
    {
        lastStableState = reading;

        if (lastStableState == LOW)
        {
            /*
             * Taste wurde gedrückt.
             */
            pressedSince = now;

            webServerPressSent = false;
            configPortalPressSent = false;

            Logger::info(
                "Button raw: pressed"
            );
        }
        else
        {
            /*
             * Taste wurde losgelassen.
             */
            Logger::info(
                "Button raw: released"
            );

            /*
             * Nur wenn keine der Haltezeiten erreicht
             * wurde, ist es ein kurzer Tastendruck.
             */
            if (
                !webServerPressSent &&
                !configPortalPressSent
            )
            {
                event =
                    ButtonEvent::ShortPress;
            }
        }
    }

    /*
     * Nach fünf Sekunden:
     * normalen Webserver starten.
     */
    if (
        lastStableState == LOW &&
        !webServerPressSent &&
        now - pressedSince >=
            BUTTON_WEB_SERVER_PRESS_MS
    )
    {
        webServerPressSent = true;

        event =
            ButtonEvent::WebServerPress;

        Logger::info(
            "Button held for 5 seconds"
        );
    }

    /*
     * Nach 15 Sekunden:
     * Konfigurations-AP starten.
     *
     * Das Ereignis wird ausgelöst, während die
     * Taste weiterhin gedrückt wird.
     */
    if (
        lastStableState == LOW &&
        !configPortalPressSent &&
        now - pressedSince >=
            BUTTON_CONFIG_PORTAL_PRESS_MS
    )
    {
        configPortalPressSent = true;

        event =
            ButtonEvent::ConfigPortalPress;

        Logger::info(
            "Button held for 15 seconds"
        );
    }
}

ButtonEvent Button::getEvent()
{
    const ButtonEvent result =
        event;

    event =
        ButtonEvent::None;

    return result;
}