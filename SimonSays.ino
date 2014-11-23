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
const int joystickYPin = A5;

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
// Handy for development. Note: At least two tasks have to be enabled!
const int disabledTasks[] = {0};
const int simonNotSaysTime = 1000; // Time in millis a non simon says command takes to complete.

// Variables
unsigned long commandStartTime;
unsigned long timeToCompleteCommand = 4*1000;
const char *pCurrentCommand;
unsigned int totalNumberOfTasks = 6;
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
// Keep strings lower than 14 chars
char *simonCommands[] = {"Simon says", "Simon insists"};
char *simonLies[] = {"Simon lies", "", "Simon sleeps"};
const char *currentSimonSays, *prevSimonSays;

void setup(void) {
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
  prevSimonSays = "";
  initScreen();
}

void loop() {
  if (!gameOver) {
    gameOver = printCounter() <= 0;

    if (wrongTaskComplete()) {
      gameOver = true;
      return;
    }
    else if (gameOver && !simonSays) {
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
  if (currentSimonSays != prevSimonSays) {
    tft.setTextSize(simonSaysTextSize);
    repaintSimonSays(ST7735_BLACK);
    prevSimonSays = currentSimonSays;
    repaintSimonSays(ST7735_WHITE);
  }
}

// Used by printSimonSays
void repaintSimonSays(uint16_t color) {
  tft.setCursor(simonSaysOffsetX, simonSaysOffsetY);
  tft.setTextColor(color);
  tft.print(prevSimonSays);
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
int prevTask;
void newTask() {
  prevTask = currentTask;
  // Make sure new task is different from last
  do {
    currentTask = random(totalNumberOfTasks);
    // Deactivation logic
    if (isTaskDisabled(currentTask)) {
      currentTask = prevTask;
    }
  }
  while (prevTask == currentTask);

  simonSays = random(chanceOfNotSimonSays) != 0;

  if (simonSays) {
    currentSimonSays = simonCommands[random(sizeof(simonCommands) / sizeof( char *))];
  } else {
    currentSimonSays = simonLies[random(sizeof(simonLies) / sizeof( char *))];
  }

  setCommand(currentTask);
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
    return 0;
  }
  File highscoreFile = SD.open(highscoreFileName, FILE_READ);
  unsigned int highscore = 0;
  for (int i = 0; i < sizeof(int); i++) {
    highscore = highscore << 8;
    highscore += highscoreFile.read();
  }
  highscoreFile.close();

  return highscore;
}

void setHighscore(uint16_t newHighscore) {
  if (SD.exists(highscoreFileName)) {
    SD.remove(highscoreFileName);
  }
  File highscoreFile = SD.open(highscoreFileName, FILE_WRITE);
  // Split int to two bytes
  byte b1 = newHighscore >> 8;
  byte b2 = newHighscore;
  highscoreFile.write(b1);
  highscoreFile.write(b2);
  highscoreFile.close();
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
  case 4:
    pCurrentCommand = "Push right!";
    break;
  case 5:
    pCurrentCommand = "Push left!";
    break;
  default:
    pCurrentCommand = "Command not implemented";
    break;
  }
}

bool taskComplete() {
  return taskComplete(currentTask);
}

bool taskComplete(int task) {

  switch (task) {
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
  case 4:
    return pushRightComplete();
    break;
  case 5:
    return pushLeftComplete();
    break;
  }
  return false;
}

/*
 * Checks if task is complete.
 * Ignores previous task
 */
bool wrongTaskComplete() {
  for (int i = 0; i < totalNumberOfTasks; i++) {
    if (i != currentTask && i != prevTask) {
      if (!isTaskDisabled(i)) {
        if (taskComplete(i)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool isTaskDisabled(int taskNum) {
  for (int i = 0; i < sizeof(disabledTasks); i++) {
    if(disabledTasks[i] == taskNum) {
      return true;
    }
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

bool pushRightComplete() {
  return analogRead(joystickYPin) >= 800;
}

bool pushLeftComplete() {
  return analogRead(joystickYPin) <= 200;
}
