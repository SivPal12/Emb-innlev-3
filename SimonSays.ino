/*
 * Based on Adafruit Graphictest lib:
 * https://github.com/adafruit/Adafruit-ST7735-Library/tree/master/examples/graphicstest
 */

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <stdio.h>

#define TFT_CS     10
#define TFT_RST    8
#define TFT_DC     9

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

#define TFT_SCLK 13
#define TFT_MOSI 11

// Configs
const unsigned int counterTextSize = 3;
const unsigned int headerTextSize = 2;
const unsigned int commandTextSize = 2;

const unsigned int counterOffsetX = 0;
const unsigned int counterOffsetY = headerTextSize*7 + counterTextSize;
const unsigned int commandOffsetX = 0;
const unsigned int commandOffsetY = counterOffsetY + counterTextSize*7 + commandTextSize;

uint16_t commandStartTime;
uint16_t timeToCompleteCommand = 0;
const char *pCurrentCommand;
const char *pPrevCommand;

void setup(void) {
  Serial.begin(9600);

  tft.initR(INITR_BLACKTAB);   // Denne funket best
  initScreen();
}

void loop() {
  printCounter(); // If < 0 do end game
  printCommand();
}

char counterBuffer[4];
char counterOnScreen[4];
/* Prints counter/100 to screen using a double buffer
 * returns time left in milliseconds
 */
int printCounter() {
  int timeLeft = (timeToCompleteCommand*1000) - (millis() - commandStartTime);

  sprintf(counterBuffer, "%3u", timeLeft > 0 ? timeLeft / 100 : 0);

  tft.setTextSize(counterTextSize);
  for (int i = 0 ; i < 3; i++) {
    if (counterBuffer[i] != counterOnScreen[i]) {
      repaintCounter(i, ST7735_BLACK);
      counterOnScreen[i] = counterBuffer[i];
      repaintCounter(i, ST7735_WHITE);
    }
  }
  return timeLeft;
}

// Used by printCounter
void repaintCounter(int pos, uint16_t color) {
  tft.setCursor(counterOffsetX + counterTextSize*6*(pos), counterOffsetY);
  tft.setTextColor(color);
  tft.print(counterOnScreen[pos]);
}

// Initializes screen. Should be used with rotation of screen
void initScreen() {
  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(3);
  printHeader();
}

void printHeader() {
  tft.setTextSize(headerTextSize);
  tft.setCursor(0, 0);
  tft.setTextColor(ST7735_WHITE);
  //  tft.setTextWrap(true);
  tft.print("Simon says:");
}


void printCommand() {
  if (pPrevCommand != pCurrentCommand) {
    tft.setTextSize(commandTextSize);
    tft.setTextWrap(true);

    repaintCommand(ST7735_BLACK);
    pPrevCommand = pCurrentCommand;
    repaintCommand(ST7735_WHITE);
  }
}

// Used by printCommand
void repaintCommand(uint16_t color) {
  tft.setCursor(commandOffsetX, commandOffsetY);
  tft.setTextColor(color);
  tft.print(pPrevCommand);
}

