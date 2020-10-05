// Include the AccelStepper library:
#include <AccelStepper.h>

// Start and end buttons
const int START_BTN_PIN = 12;
const int END_BTN_PIN = 13;

// Rotary encoder YK-040
const int ENC_CLK_PIN = 6;
const int ENC_DT_PIN = 7;
const int ENC_SW_PIN = 9; // needs to be changed to 1 (for the second version)


// Define stepper motor connections and motor interface type. Motor interface type must
const int MOTOR_INTERFACE_TYPE = 1;
// Declares pins required for control of stepper motor
const int PIN_MOTOR_ENABLE = 2;
const int PIN_MOTOR_STEP = 5;
const int PIN_MOTOR_DIR = 10;

const int STEPS_PER_MOVE = 266; // needs to be adjusted after switching microstepping MS1, MS2, MS3
const int DELAY_PER_STEP = 800; // delay between steps in microseconds

#include "RTClib.h"
RTC_Millis rtc;

DateTime timeOfAction;
TimeSpan remainingTime;

TimeSpan timers[2] = {TimeSpan(0,0,0,0), TimeSpan(0,0,0,0)};
const char * const timersLabels[] = {"T1=", "T2="};
int timeSpanStep = 0; // initial value of the following timespan types for adjusting timers
const char * const timeSpanStepTypes[] = {  "+-1s", "+-15s", "+-60s"};

boolean clockwise = true; // Defines direction of rotation (flips each time rotation is triigered)

#include <StackString.hpp> // optimization of String handling, it reuses the same string and preserves memory
using namespace Stack;

#include <MD_REncoder.h>
MD_REncoder R = MD_REncoder(ENC_CLK_PIN, ENC_DT_PIN);

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
Adafruit_SSD1306 myDisplay[] = {display};

byte startedTimer = false;
byte activeAction = 0;
byte timerModified = 0;
int encButton = 0;
int endButton = 0;
boolean isCheckEndButton = false;

void setup() {
  Serial.begin(9600);
  setupDisplay();
  setupButtons();
  setupEncoder();
  setupStepperMotor();
  setTimeOfNextAction(0);
  updateDisplayValues();
}

StackString<21> timeStr = StackString<21>("");
char* showTimeSpan(const char* txt, const TimeSpan& ts) {
  timeStr.clear();
  timeStr.append(txt);
  timeStr.append(ts.hours());
  timeStr.append(":");
  timeStr.append(ts.minutes());
  timeStr.append(":");
  timeStr.append(ts.seconds());
  return timeStr.c_str();
}

void shiftTimeSpanStep() {
  timeSpanStep = timeSpanStep + 1;
  if (timeSpanStep > 2) {
    timeSpanStep = 0;
  }
}

void checkEncoderButton() {
  encButton = digitalRead(ENC_SW_PIN);
  if (encButton == LOW) {
    delay (10);  // To solve the The problem switch bounce
    if (digitalRead(ENC_SW_PIN) == HIGH) {
      shiftTimeSpanStep();
      updateDisplayValues();
    }
  }
}

void checkEndButton() {
  endButton = digitalRead(END_BTN_PIN);
  if (endButton == HIGH) {
    startedTimer = false;
    delay (10);  // To solve the The problem switch bounce
    if (digitalRead(END_BTN_PIN) == LOW) {
      isCheckEndButton = true;
      timerModified = (timerModified == 0 ? 1 : 0);
      updateDisplayValues();
    }
  }
}

unsigned int cycleCounter = 0;
void loop() {
  if (digitalRead(START_BTN_PIN) == HIGH) {
    startedTimer = true;
    updateDisplayValues();
    setTimeOfNextAction(0);
  }
  checkEndButton();
  
  if (startedTimer) {
    if (timeOfAction - TimeSpan(0,0,0,1) >= rtc.now()) {
      TimeSpan remainingTime = timeOfAction - rtc.now();
      oledDisplay(0, activeAction * 2 + 1, showTimeSpan("" , remainingTime));
    } else {
      rotate();
      setTimeOfNextAction(-1);
    }
  } else {
    checkEncoderButton();
    uint8_t readValue = R.read();
    
    if (readValue) {
      if (timerModified == 0) {
        processEncoderChanges(readValue, timerModified);
        oledDisplay(0,1,showTimeSpan(">", timers[timerModified]));
      }
      else {
        processEncoderChanges(readValue, timerModified);
        oledDisplay(0,3,showTimeSpan(">", timers[timerModified]));
      }
    }
  }
}

void processEncoderChanges(uint8_t readValue, int index) {
  if (readValue == DIR_CW) {
    timers[index] = timers[index] + getTimeSpanStep();
  } else if (isTimeSpanPositive(timers[index] - getTimeSpanStep())) {
    timers[index] = timers[index] - getTimeSpanStep();
  }
}

StackString<10> stepTypeStr = StackString<10>("");
String getTimeSpanStepType() {
  stepTypeStr.clear();
  stepTypeStr.append(timersLabels[timerModified]);
  stepTypeStr.append(" ");
  stepTypeStr.append(timeSpanStepTypes[timeSpanStep]);
  return stepTypeStr.c_str();
}

// the trigger is supposed to fire right after
DateTime calculateFutureTrigger(TimeSpan time) {
  return rtc.now() + time;
}

boolean isTimeSpanPositive(TimeSpan timeSpan) {
  return (timeSpan.hours() >= 0 and timeSpan.minutes() >= 0 and timeSpan.seconds() >= 0);
}

void setTimeOfNextAction(TimeSpan time) {
  if (isTimeSpanPositive(time)) {
    timeOfAction = calculateFutureTrigger(time);
  } else if (activeAction == 0) {
    activeAction = 1;
    timeOfAction = calculateFutureTrigger(timers[1]);
  } else {
    activeAction = 0;
    timeOfAction = calculateFutureTrigger(timers[0]);
  }
}

TimeSpan getTimeSpanStep() {
  switch (timeSpanStep) {
  case 0:
    return TimeSpan(0,0,0,1);
    break;
  case 1:
    return TimeSpan(0,0,0,15);
    break;
  case 2:
    return TimeSpan(0,0,1,0);
    break;
  default:
    timeSpanStep = 0;
    return getTimeSpanStep();
    break;
}
}

void rotate() {
  cycleCounter++;
  if (clockwise) {
    digitalWrite(PIN_MOTOR_DIR, HIGH); // Set Dir high
    clockwise = false;
  } else {
    digitalWrite(PIN_MOTOR_DIR, LOW); // Set Dir low
    clockwise = true;
  }
  oledDisplay(0, activeAction * 2 + 1, activeAction == 0 ? F("+90 deg") : F("-90 deg"));
  rotateStepper();
  oledDisplay(0, activeAction * 2 + 1, F(""));
//  oledDisplay(1, 1, String(cycleCounter));
}

void rotateStepper() {
  for(int x = 0; x < STEPS_PER_MOVE; x++)
  {
    digitalWrite(PIN_MOTOR_STEP, HIGH); // Output high
    delayMicroseconds(DELAY_PER_STEP); // Wait
    digitalWrite(PIN_MOTOR_STEP, LOW); // Output low
    delayMicroseconds(DELAY_PER_STEP); // Wait
    checkEndButton();
  }
}

char displayLine[16];
void oledDisplay(int number, int line, String text) {
  text.toCharArray(displayLine, 16);
  myDisplay[number].setTextSize(2);      // Normal 1:1 pixel scale
  myDisplay[number].cp437(true);         // Use full 256 char 'Code Page 437' font
  myDisplay[number].setCursor(0, SCREEN_HEIGHT/4*line);     // refreshed part of the screen
  myDisplay[number].setTextColor(WHITE, BLACK);
  myDisplay[number].print("          ");
  myDisplay[number].setCursor(0, SCREEN_HEIGHT/4*line);     // Start at top-left corner
  myDisplay[number].setTextColor(SSD1306_WHITE); // Draw white text
  myDisplay[number].print(displayLine);
  myDisplay[number].display();
}

void updateDisplayValues() {
  oledDisplay(0, 0, showTimeSpan(timersLabels[0], timers[0]));
  oledDisplay(0, 1, F(""));
  oledDisplay(0, 2, showTimeSpan(timersLabels[1], timers[1]));
  oledDisplay(0, 3, F(""));
  if (!startedTimer) {
    if (timerModified == 0) {
      oledDisplay(0, 0, getTimeSpanStepType());
      oledDisplay(0, 1, showTimeSpan(">", timers[0]));
      oledDisplay(0, 2, showTimeSpan(timersLabels[1], timers[1]));
    } else {
      oledDisplay(0, 0, showTimeSpan(timersLabels[0], timers[0]));
      oledDisplay(0, 2, getTimeSpanStepType());
      oledDisplay(0, 3, showTimeSpan(">", timers[1]));
    }
  }
}

void setupButtons() {
  pinMode(START_BTN_PIN, INPUT);
  pinMode(END_BTN_PIN, INPUT);
}

void setupDisplay() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!myDisplay[0].begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    Serial.println(F("SSD1306 #1 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
//  if(!myDisplay[1].begin(SSD1306_SWITCHCAPVCC, 0x3D, false, false)) { // Address 0x3D for 128x64
//    Serial.println(F("SSD1306 #2 allocation failed"));
//    for(;;); // Don't proceed, loop forever
//  }
  // Clear the buffer
  myDisplay[0].clearDisplay();
//  myDisplay[1].clearDisplay();
}

void setupEncoder() {
  pinMode(ENC_SW_PIN, INPUT_PULLUP);
  R.begin();
}

void setupStepperMotor() {
  clockwise = true; // sets an initial direction of rotation
  pinMode(PIN_MOTOR_ENABLE, OUTPUT);
  pinMode(PIN_MOTOR_STEP, OUTPUT);
  pinMode(PIN_MOTOR_DIR, OUTPUT);
  digitalWrite(PIN_MOTOR_ENABLE, LOW); // Set Enable low
  rotate(); // initial rotation may hit the end or it needs to involve an end stop switch
}
