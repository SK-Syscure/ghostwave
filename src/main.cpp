#include <Arduino.h>

#include "Menu.h"
#include "rfscanner/RFScanner.h"
#include "BLEjammer.h"

enum Mode
{
    MODE_RF_SCANNER = 0,
    MODE_BLE_MODE
};

Mode selectedMode;

void setup()
{
    Serial.begin(115200);

    menuSetup();

    int selection = menuSelect();

    selectedMode = static_cast<Mode>(selection);

    if (selectedMode == MODE_RF_SCANNER)
    {
        rfScannerSetup();
    }
    else if (selectedMode == MODE_BLE_MODE)
    {
        bleJammerSetup();
    }
}

void loop()
{
    if (selectedMode == MODE_RF_SCANNER)
    {
        if (rfScannerLoop())
        {
            menuSetup();
            selectedMode = static_cast<Mode>(menuSelect());

            if (selectedMode == MODE_RF_SCANNER)
            {
                rfScannerSetup();
            }
            else if (selectedMode == MODE_BLE_MODE)
            {
                bleJammerSetup();
            }
        }
    }
    else if (selectedMode == MODE_BLE_MODE)
    {
        if (bleJammerLoop())
        {
            menuSetup();
            selectedMode = static_cast<Mode>(menuSelect());

            if (selectedMode == MODE_RF_SCANNER)
            {
                rfScannerSetup();
            }
            else if (selectedMode == MODE_BLE_MODE)
            {
                bleJammerSetup();
            }
        }
    }
}