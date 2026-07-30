#pragma once

class FactoryReset
{
public:
    static void execute();
    static bool shouldStartConfigPortal();
    static void clearConfigPortalRequest();
};
