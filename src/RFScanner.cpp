#include "RFScanner.h"
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define CE_PIN 4
#define CS_PIN 25
#define BUTTON_PIN 32

RF24 scannerRadio(CE_PIN, CS_PIN);

const int SAMPLES = 20;
float activity[126];

const int GRAPH_TOP = 16;
const int GRAPH_BOTTOM = 106;
const int GRAPH_LEFT = 16;

// 40x50, 1bpp, MSB-first, row-major (250 bytes), logo 40x32 centered, 0 = transparent
const uint8_t skull[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x03, 0xFF, 0xC0, 0x00,
  0x00, 0x00, 0x00, 0x70, 0x00,
  0x00, 0x03, 0xFF, 0x8C, 0x00,
  0x00, 0x0E, 0x00, 0xF8, 0x00,
  0x38, 0x18, 0xEF, 0x30, 0x18,
  0x7E, 0x1B, 0xEF, 0xC0, 0x7E,
  0x1F, 0x0F, 0xEF, 0xF0, 0xF8,
  0x0F, 0x1F, 0xEF, 0xF8, 0xE0,
  0x87, 0xFF, 0xEF, 0xFF, 0xE1,
  0xFF, 0xFF, 0xEF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xEF, 0xFF, 0xFF,
  0x7F, 0xFF, 0xEF, 0xFF, 0xFE,
  0x1B, 0xFF, 0xEF, 0xFF, 0xFC,
  0x00, 0xFF, 0xEF, 0xFF, 0x00,
  0x00, 0xFF, 0xEF, 0xFF, 0x00,
  0x00, 0xFF, 0xEF, 0xFF, 0x00,
  0x00, 0xFF, 0xEC, 0x07, 0x00,
  0x00, 0xFF, 0xEC, 0x03, 0x00,
  0x00, 0xFF, 0xEC, 0x03, 0x00,
  0x03, 0xFF, 0xEE, 0x07, 0xC0,
  0x3F, 0xFF, 0xEE, 0x07, 0xFC,
  0x7F, 0xFF, 0xEF, 0x8F, 0xFE,
  0xFF, 0xFF, 0xEF, 0xFF, 0xFF,
  0xEF, 0xFF, 0xEF, 0xFF, 0xF7,
  0x87, 0x9F, 0xEF, 0xF9, 0xE1,
  0x0F, 0x07, 0xFF, 0xE0, 0xF0,
  0x3F, 0x03, 0xFF, 0xC0, 0xFC,
  0x3E, 0x03, 0xFF, 0xC0, 0x7C,
  0x00, 0x01, 0xFF, 0x80, 0x00,
  0x00, 0x01, 0xFF, 0x80, 0x00,
  0x00, 0x03, 0xFF, 0x80, 0x00,
  0x00, 0x03, 0xC7, 0x80, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00
};

#define SKULL_W 40
#define SKULL_H 50

void introAnimation()
{
    const int SCREEN_W = 160;

    // Layout (160x128, rotation 1)
    const int titleY   = 5;
    const int skullX   = (SCREEN_W - SKULL_W) / 2;   // 60 -> horizontally centered
    const int skullY   = 34;                         // centered in the space between title and bar

    const int barWidth  = 90;
    const int barHeight = 8;
    const int barX      = (SCREEN_W - barWidth) / 2; // 35
    const int barY      = 104;
    const int pctY      = 116;                       // percentage under the bar

    tft.fillScreen(TFT_WHITE);

    // Title at top, centered
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.drawCentreString("RF SCANNER", SCREEN_W / 2, titleY, 1);

    delay(400);

    // Skull appears (6-arg form = transparent background)
    tft.drawBitmap(skullX, skullY, skull, SKULL_W, SKULL_H, TFT_BLACK);

    delay(400);

    // Fake boot progress
    for (int progress = 0; progress <= 100; progress += 5)
    {
        int filledWidth = (barWidth * progress) / 100;

        // Bar background + fill
        tft.fillRect(barX, barY, barWidth, barHeight, TFT_WHITE);
        tft.drawRect(barX - 1, barY - 1, barWidth + 2, barHeight + 2, TFT_BLACK);
        tft.fillRect(barX, barY, filledWidth, barHeight, TFT_GREEN);

        // Percentage, centered under the bar
        tft.fillRect(SCREEN_W / 2 - 20, pctY, 40, 10, TFT_WHITE);
        tft.setTextColor(TFT_BLACK, TFT_WHITE);
        tft.drawCentreString(String(progress) + "%", SCREEN_W / 2, pctY, 1);

        delay(70);
    }

    // Final skull flash
    tft.fillRect(skullX, skullY, SKULL_W, SKULL_H, TFT_WHITE);
    tft.drawBitmap(skullX, skullY, skull, SKULL_W, SKULL_H, TFT_CYAN);
    delay(120);

    tft.fillRect(skullX, skullY, SKULL_W, SKULL_H, TFT_WHITE);
    tft.drawBitmap(skullX, skullY, skull, SKULL_W, SKULL_H, TFT_BLACK);
    delay(300);

    // Clear the intro so the scanner graph starts on a clean screen
    tft.fillScreen(TFT_WHITE);
}

uint16_t activityColor(float activity)
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    if (activity < 0.5)
    {
        // Cyan -> Green
        float t = activity / 0.5;

        r = 0;
        g = 255;
        b = 255 * (1 - t);
    }
    else
    {
        // Green -> Red
        float t = (activity - 0.5) / 0.5;

        r = 255 * t;
        g = 255 * (1 - t);
        b = 0;
    }

    return tft.color565(r, g, b);
}

void rfScannerSetup()
{
    Serial.begin(115200);

    scannerRadio.powerDown();
    delay(10);

    // Initialize nRF24 first
    if (!scannerRadio.begin())
    {
        Serial.println("nRF24 initialization failed!");

        while (true)
        {
            delay(1000);
        }
    }

    scannerRadio.setDataRate(RF24_1MBPS);
    scannerRadio.setChannel(76);

    scannerRadio.setAutoAck(false);
    scannerRadio.disableCRC();

    scannerRadio.stopListening();
    scannerRadio.startListening();

    Serial.println("nRF24 initialized!");

    scannerRadio.printDetails();

    scannerRadio.startListening();

    // Initialize TFT after the scannerRadio
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_WHITE);

    introAnimation();

    
    // Clear intro leftovers (skull, loading bar, percentage)
    tft.fillScreen(TFT_WHITE);

    // Title at the top
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.drawCentreString("RF SCANNER", 80, 4, 1);

    // Draw the static graph frame
    tft.drawRect(
        GRAPH_LEFT - 1,
        GRAPH_TOP,
        128,
        GRAPH_BOTTOM - GRAPH_TOP + 1,
        TFT_BLACK);

    // ...channel labels loop stays the sam

    // Add channel labels below the graph
    for (int channel = 20; channel <= 100; channel += 20)
    {
        tft.setCursor(
            GRAPH_LEFT + channel - 3,
            GRAPH_BOTTOM + 3);

        tft.setTextColor(TFT_BLACK, TFT_WHITE);
        tft.setTextSize(1);

        tft.print(channel);
    }
}

bool rfScannerLoop()
{
    // Clear only the inside of the graph so the frame stays intact
    tft.fillRect(
        GRAPH_LEFT,
        GRAPH_TOP + 1,
        126,
        GRAPH_BOTTOM - GRAPH_TOP - 1,
        TFT_WHITE);

    // Reset the stored activity values
    for (int channel = 0; channel < 126; channel++)
    {
        activity[channel] = 0;
    }

    // Scan every nRF24 channel
    for (int channel = 0; channel < 126; channel++)
    {
        scannerRadio.setChannel(channel);

        int detections = 0;

        // Take multiple RPD measurements on the current channel
        for (int i = 0; i < SAMPLES; i++)
        {
            delayMicroseconds(170);

            if (scannerRadio.testRPD())
            {
                detections++;
            }
        }

        // Convert detections into a 0.0 - 1.0 activity value
        activity[channel] = (float)detections / SAMPLES;
    }

    // Convert each channel's activity into a bar height
    for (int channel = 0; channel < 126; channel++)
    {
        int barHeight = activity[channel] * (GRAPH_BOTTOM - GRAPH_TOP);

        tft.fillRect(
            channel + GRAPH_LEFT,
            GRAPH_BOTTOM - barHeight,
            1,
            barHeight,
            activityColor(activity[channel]));
    }

    delay(500);

    if (digitalRead(BUTTON_PIN) == LOW)
    {
        delay(50);

        while (digitalRead(BUTTON_PIN) == LOW)
        {
            delay(10);
        }

        return true;
    }

    return false;
}