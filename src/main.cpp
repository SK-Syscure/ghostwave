#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Booting...");

  tft.init();
  tft.setRotation(1);

  Serial.println("Display init done. Starting color test...");

  // Cycle through primary colors — confirms SPI/CS/DC/CLK/MOSI all work
  tft.fillScreen(TFT_RED);
  Serial.println("RED");
  delay(1000);

  tft.fillScreen(TFT_GREEN);
  Serial.println("GREEN");
  delay(1000);

  tft.fillScreen(TFT_BLUE);
  Serial.println("BLUE");
  delay(1000);

  tft.fillScreen(TFT_BLACK);

  // Now test text rendering
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Hello World!", 10, 10);
  tft.drawString("Ghostwave", 10, 40);

  Serial.println("Text drawn. If you see this on Serial but NOT on screen,");
  Serial.println("the issue is DC/CS/RST wiring, not power.");
}

void loop() {
  // Blink a border rectangle every second so you know it's alive, not frozen
  static bool on = false;
  tft.drawRect(0, 0, tft.width(), tft.height(), on ? TFT_YELLOW : TFT_BLACK);
  on = !on;
  delay(1000);
}