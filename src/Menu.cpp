#include "Menu.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

#define BUTTON_PIN 32

TFT_eSPI menuTft = TFT_eSPI();

enum MenuItem
{
    MENU_RF_SCANNER = 0,
    MENU_BLE_MODE,
    MENU_COUNT
};

bool buttonPressed()
{
    return digitalRead(BUTTON_PIN) == LOW;
}

void waitForButtonRelease()
{
    while (buttonPressed())
    {
        delay(10);
    }

    delay(50);
}

void drawMenu(int selected)
{
    menuTft.fillScreen(TFT_WHITE);

    menuTft.setTextColor(TFT_BLACK, TFT_WHITE);
    menuTft.setTextSize(1);

    menuTft.setCursor(42, 8);
    menuTft.print("GHOSTWAVE");

    menuTft.drawLine(10, 20, 118, 20, TFT_BLACK);

    // RF Scanner
    menuTft.setCursor(20, 38);

    if (selected == MENU_RF_SCANNER)
    {
        menuTft.print("> RF SCANNER");
    }
    else
    {
        menuTft.print("  RF SCANNER");
    }

    // BLE Mode
    menuTft.setCursor(20, 58);

    if (selected == MENU_BLE_MODE)
    {
        menuTft.print("> BLE MODE");
    }
    else
    {
        menuTft.print("  BLE MODE");
    }

    menuTft.setCursor(20, 90);
    menuTft.print("PRESS = NEXT");

    menuTft.setCursor(20, 103);
    menuTft.print("HOLD  = SELECT");
}

void menuSetup()
{
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    menuTft.init();
    menuTft.setRotation(1);
    menuTft.fillScreen(TFT_WHITE);
}

int menuSelect()
{
    int selected = MENU_RF_SCANNER;

    drawMenu(selected);

    while (true)
    {
        if (buttonPressed())
        {
            unsigned long pressStart = millis();

            while (buttonPressed())
            {
                delay(10);
            }

            unsigned long pressDuration = millis() - pressStart;

            // Long press = select
            if (pressDuration >= 600)
            {
                delay(100);
                return selected;
            }

            // Short press = next item
            selected++;

            if (selected >= MENU_COUNT)
            {
                selected = 0;
            }

            drawMenu(selected);

            delay(100);
        }
    }
}