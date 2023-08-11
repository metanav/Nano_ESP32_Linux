#include <Wire.h>
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

#define CARDKB_I2C_ADDR 0x5F
#define TEXT_WIDTH 8
#define TEXT_HEIGHT 16      // Height of text to be printed and scrolled
#define BOTTOM_FIXED_AREA 0 // Number of lines in bottom fixed area (lines counted from bottom of screen)
#define TOP_FIXED_AREA 16   // Number of lines in top fixed area (lines counted from top of screen)
#define YMAX 320            // Bottom of screen area

// Arduino Uno R3/R4
#define TFT_DC 9
#define TFT_CS 10

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// The initial y coordinate of the top of the scrolling area
uint16_t yStart = TOP_FIXED_AREA;
// yArea must be a integral multiple of TEXT_HEIGHT
uint16_t yArea = YMAX - TOP_FIXED_AREA - BOTTOM_FIXED_AREA;
// The initial y coordinate of the top of the bottom text line
uint16_t yDraw = YMAX - BOTTOM_FIXED_AREA - TEXT_HEIGHT;

// Keep track of the drawing x coordinate
uint16_t xPos = 0;

boolean escapeSeq = false;

// We have to blank the top line each time the display is scrolled, but this takes up to 13 milliseconds
// for a full width line, meanwhile the serial buffer may be filling... and overflowing
// We can speed up scrolling of short text lines by just blanking the character we drew
int blank[19]; // We keep all the strings pixel lengths to optimise the speed of the top line blanking

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200);

  Wire1.begin();

  tft.begin();
  tft.setRotation(0); // Must be 0 to work vertical scrolling correctly
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_BLACK, ILI9341_YELLOW);
  tft.fillRect(0, 0, 240, 16, ILI9341_YELLOW);
  tft.setTextSize(2);
  tft.print("  Nano ESP32 Linux");
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setScrollMargins(TOP_FIXED_AREA, BOTTOM_FIXED_AREA);

  for (byte i = 0; i < 18; i++) {
    blank[i] = 0;
  }
}


void loop(void)
{
  while (Serial1.available()) {
    char c = Serial1.read();
    // If it is a CR or we are near end of line then scroll one line
    if (c == '\r'  || xPos > 231) {
      xPos = 0;
      yDraw = scroll_line(); // It can take 13ms to scroll and blank 16 pixel lines
    }

    // delete key
    if (c == 8) {
      xPos -= TEXT_WIDTH;
      Serial.print("\nDelete: ");
      //tft.drawChar(xPos, yDraw, 219, ILI9341_BLACK, ILI9341_BLACK , 1);
      tft.fillRect(xPos, yDraw, TEXT_WIDTH, TEXT_HEIGHT, ILI9341_BLACK);

      continue;
    }

    // start of ANSI Escape Sequence
    if (c == 27) {
      escapeSeq = true;
    }

    if (c > 31 && c < 128) {
      if (escapeSeq) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
          escapeSeq = false;
        }
        continue;
      }
      tft.drawChar(xPos, yDraw, c, ILI9341_WHITE, ILI9341_BLACK , 1);
      xPos += TEXT_WIDTH;
      blank[(18 + (yStart - TOP_FIXED_AREA) / TEXT_HEIGHT) % 19] = xPos; // Keep a record of line lengths
    }
  }

  while (Serial.available()) {
    char c = Serial.read();
    Serial1.print(c);
  }

  Wire1.requestFrom(CARDKB_I2C_ADDR, 1);

  while (Wire1.available()) {
    byte b = Wire1.read();

    switch (b) {
      case 0xA8: // Fn+C
        Serial1.write(0x03); // Send Ctrl-C
        break;
      case 0x96: // Fn+P
        Serial1.write(0x10); // Send Ctrl-P
        break;
      case 0x90: // Fn+R
        Serial1.write(0x12); // Send Ctrl-R
        break;
      default:
        break;
    }

    char c = b;
    if (c != 0) {
      Serial1.print(c);
      Serial.print(c, HEX);
    }
  }
}

int scroll_line() {
  int yTemp = yStart; // Store the old yStart, this is where we draw the next line

  // Use the record of line lengths to optimise the rectangle size we need to erase the top line
  tft.fillRect(0, yStart, blank[(yStart - TOP_FIXED_AREA) / TEXT_HEIGHT], TEXT_HEIGHT, ILI9341_BLACK);

  // Change the top of the scroll area
  yStart += TEXT_HEIGHT;

  // The value must wrap around as the screen memory is a circular buffer
  if (yStart >= YMAX - BOTTOM_FIXED_AREA) {
    yStart = TOP_FIXED_AREA + (yStart - YMAX + BOTTOM_FIXED_AREA);
  }

  // Now we can scroll the display
  tft.scrollTo(yStart);

  return  yTemp;
}
