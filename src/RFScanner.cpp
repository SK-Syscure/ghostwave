#include "RFScanner.h"
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define CE_PIN 4
#define CS_PIN 25

RF24 radio(CE_PIN, CS_PIN);

const int SAMPLES = 20;
float activity[126];

const int GRAPH_TOP = 16;
const int GRAPH_BOTTOM = 106;
const int GRAPH_LEFT = 16;

const uint8_t skull[] PROGMEM = 
{
    // Top bones
    0b11110000, 0b00000000, 0b00011110,
    0b11111000, 0b00000000, 0b00111110,
    0b11111100, 0b00000000, 0b01111110,
    0b01111100, 0b00000000, 0b01111100,
    
    // Top of skull
    0b00111111, 0b11111111, 0b11111000,
    0b00011111, 0b11111111, 0b11110000,
    0b00000111, 0b11111111, 0b11000000,
    0b00001111, 0b11111111, 0b11100000,
    0b00001111, 0b11111111, 0b11100000,
    0b00001111, 0b11111111, 0b11100000,
    
    // Eyes (with bottom outer-corner cuts)
    0b00001100, 0b00111000, 0b01100000,
    0b00001100, 0b00111000, 0b01100000,
    0b00001110, 0b00111000, 0b11100000,
    
    // Nose
    0b00001111, 0b11101111, 0b11100000,
    0b00001111, 0b11101111, 0b11100000,
    
    // Cheek tapers to jaw
    0b00000111, 0b11111111, 0b11000000,
    0b00000011, 0b11111111, 0b10000000,
    
    // Teeth (4 teeth, 3 gaps)
    0b00000011, 0b01101101, 0b10000000,
    0b00000011, 0b01101101, 0b10000000,
    0b00000011, 0b01101101, 0b10000000,
    
    // Bottom bones
    0b01111100, 0b00000000, 0b01111100,
    0b11111100, 0b00000000, 0b01111110,
    0b11111000, 0b00000000, 0b00111110,
    0b11110000, 0b00000000, 0b00011110
};

void introAnimation()
{
    const int skullX = 68;
    const int skullY = 28;

    const int barX = 35;
    const int barY = 75;
    const int barWidth = 90;
    const int barHeight = 8;

    // Initial screen
    tft.fillScreen(TFT_WHITE);

    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setCursor(45, 5);
    tft.print("RF SCANNER");

    delay(400);


    // Skull appears
    tft.drawBitmap(
        skullX,
        skullY,
        skull,
        24,
        24,
        TFT_BLACK,
        TFT_WHITE
    );

    delay(400);


    // Fake boot progress
    for (int progress = 0; progress <= 100; progress += 5)
    {
        int filledWidth = (barWidth * progress) / 100;

        // Clear the progress bar
        tft.fillRect(
            barX,
            barY,
            barWidth,
            barHeight,
            TFT_WHITE
        );

        // Draw the progress
        tft.fillRect(
            barX,
            barY,
            filledWidth,
            barHeight,
            TFT_GREEN
        );

        // Draw percentage
        tft.fillRect(
            65,
            88,
            30,
            10,
            TFT_WHITE
        );

        tft.setTextColor(TFT_BLACK, TFT_WHITE);
        tft.setTextSize(1);
        tft.setCursor(70, 88);
        tft.print(progress);
        tft.print("%");

        delay(70);
    }


    // Final skull flash
    tft.fillRect(
        skullX,
        skullY,
        24,
        24,
        TFT_WHITE
    );

    tft.drawBitmap(
        skullX,
        skullY,
        skull,
        24,
        24,
        TFT_CYAN,
        TFT_WHITE
    );

    delay(120);

    tft.fillRect(
        skullX,
        skullY,
        24,
        24,
        TFT_WHITE
    );

    tft.drawBitmap(
        skullX,
        skullY,
        skull,
        24,
        24,
        TFT_BLACK,
        TFT_WHITE
    );

    delay(300);
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

    // Initialize nRF24 first
    if (!radio.begin())
    {
        Serial.println("nRF24 initialization failed!");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println("nRF24 initialized!");

    radio.startListening();

    // Initialize TFT after the radio
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_WHITE);

    introAnimation();

    // Draw the static graph frame
    tft.drawRect(
        GRAPH_LEFT - 1,
        GRAPH_TOP,
        128,
        GRAPH_BOTTOM - GRAPH_TOP + 1,
        TFT_BLACK
    );

    // Add channel labels below the graph
    for (int channel = 20; channel <= 100; channel += 20)
    {
        tft.setCursor(
            GRAPH_LEFT + channel - 3,
            GRAPH_BOTTOM + 3
        );

        tft.setTextColor(TFT_BLACK, TFT_WHITE);
        tft.setTextSize(1);

        tft.print(channel);
    }
}

void rfScannerLoop()
{
    // Clear only the inside of the graph so the frame stays intact
    tft.fillRect(
        GRAPH_LEFT,
        GRAPH_TOP + 1,
        126,
        GRAPH_BOTTOM - GRAPH_TOP - 1,
        TFT_WHITE
    );

    // Reset the stored activity values
    for (int channel = 0; channel < 126; channel++)
    {
        activity[channel] = 0;
    }

    // Scan every nRF24 channel
    for (int channel = 0; channel < 126; channel++)
    {
        radio.setChannel(channel);

        int detections = 0;

        // Take multiple RPD measurements on the current channel
        for (int i = 0; i < SAMPLES; i++)
        {
            delayMicroseconds(170);

            if (radio.testRPD())
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
            activityColor(activity[channel])
        );
    }

    delay(500);
}