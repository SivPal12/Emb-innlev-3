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

// Config pins
const int tiltPin = 3;
const int pushPin = 2;

// Config screen
const unsigned int counterTextSize = 3;
const unsigned int headerTextSize = 2;
const unsigned int commandTextSize = 2;

const unsigned int counterOffsetX = 0;
const unsigned int counterOffsetY = headerTextSize*7 + counterTextSize;
const unsigned int commandOffsetX = 0;
const unsigned int commandOffsetY = counterOffsetY + counterTextSize*7 + commandTextSize;

uint16_t commandStartTime;
uint16_t timeToCompleteCommand = 20*1000; // Initial time to complete a command (millis)
const char *pCurrentCommand;
unsigned int totalNumberOfTasks = 2;
unsigned int currentTask = totalNumberOfTasks + 1;

void setup(void) {
  Serial.begin(9600);

  // Pin modes
  pinMode(tiltPin, INPUT);
  pinMode(pushPin, INPUT);

  randomSeed(analogRead(0));
  tft.initR(INITR_BLACKTAB);   // Denne funket best
  newTask();

  initScreen();
}

void loop() {
  printCounter(); // If < 0 do end game
  printCommand();

  if (taskComplete()) {
    newTask();
  }
}

char counterBuffer[4];
char counterOnScreen[4];
/* Prints counter/100 to screen using a double buffer
 * returns time left in milliseconds
 */
int printCounter() {
  int timeLeft = (timeToCompleteCommand) - (millis() - commandStartTime);

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

const char *pPrevCommand;
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

/*
 * Inits a new task
 */
void newTask() {
  int nextTask;
  do {
    nextTask = random(totalNumberOfTasks);
  }
  while (nextTask == currentTask);

  setCommand(nextTask);
  currentTask = nextTask;
  commandStartTime = millis();
}

// Tasks config
void setCommand(int commandNumber) {
  switch (commandNumber) {
  case 0:
    pCurrentCommand = "Flip it!";
    break;
  case 1:
    pCurrentCommand = "Push it!";
    break;
  default:
    pCurrentCommand = "Command not implemented";
    break;
  }
}

bool taskComplete() {
  switch (currentTask) {
  case 0:
    return tiltComplete();
    break;
  case 1:
    return pushComplete();
    break;
  }
  return false;
}

int prevShake = LOW;
bool tiltComplete() {
  bool result = false;

  int currentShake = digitalRead(tiltPin);
  if (prevShake == HIGH && currentShake == LOW) {
    result = true;
  }
  prevShake = currentShake;
  return result;
}

int pushState1 = LOW, pushState2 = HIGH;
// Checks for a complete push of joystick button (HIGH -> LOW -> HIGH)
bool pushComplete() {
  int currentState = digitalRead(pushPin);

  if (pushState1 == HIGH) {
    if (pushState2 == LOW) {
      if (currentState == HIGH) {
        pushState1 = LOW;
        pushState2 = HIGH;
        return true;
      }
    } else {
      pushState2 = currentState;
    }
  } else {
    pushState1 = currentState;
  }
  return false;
}
