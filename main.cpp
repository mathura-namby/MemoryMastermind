#include "DebouncedInterrupt.h"
#include "LCD_DISCO_F429ZI.h"
#include "TS_DISCO_F429ZI.h"
#include "mbed.h"
#include <cstdint>
#include <time.h>
#include <vector>

#define SDA_PIN PC_9
#define SCL_PIN PA_8
#define EEPROM_ADDR 0xA0

LCD_DISCO_F429ZI LCD;
TS_DISCO_F429ZI TS;

DigitalOut MOSI(PE_6);
DigitalOut SSEL(PE_4);
DigitalOut SCLK(PE_2);

I2C i2c(SDA_PIN, SCL_PIN);

InterruptIn userButton(BUTTON1);
DebouncedInterrupt ext_up(PA_7);
DebouncedInterrupt ext_down(PA_6);

Ticker tick1;

bool ButtonPressed = false;
bool cleared = true;
bool generated = false;
bool displayed = false;
bool attached = false;
int LastLvl;
int BestLvl;
int CurLvl = 1;
int val = 0;
int size = 3;
float speed = 0.5;
float minSpeed = 0.1;
float maxSpeed = 2.5;
float incrSpeed = 0.1;
int randNum;
int incr = 1;
vector<int> randSeq;
vector<int> playerSeq;
int width = 210;
int height = 240;
int pos[2][3] = {{44, 114, 184}, {100, 180, 260}};
int pad[2][4] = {{15, 85, 155, 225}, {65, 145, 225, 305}};
uint8_t ascii[9] = {49, 50, 51, 52, 53, 54, 55, 56, 57};
int value[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
int digits[10][8] = {{0, 1, 1, 1, 1, 1, 1, 0}, {0, 0, 1, 1, 0, 0, 0, 0},
                     {0, 1, 1, 0, 1, 1, 0, 1}, {0, 1, 1, 1, 1, 0, 0, 1},
                     {0, 0, 1, 1, 0, 0, 1, 1}, {0, 1, 0, 1, 1, 0, 1, 1},
                     {0, 1, 0, 1, 1, 1, 1, 1}, {0, 1, 1, 1, 0, 0, 0, 0},
                     {0, 1, 1, 1, 1, 1, 1, 1}, {0, 1, 1, 1, 1, 0, 1, 1}};

void WriteEEPROM(int address, unsigned int eeaddress, char *data, int size);
void ReadEEPROM(int address, unsigned int eeaddress, char *data, int size);
void onUserButton();
void StartMenu();
void PinPad();
void PlayDisplay();
void GameOver();
void GameReset();
void NextLvl();
void GenSeq();
void DisplayDigit();
void Blink();
void DisplaySeq();
void Touched();
void SpeedUp();
void SlowDown();

void WriteEEPROM(int address, unsigned int eeaddress, char *data, int size) {
  char i2cBuffer[size + 2];
  i2cBuffer[0] = (unsigned char)(eeaddress >> 8);   // MSB
  i2cBuffer[1] = (unsigned char)(eeaddress & 0xFF); // LSB

  for (int i = 0; i < size; i++) {
    i2cBuffer[i + 2] = data[i];
  }

  int result = i2c.write(address, i2cBuffer, size + 2, false);
  thread_sleep_for(6);
}

void ReadEEPROM(int address, unsigned int eeaddress, char *data, int size) {
  char i2cBuffer[2];
  i2cBuffer[0] = (unsigned char)(eeaddress >> 8);   // MSB
  i2cBuffer[1] = (unsigned char)(eeaddress & 0xFF); // LSB

  // Reset eeprom pointer address
  int result = i2c.write(address, i2cBuffer, 2, false);
  thread_sleep_for(6);

  // Read eeprom
  i2c.read(address, data, size);
  thread_sleep_for(6);
}

void onUserButton() {
  cleared = false;
  ButtonPressed = true;
}

void StartMenu() {
  LCD.SetFont(&Font16);
  LCD.SetTextColor(LCD_COLOR_MAGENTA);
  LCD.DisplayStringAt(0, 100, (uint8_t *)"MEMORY MASTERMIND:", CENTER_MODE);
  LCD.DisplayStringAt(0, 120, (uint8_t *)"Sequence Challenge", CENTER_MODE);
  LCD.SetTextColor(LCD_COLOR_BLACK);
  char readLast[10];
  char readBest[10];
  ReadEEPROM(EEPROM_ADDR, 0, readLast, sizeof(readLast));
  ReadEEPROM(EEPROM_ADDR, 11, readBest, sizeof(readBest));
  uint8_t text[100];
  sprintf((char *)text, "Lastest Level: %s", readLast);
  LCD.DisplayStringAt(0, 160, (uint8_t *)&text, CENTER_MODE);
  sprintf((char *)text, "Best Level: %s", readBest);
  LCD.DisplayStringAt(0, 180, (uint8_t *)&text, CENTER_MODE);
  sprintf((char *)text, "Speed: %.1fs", speed);
  LCD.DisplayStringAt(0, 270, (uint8_t *)&text, CENTER_MODE);
}

void PinPad() {
  LCD.SetTextColor(LCD_COLOR_BLACK);
  LCD.DrawRect(pad[0][0], pad[1][0], width, height);
  LCD.DrawHLine(pad[0][0], pad[1][1], width);
  LCD.DrawHLine(pad[0][0], pad[1][2], width);
  LCD.DrawVLine(pad[0][1], pad[1][0], height);
  LCD.DrawVLine(pad[0][2], pad[1][0], height);
  LCD.SetFont(&Font24);
  int a = 0;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      LCD.DisplayChar(pos[0][j], pos[1][i], ascii[a]);
      a++;
    }
  }
}

void PlayDisplay() {
  LCD.SetFont(&Font20);
  LCD.SetTextColor(LCD_COLOR_MAGENTA);
  uint8_t text[100];
  sprintf((char *)text, "LEVEL %d", CurLvl);
  LCD.DisplayStringAt(0, 30, (uint8_t *)text, CENTER_MODE);
  PinPad();
}

void GameOver() {
  LCD.Clear(LCD_COLOR_WHITE);
  LCD.SetFont(&Font24);
  LCD.SetTextColor(LCD_COLOR_RED);
  LCD.DisplayStringAt(0, 160, (uint8_t *)"GAME OVER", CENTER_MODE);
  thread_sleep_for(3000);
}

void GameReset() {
  Blink();
  ButtonPressed = false;
  cleared = false;
  generated = false;
  displayed = false;
  playerSeq.clear();
  randSeq.clear();
  val = 0;
  size = 3;
  speed = 0.5;
  LastLvl = CurLvl - 1;
  char readBest[10];
  ReadEEPROM(EEPROM_ADDR, 11, readBest, sizeof(readBest));
  BestLvl = atoi(readBest);
  if (LastLvl > BestLvl) {
    BestLvl = LastLvl;
  }
  char Last[10];
  char Best[10];
  sprintf(Last, "%d", LastLvl);
  sprintf(Best, "%d", BestLvl);
  WriteEEPROM(EEPROM_ADDR, 0, Last, sizeof(Last));
  WriteEEPROM(EEPROM_ADDR, 11, Best, sizeof(Best));
  printf("Last: %s\n", Last);
  printf("Best: %s\n", Best);
  CurLvl = 1;
}

void NextLvl() {
  CurLvl++;
  size += incr;
  generated = false;
  displayed = false;
  playerSeq.clear();
  randSeq.clear();
  val = 0;
}

void GenSeq() {
  srand(time(NULL));
  for (int i = 0; i < size; i++) {
    randNum = rand() % 9 + 1;
    randSeq.push_back(randNum);
    printf("%d", randNum);
  }
  generated = true;
  printf("\ngenerated\n");
}

void DisplayDigit(int digit) {
  SSEL.write(0);
  for (int i = 7; i >= 0; i--) {
    SCLK.write(0);
    MOSI.write(digits[digit][i]);
    SCLK.write(1);
  }
  SSEL.write(1);
}

void Blink() {
  SSEL.write(0);
  for (int j = 7; j >= 0; j--) {
    SCLK.write(0);
    MOSI.write(0);
    SCLK.write(1);
  }
  SSEL.write(1);
}

void DisplaySeq() {
  static int i = 0;
  static bool state = false;
  state = !state;
  if (i < randSeq.size()) {
    if (state) {
      DisplayDigit(randSeq[i]);
      i++;
    } else if (!state) {
      Blink();
    }
  } else {
    Blink();
    displayed = true;
    i = 0;
    tick1.detach();
    attached = false;
    state = false;
  }
}

void Touched(uint16_t tsX, uint16_t tsY) {
  LCD.SetTextColor(LCD_COLOR_ORANGE);
  int a = 0;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if ((tsX >= pad[0][j] && tsX <= pad[0][j + 1]) &&
          (tsY >= pad[1][i] && tsY <= pad[1][i + 1])) {
        LCD.DisplayChar(pos[0][j], pos[1][i], ascii[a]);
        val = value[a];
      }
      a++;
    }
  }
}

void SpeedUp() {
  if (!ButtonPressed) {
    LCD.ClearStringLine(270);
    if (speed - incrSpeed > minSpeed) {
      speed -= incrSpeed;
    }
  }
}

void SlowDown() {
  if (!ButtonPressed) {
    LCD.ClearStringLine(270);
    if (speed + incrSpeed < maxSpeed) {
      speed += incrSpeed;
    }
  }
}

// main() runs in its own thread in the OS
int main() {

  TS_StateTypeDef tsState;
  uint16_t tsX, tsY;

  userButton.fall(&onUserButton);
  ext_up.attach(&SpeedUp, IRQ_FALL);
  ext_down.attach(&SlowDown, IRQ_FALL);

  Blink();

  while (true) {
    if (!cleared) {
      LCD.Clear(LCD_COLOR_WHITE);
      cleared = true;
    }
    if (ButtonPressed) {
      if (!generated) {
        GenSeq();
      }
      if (generated && !displayed) {
        if (!attached) {
          auto duration_ms =
              std::chrono::milliseconds(static_cast<int>(speed * 1000));
          tick1.attach(&DisplaySeq, duration_ms);
          attached = true;
        }
      }
      PlayDisplay();
      TS.GetState(&tsState);
      if (tsState.TouchDetected && displayed) {
        tsX = tsState.X;
        tsY = 320 - tsState.Y;
        Touched(tsX, tsY);
        thread_sleep_for(150);
      }
      if (!tsState.TouchDetected && tsX != 0 && tsX != 0 && val != 0 &&
          displayed) {
        printf("val: %d\n", val);
        if (playerSeq.size() < size) {
          playerSeq.push_back(val);
        }
        tsX = 0;
        tsY = 0;
      }

      if (playerSeq.size() == randSeq.size() && playerSeq.size() != 0) {
        if (playerSeq == randSeq) {
          NextLvl();
        } else {
          GameOver();
          GameReset();
        }
      }
    } else {
      StartMenu();
    }
  }
}
