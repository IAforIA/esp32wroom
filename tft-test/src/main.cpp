// ============================================================================
//  ILI9341 2.4" display test - ESP32-WROOM-32
//  If the screen flashes RED/GREEN/BLUE and shows "Display OK", the display
//  wiring is correct and you can move on to the touch test.
// ============================================================================

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== ILI9341 display test starting ===");

  // Make sure the backlight is on (LED on D5)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  tft.init();
  tft.setRotation(3);   // 3 = landscape, matches how the panel is mounted here

  // Color test: the screen should flash through these 3 colors
  Serial.println("Filling RED...");
  tft.fillScreen(TFT_RED);    delay(600);
  Serial.println("Filling GREEN...");
  tft.fillScreen(TFT_GREEN);  delay(600);
  Serial.println("Filling BLUE...");
  tft.fillScreen(TFT_BLUE);   delay(600);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Display OK", tft.width() / 2, tft.height() / 2 - 20, 4);
  tft.drawString("ILI9341 2.4in", tft.width() / 2, tft.height() / 2 + 20, 2);

  Serial.println("If white text showed up on the screen, the wiring is OK.");
}

void loop() {
  // Blinking yellow square = firmware is alive
  static uint32_t t = 0;
  static bool on = false;
  if (millis() - t > 500) {
    t = millis();
    on = !on;
    tft.fillRect(8, 8, 24, 24, on ? TFT_YELLOW : TFT_BLACK);
  }
}
