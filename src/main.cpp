#include <Arduino.h>

#include "Menu.h"
#include "RFScanner.h"

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
        // BLE mode placeholder for now.
        Serial.println("BLE mode selected.");
        Serial.println("BLE mode is not implemented yet.");

        // Keep the TFT alive with a simple message.
        // We don't activate the experimental RF transmitter here.
    }
}

void loop()
{
    if (selectedMode == MODE_RF_SCANNER)
    {
        if (rfScannerLoop())
        {
            // Return to menu
            menuSetup();
            selectedMode = static_cast<Mode>(menuSelect());

            if (selectedMode == MODE_RF_SCANNER)
            {
                rfScannerSetup();
            }
        }
    }
    else if (selectedMode == MODE_BLE_MODE)
    {
        delay(100);
    }
}