// ============================================================================
//  User_Setup.h  -  TFT_eSPI
//  ESP32-WROOM-32 DevKit (30 pins)  +  TFT 2.4" ILI9341 SPI 240x320
//
//  Reference only. The actual projects in this repo pass the same pin mapping
//  through PlatformIO build_flags, so you do not have to edit the library.
//  Keep this file if you build from the Arduino IDE instead: copy it over the
//  User_Setup.h that ships with the TFT_eSPI library.
//
//  USB-serial chip on the board: Silicon Labs CP2102 (CP210x driver).
//  The display also has an XPT2046 touch controller + microSD slot.
// ============================================================================

#define USER_SETUP_INFO "ESP32-WROOM ILI9341 2.4in"

// ---- Controller driver ----
#define ILI9341_DRIVER

// ---- Display SPI pins ----
#define TFT_MISO  -1    // display-only, MISO not used (see TOUCH note below)
#define TFT_MOSI  23    // SDI (MOSI) -> D23
#define TFT_SCLK  18    // SCK        -> D18
#define TFT_CS    15    // CS         -> D15   (strapping pin, see README)
#define TFT_DC     2    // DC         -> D2    (strapping pin, see README)
#define TFT_RST    4    // RESET      -> D4

// ---- Backlight (LED) ----
#define TFT_BL     5              // LED -> D5
#define TFT_BACKLIGHT_ON HIGH

// ---- TOUCH XPT2046 ----
// This project drives the touch controller on a SEPARATE SPI bus (HSPI) from
// the application code, not through TFT_eSPI's built-in touch. That is why the
// touch pins below are NOT configured here. Wiring used:
//   T_CLK -> D14   T_DIN -> D13   T_DO -> D27   T_CS -> D33   T_IRQ -> D32
// TFT_eSPI's built-in touch would require sharing SCK/MOSI with the display.

// ---- Fonts ----
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ---- SPI clock ----
// If the image looks noisy, lower SPI_FREQUENCY to 27000000.
#define SPI_FREQUENCY        40000000
#define SPI_READ_FREQUENCY   20000000
#define SPI_TOUCH_FREQUENCY   2500000
