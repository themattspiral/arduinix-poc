/**
 * ============================
 *    Hardware Definitions
 * ============================
 */

// ArduiNIX controller 0 (SN74141/K155ID1)
const int PIN_CATHODE_0_A = 2;                
const int PIN_CATHODE_0_B = 3;
const int PIN_CATHODE_0_C = 4;
const int PIN_CATHODE_0_D = 5;

// ArduiNIX controller 1 (SN74141/K155ID1)
const int PIN_CATHODE_1_A = 6;                
const int PIN_CATHODE_1_B = 7;
const int PIN_CATHODE_1_C = 8;
const int PIN_CATHODE_1_D = 9;

// ArduiNIX anode pins
const int PIN_ANODE_1 = 10;
const int PIN_ANODE_2 = 11;
const int PIN_ANODE_3 = 12;
const int PIN_ANODE_4 = 13;

// button pins
const int PIN_BUTTON_RIGHT = A0;
const int PIN_BUTTON_LEFT = A1;
const int PIN_BUTTON_RIGHT_LED = A2;
const int PIN_BUTTON_LEFT_LED = A3;
const int PIN_BUTTON_UTILITY = A4;

// 6 tubes, each wired to a unique combination of anode pin and cathode controller
// TODO: IMPROVE THIS (no true/false)
const int TUBE_COUNT = 6;
const int TUBE_ANODES[] = {1, 1, 2, 2, 3, 3};
const bool TUBE_CATHODE_CTRL_0[] = {false, true, false, true, false, true};

// SN74141/K155ID1 controllers are BCD-to-decimal, and are wired such that
// giving them 0-9 in BCD will represent themselves as Nixie tube decimals, 
// and anything else is unconnected (therefore blank)
const int BLANK = 15;

// behavior constants
const int IDLE_DELAY_MS = 2;
const int MUX_SINGLE_TUBE_DELAY_US = 3000;  // 300µs - 3000µs is ideal (<300 ghosts, >3000 flickers)
const int DEMO_STEP_DURATION_MS = 150;      // how fast to count up
const int BUTTON_DEBOUNCE_DELAY_MS = 20;


/**
 * ============================
 *    Runtime State
 * ============================
 */

// nixie tube demo state 🚥🚥
unsigned long lastDemoStepTimestampMs = 0UL;
int basicDemoTubeValue = 0;
int muxDemoTubeValues[] = {0, 1, 2, 3, 4, 5};

// button state 🔘🔘
int rightButtonLastVal = HIGH;
int leftButtonLastVal = HIGH;
int utilityButtonLastVal = HIGH;
int rightButtonVal = HIGH;
int leftButtonVal = HIGH;
int utilityButtonVal = HIGH;
unsigned long rightButtonLastDebounceMS = 0UL;
unsigned long leftButtonLastDebounceMS = 0UL;
unsigned long utilityButtonLastDebounceMS = 0UL;

// chess clock state ♟⏲⏲♟
bool leftPlayersTurn = false;
bool clockRunning = false;
unsigned long turnStartTimestampMS = 0UL;

typedef struct {
  int displayTubeValues[];
  unsigned long turnLimitMS;
} timerOption;

timerOption TURN_TIMER_OPTIONS[] = {
  { { 2, 4, BLANK, BLANK, BLANK, BLANK }, 86401000UL },
  { { 0, 1, BLANK, BLANK, BLANK, BLANK }, 3600000UL },
  { { BLANK, BLANK, 3, 0, BLANK, BLANK }, 1800000UL }
};

/**
 * ============================
 *    INITIAL SETUP (ON BOOT)
 * ============================
 */
void setup() 
{
  // Serial.begin(9600);
  Serial.begin(115200);

  pinMode(PIN_ANODE_1, OUTPUT);
  pinMode(PIN_ANODE_2, OUTPUT);
  pinMode(PIN_ANODE_3, OUTPUT);
  pinMode(PIN_ANODE_4, OUTPUT);
  
  pinMode(PIN_CATHODE_0_A, OUTPUT);
  pinMode(PIN_CATHODE_0_B, OUTPUT);
  pinMode(PIN_CATHODE_0_C, OUTPUT);
  pinMode(PIN_CATHODE_0_D, OUTPUT);
  
  pinMode(PIN_CATHODE_1_A, OUTPUT);
  pinMode(PIN_CATHODE_1_B, OUTPUT);
  pinMode(PIN_CATHODE_1_C, OUTPUT);
  pinMode(PIN_CATHODE_1_D, OUTPUT);
  
  digitalWrite(PIN_ANODE_1, LOW);
  digitalWrite(PIN_ANODE_2, LOW);
  digitalWrite(PIN_ANODE_3, LOW);
  digitalWrite(PIN_ANODE_4, LOW);
  
  setCathode(true, BLANK);
  setCathode(false, BLANK);

  // use analog inputs as digital inputs for buttons
  pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_LEFT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_UTILITY, INPUT_PULLUP);

  // use analog inputs as digital outputs for button LEDs
  pinMode(PIN_BUTTON_RIGHT_LED, OUTPUT);
  pinMode(PIN_BUTTON_LEFT_LED, OUTPUT);
  digitalWrite(PIN_BUTTON_RIGHT_LED, LOW);
  digitalWrite(PIN_BUTTON_LEFT_LED, LOW);
}


/**
 * ============================
 *    Internal Functions
 * ============================
 */

void setCathode(boolean ctrl0, int displayNumber) {
  byte a, b, c, d;
  d = c = b = a = 1;
  
  // binary representation
  switch(displayNumber) {
    case 0: d=0; c=0; b=0; a=0; break;
    case 1: d=0; c=0; b=0; a=1; break;
    case 2: d=0; c=0; b=1; a=0; break;
    case 3: d=0; c=0; b=1; a=1; break;
    case 4: d=0; c=1; b=0; a=0; break;
    case 5: d=0; c=1; b=0; a=1; break;
    case 6: d=0; c=1; b=1; a=0; break;
    case 7: d=0; c=1; b=1; a=1; break;
    case 8: d=1; c=0; b=0; a=0; break;
    case 9: d=1; c=0; b=0; a=1; break;
    default: d=1; c=1; b=1; a=1;
  }  
  
  // write to output pins
  if (ctrl0) {
    // controller 0
    digitalWrite(PIN_CATHODE_0_D, d);
    digitalWrite(PIN_CATHODE_0_C, c);
    digitalWrite(PIN_CATHODE_0_B, b);
    digitalWrite(PIN_CATHODE_0_A, a);
  } else {
    // controller 1
    digitalWrite(PIN_CATHODE_1_D, d);
    digitalWrite(PIN_CATHODE_1_C, c);
    digitalWrite(PIN_CATHODE_1_B, b);
    digitalWrite(PIN_CATHODE_1_A, a);
  }
}

void displayOnTube(int tubeIndex, int displayVal) {
  int anode = TUBE_ANODES[tubeIndex];
  bool cathodeCtrl0 = TUBE_CATHODE_CTRL_0[tubeIndex];
  
  switch(anode) {
    case 1:
      digitalWrite(PIN_ANODE_2, LOW);
      digitalWrite(PIN_ANODE_3, LOW);
      digitalWrite(PIN_ANODE_1, displayVal == BLANK ? LOW : HIGH);
      break;
    case 2:
      digitalWrite(PIN_ANODE_1, LOW);
      digitalWrite(PIN_ANODE_3, LOW);
      digitalWrite(PIN_ANODE_2, displayVal == BLANK ? LOW : HIGH);
      break;
    case 3:
      digitalWrite(PIN_ANODE_1, LOW);
      digitalWrite(PIN_ANODE_2, LOW);
      digitalWrite(PIN_ANODE_3, displayVal == BLANK ? LOW : HIGH);
      break;
  }
  
  setCathode(!cathodeCtrl0, BLANK);
  setCathode(cathodeCtrl0, displayVal);
}

void setButtonLEDs(bool left, bool right) {
  digitalWrite(PIN_BUTTON_LEFT_LED, left ? HIGH : LOW);
  digitalWrite(PIN_BUTTON_RIGHT_LED, right ? HIGH : LOW);
}

// basic count up - same value on all tubes
void loopCountBasic(unsigned long loopNow) {
  if (loopNow - lastDemoStepTimestampMs > DEMO_STEP_DURATION_MS) {
    lastDemoStepTimestampMs = loopNow;

    if (basicDemoTubeValue == 9) {
      basicDemoTubeValue = 0;
    } else {
      basicDemoTubeValue++;
    }
        
    digitalWrite(PIN_ANODE_1, HIGH);
    digitalWrite(PIN_ANODE_2, HIGH);
    digitalWrite(PIN_ANODE_3, HIGH);

    setCathode(true, basicDemoTubeValue);
    setCathode(false, basicDemoTubeValue);
  }

  delay(IDLE_DELAY_MS);
}

// multiplex couting up different values on each tube
void loopCountMultiplexed(unsigned long loopNow) {
  for (int i = 0; i < TUBE_COUNT; i++) {
    displayOnTube(i, muxDemoTubeValues[i]);
    delayMicroseconds(MUX_SINGLE_TUBE_DELAY_US);
  }
  
  if (loopNow - lastDemoStepTimestampMs > DEMO_STEP_DURATION_MS) {
    lastDemoStepTimestampMs = loopNow;

    for (int i = 0; i < TUBE_COUNT; i++) {
      if (muxDemoTubeValues[i] == 9) {
        muxDemoTubeValues[i] = 0;
      } else {
        muxDemoTubeValues[i]++;
      }
    }
  }
}

void startClock(unsigned long loopNow) {
  turnStartTimestampMS = loopNow;
  clockRunning = true;
  setButtonLEDs(leftPlayersTurn, !leftPlayersTurn);
}

void switchTurns(unsigned long loopNow) {
  leftPlayersTurn = !leftPlayersTurn;
  turnStartTimestampMS = loopNow;
  setButtonLEDs(leftPlayersTurn, !leftPlayersTurn);
};

void handleRightButtonPress(unsigned long loopNow) {
  if (clockRunning && !leftPlayersTurn) {
    switchTurns(loopNow);
  } else if (!clockRunning) {
    leftPlayersTurn = false;
    startClock(loopNow);
  }
}

void handleLeftButtonPress(unsigned long loopNow) {
  if (clockRunning && leftPlayersTurn) {
    switchTurns(loopNow);
  } else if (!clockRunning) {
    leftPlayersTurn = true;
    startClock(loopNow);
  }
}

void handleUtilityButtonPress(unsigned long loopNow) {
  if (clockRunning) {
    Serial.println("utility press");
    // clockRunning = false;
  }
}

void loopCheckButtons(unsigned long loopNow) {
  int rightButtonReading = digitalRead(PIN_BUTTON_RIGHT);
  int leftButtonReading = digitalRead(PIN_BUTTON_LEFT);
  int utilityButtonReading = digitalRead(PIN_BUTTON_UTILITY);

  // handle press state change (set debounce timer)
  if (rightButtonReading != rightButtonLastVal) {
    rightButtonLastDebounceMS = loopNow;
  }
  if (leftButtonReading != leftButtonLastVal) {
    leftButtonLastDebounceMS = loopNow;
  }
  if (utilityButtonReading != utilityButtonLastVal) {
    utilityButtonLastDebounceMS = loopNow;
  }

  // check right button debounce time exceeded with un-flickering changed value
  unsigned long rightDebounceDiff = loopNow - rightButtonLastDebounceMS;
  if (rightDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && rightButtonReading != rightButtonVal) {
    rightButtonVal = rightButtonReading;

    // using internal pull-up resistor means a pressed button goes LOW
    if (rightButtonVal == LOW) {
      handleRightButtonPress(loopNow);
    }
  }

  // check left button debounce time exceeded with un-flickering changed value
  unsigned long leftDebounceDiff = loopNow - leftButtonLastDebounceMS;
  if (leftDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && leftButtonReading != leftButtonVal) {
    leftButtonVal = leftButtonReading;

    // using internal pull-up resistor means a pressed button goes LOW
    if (leftButtonVal == LOW) {
      handleLeftButtonPress(loopNow);
    }
  }

  // check utility button debounce time exceeded with un-flickering changed value
  unsigned long utilityDebounceDiff = loopNow - utilityButtonLastDebounceMS;
  if (utilityDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && utilityButtonReading != utilityButtonVal) {
    utilityButtonVal = utilityButtonReading;

    // using internal pull-up resistor means a pressed button goes LOW
    if (utilityButtonVal == LOW) {
      handleUtilityButtonPress(loopNow);
    }
  }

  rightButtonLastVal = rightButtonReading;
  leftButtonLastVal = leftButtonReading;
  utilityButtonLastVal = utilityButtonReading;
}

void displayClockTime(unsigned long turnTimeMS) {
  unsigned long elapsedSec = turnTimeMS / 1000;

  int hours = elapsedSec / 3600;
  int min = (elapsedSec % 3600) / 60;
  int sec = (elapsedSec % 3600) % 60;
  int fractionalSec = (turnTimeMS % 1000) / 10;

  int clockTubeValues[TUBE_COUNT];

  if (hours > 0) {
    clockTubeValues[0] = hours / 10;
    clockTubeValues[1] = hours % 10;
    clockTubeValues[2] = min / 10;
    clockTubeValues[3] = min % 10;
    clockTubeValues[4] = sec / 10;
    clockTubeValues[5] = sec % 10;
  } else if (min > 0) {
    clockTubeValues[0] = min / 10;
    clockTubeValues[1] = min % 10;
    clockTubeValues[2] = sec / 10;
    clockTubeValues[3] = sec % 10;
    clockTubeValues[4] = fractionalSec / 10;
    clockTubeValues[5] = fractionalSec % 10;
  } else {
    if (leftPlayersTurn) {
      clockTubeValues[0] = sec / 10;
      clockTubeValues[1] = sec % 10;
      clockTubeValues[2] = fractionalSec / 10;
      clockTubeValues[3] = fractionalSec % 10;
      clockTubeValues[4] = BLANK;
      clockTubeValues[5] = BLANK;
    } else {
      clockTubeValues[0] = BLANK;
      clockTubeValues[1] = BLANK;
      clockTubeValues[2] = sec / 10;
      clockTubeValues[3] = sec % 10;
      clockTubeValues[4] = fractionalSec / 10;
      clockTubeValues[5] = fractionalSec % 10;
    }
  }

  for (int i = 0; i < TUBE_COUNT; i++) {
    displayOnTube(i, clockTubeValues[i]);
    delayMicroseconds(MUX_SINGLE_TUBE_DELAY_US);
  }
}

void loopChessClock(unsigned long loopNow) {
  if (clockRunning) {
    unsigned long elapsedMS = loopNow - turnStartTimestampMS;
    unsigned long remainingMS = 86401000UL - elapsedMS;

    displayClockTime(remainingMS);
  } else {
    delay(IDLE_DELAY_MS);
  }
}


/**
 * ============================
 *    MAIN LOOP (CONTINUOUS)
 * ============================
 */
void loop() {
  unsigned long now = millis();
  loopCheckButtons(now);
  loopChessClock(now);
}
