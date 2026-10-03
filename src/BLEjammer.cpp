#include "BLEjammer.h"

#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <TFT_eSPI.h>

#include <esp_bt.h>
#include <esp_bt_main.h>

SPIClass *spJammer = nullptr;

#define BUTTON_PIN 32

TFT_eSPI bleTft = TFT_eSPI();

bool bleActive = false;

RF24 radioJammer(4, 25, 19909090);

byte i = 45;
byte ptr_hop = 0;
byte flag = 0;

byte hopping_channel[] = {
    32, 34, 46, 48, 50, 52,
    0, 1, 2, 4, 6, 8,
    22, 24, 26, 28, 30,
    74, 76, 78, 80, 82, 84, 86
};

void bleJammerShutdown()
{
    radioJammer.stopConstCarrier();
    radioJammer.stopListening();
    radioJammer.powerDown();

    delay(10);
}

void nrfSPIInit()
{
    spJammer = new SPIClass(VSPI);

    spJammer->begin();

    if (radioJammer.begin(spJammer))
    {
        radioJammer.setAutoAck(false);
        radioJammer.stopListening();
        radioJammer.setRetries(0, 0);
        radioJammer.setPayloadSize(31);
        radioJammer.setAddressWidth(4);
        radioJammer.setPALevel(RF24_PA_MAX, true);
        radioJammer.setDataRate(RF24_2MBPS);
        radioJammer.setCRCLength(RF24_CRC_DISABLED);
        radioJammer.startConstCarrier(RF24_PA_MAX, i);
    }
}

void adjustAndSweepChannels()
{
    flag = (i > 79) ? 1 : (i < 2 ? 0 : flag);

    i += flag ? -2 : 2;

    for (int j = 0; j <= 79; j++)
    {
        radioJammer.setChannel(j);
    }
}

void drawBLEScreen()
{
    bleTft.fillScreen(TFT_WHITE);

    bleTft.setTextColor(TFT_BLACK, TFT_WHITE);
    bleTft.setTextSize(1);

    bleTft.setCursor(42, 8);
    bleTft.print("BLE MODE");

    bleTft.drawLine(10, 20, 118, 20, TFT_BLACK);

    bleTft.setCursor(45, 45);

    if (bleActive)
    {
        bleTft.print("ACTIVE");
    }
    else
    {
        bleTft.print("INACTIVE");
    }

    bleTft.setCursor(20, 80);
    bleTft.print("PRESS = TOGGLE");

    bleTft.setCursor(20, 95);
    bleTft.print("HOLD  = MENU");
}

void bleJammerSetup()
{
    esp_bt_controller_deinit();

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED)
    {
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
    }

    nrfSPIInit();

       // UI
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    bleTft.init();
    bleTft.setRotation(1);

    bleActive = false;

    drawBLEScreen();
}

bool bleJammerLoop()
{
    if (digitalRead(BUTTON_PIN) == LOW)
    {
        unsigned long pressStart = millis();

        while (digitalRead(BUTTON_PIN) == LOW)
        {
            delay(10);
        }

        unsigned long pressDuration = millis() - pressStart;

        // HOLD = return to menu
        if (pressDuration >= 600)
        {
            bleJammerShutdown();
            delay(50);
            return true;
        }

        // SHORT PRESS = toggle state
        bleActive = !bleActive;

        drawBLEScreen();

        delay(100);
    }

    // Only your benign BLE-mode work should run when bleActive is true.
    if (bleActive)
    {
        adjustAndSweepChannels();
        
        ptr_hop = (ptr_hop + 1) % sizeof(hopping_channel);
        
        radioJammer.setChannel(hopping_channel[ptr_hop]);
    }

    return false;
}