// ============================================================================
//  Touch test with auto-calibration - ESP32-WROOM + ILI9341 2.4" + XPT2046
//
//  Touch on its own SPI bus (HSPI), dedicated pins:
//     T_CLK -> D14 | T_DIN(MOSI) -> D13 | T_DO(MISO) -> D27
//     T_CS  -> D33 | T_IRQ -> D32
//  Nothing is shared with the display, so every wire is a direct jumper
//  (no breadboard needed).
//
//  Flow: tap 3 targets -> compute calibration -> free-draw mode.
//  The final calibration is printed over Serial (115200).
// ============================================================================

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// ---- Touch pins (dedicated HSPI bus) ----
static const int T_CLK  = 14;
static const int T_MISO = 27;   // T_DO
static const int T_MOSI = 13;   // T_DIN
static const int T_CS   = 33;
static const int T_IRQ  = 32;

SPIClass tSPI(HSPI);

// ---- Calibration (filled by the auto-calibration) ----
int   cal_sxMinRaw, cal_sxMaxRaw, cal_syMinRaw, cal_syMaxRaw;
bool  cal_swap;

int SCRW, SCRH;

// Read one XPT2046 channel (0xD0 = X, 0x90 = Y), 12-bit result
uint16_t xptRead(uint8_t cmd) {
  tSPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
  digitalWrite(T_CS, LOW);
  tSPI.transfer(cmd);
  uint8_t a = tSPI.transfer(0x00);
  uint8_t b = tSPI.transfer(0x00);
  digitalWrite(T_CS, HIGH);
  tSPI.endTransaction();
  return ((a << 8) | b) >> 3;
}

bool touching() { return digitalRead(T_IRQ) == LOW; }

// Read averaged raw values while pressed. Returns false if the touch is unstable.
bool readRaw(uint16_t &rx, uint16_t &ry) {
  if (!touching()) return false;
  uint32_t sx = 0, sy = 0; int n = 0;
  for (int i = 0; i < 24 && touching(); i++) {
    sy += xptRead(0x90);
    sx += xptRead(0xD0);
    n++;
    delay(1);
  }
  if (n < 10) return false;
  rx = sx / n; ry = sy / n;
  // drop degenerate readings
  if (rx < 50 || ry < 50 || rx > 4050 || ry > 4050) return false;
  return true;
}

// Draw a crosshair target on the screen
void drawTarget(int x, int y, uint16_t color) {
  tft.drawCircle(x, y, 10, color);
  tft.drawLine(x - 16, y, x + 16, y, color);
  tft.drawLine(x, y - 16, x, y + 16, color);
}

// Wait for the user to tap the target at (sx,sy) and return the captured raw value
void captureTarget(int sx, int sy, uint16_t &rx, uint16_t &ry) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Tap the target", SCRW / 2, SCRH / 2, 4);
  drawTarget(sx, sy, TFT_YELLOW);

  // wait for release first (debounce)
  while (touching()) delay(5);
  delay(150);

  uint16_t ax, ay;
  while (true) {
    if (readRaw(ax, ay)) { rx = ax; ry = ay; break; }
    delay(5);
  }
  drawTarget(sx, sy, TFT_GREEN);
  Serial.printf("Target (%d,%d) -> raw X=%u Y=%u\n", sx, sy, rx, ry);
  // wait for release
  while (touching()) delay(5);
  delay(250);
}

void calibrate() {
  const int m = 28;                 // margin
  int xmin = m, xmax = SCRW - m;
  int ymin = m, ymax = SCRH - m;

  uint16_t ax, ay, bx, by, cx, cy;
  captureTarget(xmin, ymin, ax, ay);   // top-left
  captureTarget(xmax, ymin, bx, by);   // top-right
  captureTarget(xmin, ymax, cx, cy);   // bottom-left

  // Detect whether the touch axes are swapped relative to the screen
  int dX = abs((int)bx - (int)ax);     // raw-X change going top-left -> top-right
  int dY = abs((int)by - (int)ay);     // raw-Y change going top-left -> top-right
  cal_swap = dY > dX;

  if (cal_swap) {
    cal_sxMinRaw = ay;  cal_sxMaxRaw = by;   // screenX <- rawY
    cal_syMinRaw = ax;  cal_syMaxRaw = cx;   // screenY <- rawX
  } else {
    cal_sxMinRaw = ax;  cal_sxMaxRaw = bx;   // screenX <- rawX
    cal_syMinRaw = ay;  cal_syMaxRaw = cy;   // screenY <- rawY
  }

  Serial.println("==== CALIBRATION ====");
  Serial.printf("swap=%d sxMin=%d sxMax=%d syMin=%d syMax=%d\n",
                cal_swap, cal_sxMinRaw, cal_sxMaxRaw, cal_syMinRaw, cal_syMaxRaw);
}

// Convert raw -> screen coordinates using the calibration
bool getPoint(int &sx, int &sy) {
  uint16_t rx, ry;
  if (!readRaw(rx, ry)) return false;
  int rForX = cal_swap ? ry : rx;
  int rForY = cal_swap ? rx : ry;
  sx = map(rForX, cal_sxMinRaw, cal_sxMaxRaw, 28, SCRW - 28);
  sy = map(rForY, cal_syMinRaw, cal_syMaxRaw, 28, SCRH - 28);
  sx = constrain(sx, 0, SCRW - 1);
  sy = constrain(sy, 0, SCRH - 1);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Touch test (auto-calibration) ===");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(3);        // same orientation as the display test
  SCRW = tft.width();
  SCRH = tft.height();

  // Touch on HSPI with the dedicated pins
  tSPI.begin(T_CLK, T_MISO, T_MOSI, T_CS);
  pinMode(T_CS, OUTPUT);  digitalWrite(T_CS, HIGH);
  pinMode(T_IRQ, INPUT);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Touch calibration", SCRW / 2, SCRH / 2 - 20, 4);
  tft.drawString("tap the 3 targets", SCRW / 2, SCRH / 2 + 20, 2);
  delay(1800);

  calibrate();

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Calibrated! Draw with your finger", SCRW / 2, 16, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("(hold to clear)", SCRW / 2, 34, 2);
}

void loop() {
  static uint32_t pressStart = 0;
  int x, y;
  if (getPoint(x, y)) {
    if (pressStart == 0) pressStart = millis();
    // draw a dot where the screen is touched
    tft.fillCircle(x, y, 3, TFT_YELLOW);
    // show the coordinate at the top
    tft.fillRect(0, 0, SCRW, 12, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("X=" + String(x) + "  Y=" + String(y), 4, 2, 2);
    // holding for >1.5s clears the screen
    if (millis() - pressStart > 1500) {
      tft.fillScreen(TFT_BLACK);
      pressStart = 0;
    }
  } else {
    pressStart = 0;
  }
}
