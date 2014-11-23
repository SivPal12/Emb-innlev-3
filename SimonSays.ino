/*
 * Based on Adafruit Graphictest lib:
 * https://github.com/adafruit/Adafruit-ST7735-Library/tree/master/examples/graphicstest
 * SD card interaction based on SD -> Files sketch
 * Application expects SD card to be write/readable
 * and not contain a file named highscoreFileName(see variable)
 */

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <stdio.h>
#include <stdlib.h>
#include <SD.h>

#define TFT_CS    10
#define TFT_RST    8
#define TFT_DC     9

#define SD_CS      4

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

#define TFT_SCLK 13
#define TFT_MOSI 11

// Config pins
const int tiltPin = 3;
const int pushPin = 2;
const int joystickXPin = A4;

// Config screen
const uint16_t backgroundColor = ST7735_BLACK;

const unsigned int counterTextSize = 3;
const unsigned int headerTextSize = 2;
const unsigned int commandTextSize = 2;
const unsigned int gameOverTextSize = 2;
const unsigned int simonSaysTextSize = 2;

const unsigned int counterOffsetX = 0;
const unsigned int counterOffsetY = 0;
const unsigned int simonSaysOffsetX = 0;
const unsigned int simonSaysOffsetY = counterTextSize*8 + simonSaysTextSize;
const unsigned int commandOffsetX = 0;
const unsigned int commandOffsetY = simonSaysOffsetY + simonSaysTextSize*8 + commandTextSize;
const unsigned int gameOverOffsetX = 0;
const unsigned int gameOverOffsetY = 0;

// Game config
const int chanceOfNotSimonSays = 5; // One in n
const int speedUpEvery = 5; // Speed up game every n score
const int reduceTimeBy = 20; // Reduce thinking time by n millis
const int minimumTimeToThink = 500; // Thinking time should not be less than this (millis)
const int disabledTasks[] = {0}; // Handy for development. Note: At least two tasks have to be enabled!
const int simonNotSaysTime = 1000; // Time in millis a non simon says command takes to complete.

// Variables
unsigned long commandStartTime;
unsigned long timeToCompleteCommand = 4*1000;
const char *pCurrentCommand;
unsigned int totalNumberOfTasks = 4;
unsigned int currentTask = totalNumberOfTasks + 1;
char *gameOverString = "GAME OVER!\nFinal Score:\n%u";
char *finalGameOverString = (char *) malloc(sizeof(unsigned int) + sizeof(gameOverString));
unsigned int score;
// Quote "Arduino sd card notes": FAT file systems have a limitation when it comes to naming conventions. You must use the 8.3 format, so that file names look like “NAME001.EXT”, where “NAME001” is an 8 character or fewer string, and “EXT” is a 3 character extension. People commonly use the extensions .TXT and .LOG. It is possible to have a shorter file name (for example, mydata.txt, or time.log), but you cannot use longer file names.
// Took me only one hour to figure that out.
char *highscoreFileName = "topscore";
bool gameOver;
bool gameOverLogicComplete;
bool simonSays;
bool prevSimon;

void setup(void) {
  Serial.begin(9600);

  SD.begin(SD_CS);

  // Pin modes
  pinMode(tiltPin, INPUT);
  pinMode(pushPin, INPUT);
  pinMode(joystickXPin, INPUT);
  pinMode(TFT_CS, OUTPUT);

  randomSeed(analogRead(0));
  tft.initR(INITR_BLACKTAB);

  resetGame();
}

void resetGame() {
  newTask();
  score = 0;
  gameOver = false;
  gameOverLogicComplete = false;
  simonSays = true;
  prevSimon = false;
  initScreen();
}

void loop() {
  if (!gameOver) {
    gameOver = printCounter() <= 0;

    if (gameOver && !simonSays) {
      gameOver = false;
      score++;
      newTask();
    }
    else if (taskComplete()) {
      if (!simonSays) {
        gameOver = true;
        return;
      }
      score++;
      newTask();
    }

    printCommand();
  }
  else {
    doGameOverLogic();
  }
}

char counterBuffer[4];
char counterOnScreen[4];
/*
 * Prints counter/100 to screen using a double buffer
 * returns time left in milliseconds
 */
long printCounter() {
  // Make game go faster and faster and faster and faster
  long reducedTime = timeToCompleteCommand - ((score - (score % speedUpEvery)) / speedUpEvery * reduceTimeBy);

  if (!simonSays) {
    reducedTime = simonNotSaysTime;
  }
  else if (reducedTime < minimumTimeToThink) {
    reducedTime = minimumTimeToThink;
  }

  long timeLeft = reducedTime - (millis() - commandStartTime);

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
  tft.fillScreen(backgroundColor);
  tft.setRotation(3);
}

void printSimonSays() {
  if (prevSimon != simonSays) {
    prevSimon = simonSays;
    tft.setTextSize(simonSaysTextSize);
    tft.setCursor(simonSaysOffsetX, simonSaysOffsetY);
    tft.setTextColor(simonSays ? ST7735_WHITE : ST7735_BLACK);
    tft.print("Simon says:");
  }
}

const char *pPrevCommand;
void printCommand() {
  printSimonSays();
  if (pPrevCommand != pCurrentCommand) {
    tft.setTextSize(commandTextSize);
    tft.setTextWrap(true);

    repaintCommand(backgroundColor);
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
 * Inits a new task, changes command on screen and resets command counter
 */
void newTask() {
  int nextTask;
  // Make sure new task is different from last
  do {
    nextTask = random(totalNumberOfTasks);
    // Deactivation logic
    for (int i = 0; i < sizeof(disabledTasks); i++) {
      if(disabledTasks[i] == nextTask) {
        nextTask = currentTask;
        break;
      }
    }
  }
  while (nextTask == currentTask);

  simonSays = random(chanceOfNotSimonSays) != 0;

  setCommand(nextTask);
  currentTask = nextTask;
  commandStartTime = millis();
}

// Game over logic
void doGameOverLogic() {
  if (!gameOverLogicComplete) {
    tft.fillScreen(backgroundColor);
    tft.setTextSize(gameOverTextSize);
    sprintf(finalGameOverString, gameOverString, score);
    tft.setCursor(gameOverOffsetX,gameOverOffsetY);
    tft.println(finalGameOverString);

    // Higscore logic
    uint16_t highscore = getHighScore();
    //    tft.print("\n");
    if (score > highscore) {
      tft.println("New personal best!");
      setHighscore(score);
    }
    else {
      tft.println("Highscore:");
      tft.println(getHighScore());
    }
    tft.println("Press\njoystick for\nnew game");

    gameOverLogicComplete = true;
  }
  if (pushComplete()) {
    resetGame();
  }
}

unsigned int getHighScore() {
  if (!SD.exists(highscoreFileName)) {
    Serial.println("File not found!");
    return 0;
  }
  File highscoreFile = SD.open(highscoreFileName, FILE_READ);
  unsigned int highscore = 0;
  for (int i = 0; i < sizeof(int); i++) {
    highscore = highscore << 8;
    highscore += highscoreFile.read();
  }
  highscoreFile.close();
  Serial.print("Highscore from file: ");
  Serial.println(highscore);

  return highscore;
}

void setHighscore(uint16_t newHighscore) {
  if (SD.exists(highscoreFileName)) {
    SD.remove(highscoreFileName);
    Serial.println("File removed");
  }
  File highscoreFile = SD.open(highscoreFileName, FILE_WRITE);
  if (!highscoreFile) {
    Serial.print("Could not create file: ");
    Serial.println(highscoreFileName);
  }
  // Split int to two bytes
  byte b1 = newHighscore >> 8;
  byte b2 = newHighscore;
  highscoreFile.write(b1);
  highscoreFile.write(b2);
  highscoreFile.close();
  Serial.print("Set new highscore: ");
  Serial.println(newHighscore);
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
  case 2:
    pCurrentCommand = "Push up!";
    break;
  case 3:
    pCurrentCommand = "Push down!";
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
  case 2:
    return pushUpComplete();
    break;
  case 3:
    return pushDownComplete();
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
    }
    else {
      pushState2 = currentState;
    }
  }
  else {
    pushState1 = currentState;
  }
  return false;
}

bool pushUpComplete() {
  return analogRead(joystickXPin) >= 800;
}

bool pushDownComplete() {
  return analogRead(joystickXPin) <= 200;
}
