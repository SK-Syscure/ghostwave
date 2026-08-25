#include <Arduino.h>    // Core Arduino functions (pinMode, Serial, etc.)
#include <TFT_eSPI.h>   // TFT display library by Bodmer

// Create a display object — this is your interface to the screen
TFT_eSPI tft = TFT_eSPI();

void setup() {
  // setup() runs ONCE when the microcontroller powers on or resets

  // Start serial communication at 115200 baud (for debugging via USB)
  Serial.begin(115200);
  Serial.println("Booting...");  // Print to Serial Monitor, not the screen

  tft.init();           // Initialize the display hardware
  tft.setRotation(1);   // Rotate screen: 0=portrait, 1=landscape, 2,3=flipped
  tft.fillScreen(TFT_BLACK);  // Clear screen by filling it black

  // Set text color to WHITE with BLACK background (background prevents artifacts)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);   // Font scale: 1=small, 2=medium, 3=large, etc.

  // Draw "Hello World!" starting at pixel position x=10, y=10
  tft.drawString("Hello World!", 10, 10);

  Serial.println("Display initialized.");
}

void loop() {
  // loop() runs FOREVER after setup()
  // Empty here — the display holds its content without needing refresh
}