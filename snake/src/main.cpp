// ============================================================================
//  SNAKE  -  ESP32-WROOM + ILI9341 2.4" (VSPI) + XPT2046 touch (HSPI)
//  Control: tap next to the snake head, on the side you want it to turn.
//
//  Touch on a dedicated HSPI bus (no breadboard, direct jumper wires):
//     T_CLK->D14  T_DIN(MOSI)->D13  T_DO(MISO)->D27  T_CS->D33  T_IRQ->D32
//
//  Calibration and high score are stored in flash (Preferences).
//  On the menu, hold the screen for 2 s to recalibrate the touch.
// ============================================================================

#include <SPI.h>
#include <TFT_eSPI.h>
#include <Preferences.h>

TFT_eSPI   tft = TFT_eSPI();
Preferences prefs;

// ---------- Touch (HSPI) ----------
static const int T_CLK = 14, T_MISO = 27, T_MOSI = 13, T_CS = 33, T_IRQ = 32;
SPIClass tSPI(HSPI);

static const int M = 28;          // margin used by the calibration targets
int SCRW, SCRH;

struct Cal { int sxMin, sxMax, syMin, syMax, swap; uint32_t magic; };
Cal cal;
static const uint32_t CAL_MAGIC = 0xCA11B003;

// ---------- Game ----------
static const int CELL   = 10;
static const int TOPBAR = 20;
static const int COLS   = 32;                 // 320 / 10
static const int ROWS   = (240 - TOPBAR) / CELL; // = 22
static const int MAXLEN = COLS * ROWS;

uint8_t sx[MAXLEN], sy[MAXLEN];   // segments (head = index 0)
int     snakeLen;
int     dirx, diry;               // current direction
int     pdirx, pdiry;             // pending direction (applied on next step)
int     foodX, foodY;
int     score, highScore;
uint32_t stepMs;                  // interval between steps
uint32_t lastStep;

// colors
#define C_BG     TFT_BLACK
#define C_BODY   0x07E0           // green
#define C_HEAD   0xAFE5           // yellow-green
#define C_FOOD   0xF800           // red

enum State { ST_MENU, ST_PLAY, ST_OVER };
State state;

// ======================= XPT2046 =======================
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

bool readRaw(uint16_t &rx, uint16_t &ry) {
  if (!touching()) return false;
  uint32_t ax = 0, ay = 0; int n = 0;
  for (int i = 0; i < 20 && touching(); i++) {
    ay += xptRead(0x90);
    ax += xptRead(0xD0);
    n++;
  }
  if (n < 8) return false;
  rx = ax / n; ry = ay / n;
  if (rx < 50 || ry < 50 || rx > 4050 || ry > 4050) return false;
  return true;
}

bool getPoint(int &x, int &y) {
  uint16_t rx, ry;
  if (!readRaw(rx, ry)) return false;
  int rForX = cal.swap ? ry : rx;
  int rForY = cal.swap ? rx : ry;
  x = map(rForX, cal.sxMin, cal.sxMax, M, SCRW - M);
  y = map(rForY, cal.syMin, cal.syMax, M, SCRH - M);
  x = constrain(x, 0, SCRW - 1);
  y = constrain(y, 0, SCRH - 1);
  return true;
}

// ======================= Calibration =======================
void drawTarget(int x, int y, uint16_t c) {
  tft.drawCircle(x, y, 10, c);
  tft.drawLine(x - 16, y, x + 16, y, c);
  tft.drawLine(x, y - 16, x, y + 16, c);
}

void captureTarget(int tx, int ty, uint16_t &rx, uint16_t &ry) {
  tft.fillScreen(C_BG);
  tft.setTextColor(TFT_WHITE, C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Tap the target", SCRW / 2, SCRH / 2, 4);
  drawTarget(tx, ty, TFT_YELLOW);
  while (touching()) delay(5);
  delay(150);
  uint16_t a, b;
  while (!readRaw(a, b)) delay(5);
  rx = a; ry = b;
  drawTarget(tx, ty, TFT_GREEN);
  while (touching()) delay(5);
  delay(250);
}

void calibrate() {
  uint16_t ax, ay, bx, by, cx, cy;
  captureTarget(M, M, ax, ay);               // top-left
  captureTarget(SCRW - M, M, bx, by);        // top-right
  captureTarget(M, SCRH - M, cx, cy);        // bottom-left

  // detect whether touch axes are swapped relative to the screen
  int dX = abs((int)bx - (int)ax);
  int dY = abs((int)by - (int)ay);
  cal.swap = (dY > dX) ? 1 : 0;
  if (cal.swap) {
    cal.sxMin = ay; cal.sxMax = by;
    cal.syMin = ax; cal.syMax = cx;
  } else {
    cal.sxMin = ax; cal.sxMax = bx;
    cal.syMin = ay; cal.syMax = cy;
  }
  cal.magic = CAL_MAGIC;

  prefs.begin("snake", false);
  prefs.putBytes("cal", &cal, sizeof(cal));
  prefs.end();

  Serial.printf("Calibration saved: swap=%d sx[%d..%d] sy[%d..%d]\n",
                cal.swap, cal.sxMin, cal.sxMax, cal.syMin, cal.syMax);
}

bool loadCal() {
  prefs.begin("snake", true);
  size_t n = prefs.getBytes("cal", &cal, sizeof(cal));
  highScore = prefs.getInt("hi", 0);
  prefs.end();
  return (n == sizeof(cal) && cal.magic == CAL_MAGIC);
}

void saveHigh() {
  prefs.begin("snake", false);
  prefs.putInt("hi", highScore);
  prefs.end();
}

// ======================= Game drawing =======================
void eraseCell(int gx, int gy) {
  tft.fillRect(gx * CELL, TOPBAR + gy * CELL, CELL, CELL, C_BG);
}
void drawSeg(int gx, int gy, uint16_t c) {
  tft.fillRoundRect(gx * CELL + 1, TOPBAR + gy * CELL + 1, CELL - 2, CELL - 2, 2, c);
}
void drawFood() {
  tft.fillCircle(foodX * CELL + CELL / 2, TOPBAR + foodY * CELL + CELL / 2, CELL / 2 - 1, C_FOOD);
}
void drawScoreBar() {
  tft.fillRect(0, 0, SCRW, TOPBAR - 2, C_BG);
  tft.setTextColor(TFT_WHITE, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Score: " + String(score), 4, 2, 2);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("Best: " + String(highScore), SCRW - 4, 2, 2);
  tft.drawFastHLine(0, TOPBAR - 1, SCRW, 0x39E7);
}

bool onSnake(int gx, int gy) {
  for (int i = 0; i < snakeLen; i++)
    if (sx[i] == gx && sy[i] == gy) return true;
  return false;
}
void placeFood() {
  do {
    foodX = random(COLS);
    foodY = random(ROWS);
  } while (onSnake(foodX, foodY));
  drawFood();
}

// ======================= Game flow =======================
void startGame() {
  snakeLen = 3;
  int cxg = COLS / 2, cyg = ROWS / 2;
  for (int i = 0; i < snakeLen; i++) { sx[i] = cxg - i; sy[i] = cyg; }
  dirx = 1; diry = 0;
  pdirx = 1; pdiry = 0;
  score = 0;
  stepMs = 170;

  tft.fillRect(0, TOPBAR, SCRW, SCRH - TOPBAR, C_BG);
  drawScoreBar();
  for (int i = 0; i < snakeLen; i++) drawSeg(sx[i], sy[i], i == 0 ? C_HEAD : C_BODY);
  placeFood();

  lastStep = millis();
  state = ST_PLAY;
}

void gameOver() {
  if (score > highScore) { highScore = score; saveHigh(); }
  tft.fillRoundRect(SCRW / 2 - 110, SCRH / 2 - 55, 220, 110, 8, 0x2104);
  tft.drawRoundRect(SCRW / 2 - 110, SCRH / 2 - 55, 220, 110, 8, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_RED, 0x2104);
  tft.drawString("GAME OVER", SCRW / 2, SCRH / 2 - 28, 4);
  tft.setTextColor(TFT_WHITE, 0x2104);
  tft.drawString("Score: " + String(score) + "   Best: " + String(highScore), SCRW / 2, SCRH / 2 + 2, 2);
  tft.setTextColor(TFT_GREENYELLOW, 0x2104);
  tft.drawString("tap to play again", SCRW / 2, SCRH / 2 + 30, 2);
  state = ST_OVER;
  while (touching()) delay(5);   // wait for release
}

void setDir(int nx, int ny) {
  // cannot reverse 180 degrees relative to the CURRENT direction
  if (nx == -dirx && ny == -diry) return;
  pdirx = nx; pdiry = ny;
}

void step() {
  dirx = pdirx; diry = pdiry;
  int nx = sx[0] + dirx;
  int ny = sy[0] + diry;

  // wrap around the walls (Nokia style)
  nx = (nx + COLS) % COLS;
  ny = (ny + ROWS) % ROWS;

  bool grow = (nx == foodX && ny == foodY);

  // self collision (the tail frees its cell when not growing)
  int last = grow ? snakeLen - 1 : snakeLen - 2;
  for (int i = 1; i <= last; i++)
    if (sx[i] == nx && sy[i] == ny) { gameOver(); return; }

  // demote current head to body
  drawSeg(sx[0], sy[0], C_BODY);
  // erase tail when not growing
  if (!grow) eraseCell(sx[snakeLen - 1], sy[snakeLen - 1]);
  // extend when growing
  if (grow && snakeLen < MAXLEN) snakeLen++;
  // shift body
  for (int i = snakeLen - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
  // new head
  sx[0] = nx; sy[0] = ny;
  drawSeg(nx, ny, C_HEAD);

  if (grow) {
    score++;
    drawScoreBar();
    if (stepMs > 75) stepMs -= 6;   // speed up
    placeFood();
  }
}

void showMenu() {
  tft.fillScreen(C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_BODY, C_BG);
  tft.drawString("SNAKE", SCRW / 2, 55, 6);
  tft.setTextColor(TFT_WHITE, C_BG);
  tft.drawString("Tap beside the snake to turn", SCRW / 2, 120, 2);
  tft.setTextColor(TFT_GREENYELLOW, C_BG);
  tft.drawString("tap to start", SCRW / 2, 150, 4);
  tft.setTextColor(0x8410, C_BG);
  tft.drawString("Best: " + String(highScore) + "   (hold 2s = recalibrate)", SCRW / 2, 210, 2);
  state = ST_MENU;
}

// ======================= setup / loop =======================
void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println("\n=== SNAKE ===");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(3);
  SCRW = tft.width();
  SCRH = tft.height();

  tSPI.begin(T_CLK, T_MISO, T_MOSI, T_CS);
  pinMode(T_CS, OUTPUT);  digitalWrite(T_CS, HIGH);
  pinMode(T_IRQ, INPUT);

  randomSeed(esp_random());

  if (!loadCal()) {
    tft.fillScreen(C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_CYAN, C_BG);
    tft.drawString("Touch calibration", SCRW / 2, SCRH / 2 - 18, 4);
    tft.setTextColor(TFT_WHITE, C_BG);
    tft.drawString("tap the 3 targets", SCRW / 2, SCRH / 2 + 18, 2);
    delay(1600);
    calibrate();
  }
  showMenu();
}

// directional touch control (during the game)
bool     tActive = false;
// touch handling (menu / game over)
uint32_t pressStart = 0;

void loop() {
  int x, y;
  bool pressed = getPoint(x, y);

  if (state == ST_PLAY) {
    // ---- directional touch: tap beside the snake to turn ----
    if (pressed) {
      if (!tActive) {                    // register one direction per touch (edge)
        tActive = true;
        // reference = snake HEAD (tap above it -> go up, etc.)
        int hx = sx[0] * CELL + CELL / 2;
        int hy = TOPBAR + sy[0] * CELL + CELL / 2;
        int rx = x - hx, ry = y - hy;
        int ndx = 0, ndy = 0;
        if (abs(rx) > abs(ry)) ndx = rx > 0 ? 1 : -1;
        else                   ndy = ry > 0 ? 1 : -1;
        setDir(ndx, ndy);
      }
    } else tActive = false;

    // ---- step on the right interval ----
    if (millis() - lastStep >= stepMs) {
      lastStep = millis();
      step();
    }
  }
  else if (state == ST_MENU) {
    if (pressed) {
      if (pressStart == 0) pressStart = millis();
      else if (millis() - pressStart > 2000) {       // held: recalibrate
        calibrate();
        showMenu();
        pressStart = 0;
        while (touching()) delay(5);
      }
    } else {
      if (pressStart > 0 && millis() - pressStart < 2000) startGame();
      pressStart = 0;
    }
  }
  else { // ST_OVER
    if (pressed) showMenu();
  }

  delay(5);
}
