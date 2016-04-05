#include <Wire.h>
#include <RTClib.h>

/**
 * arduinix-poc
 *
 * TODO
 * 
 * - define instruction format for tube display value, duration (0 = unlimited / until next instruction?), & potentially effects
 * - decide: should effects be defined at this level, or handled by higher-order code feeding more instructions?
 *    - effect: off
 *    - effect: constant on, @ constant fade % (= duty cycle)
 *    - effect: constant on, @ constant fade %, + fade out at end of duration
 *    - effect: pulse- up then down, or down then up, using specified start and stop % (fade levels), and specified fade duration & number of cycles
 * - Serial streaming of instructions
 *    - freq change
 *    - tube instruction queue
 *    - instruction parsing (parse 2 bytes -> int using methods here: http://projectsfromtech.blogspot.com/2013/09/combine-2-bytes-into-int-on-arduino.html)
 *
 * - custom map function(s)
 *   - custom map function that creates different shapes
 *   - exponential
 *   - logarithmic
 *   - integrate with fade effects
 *
 */

// rtc chip and constants
RTC_DS1307 RTC;
char DAYS_OF_THE_WEEK[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};


// Controller 0 (SN74141/K155ID1)
const byte PIN_CATHODE_0_A = 2;                
const byte PIN_CATHODE_0_B = 3;
const byte PIN_CATHODE_0_C = 4;
const byte PIN_CATHODE_0_D = 5;

// Controller 1 (SN74141/K155ID1)
const byte PIN_CATHODE_1_A = 6;                
const byte PIN_CATHODE_1_B = 7;
const byte PIN_CATHODE_1_C = 8;
const byte PIN_CATHODE_1_D = 9;

// anode pins
const byte PIN_ANODE_1 = 10;
const byte PIN_ANODE_2 = 11;
const byte PIN_ANODE_3 = 12;
const byte PIN_ANODE_4 = 13;

const byte BLANK_DISPLAY = 15;

// TUBE DEFINITIONS
// 6 tubes, each wired to a unique combination of anode pin and cathode controller
const byte TUBE_COUNT = 6;
const byte TUBE_ANODES[] = {1, 1, 2, 2, 3, 3};
const byte TUBE_CATHODE_CTRL_0[] = {false, true, false, true, false, true};

const byte FADE_LEVEL_MIN = 0;
const byte FADE_LEVEL_MAX = 6;

boolean WARMED_UP = false;

// timing
unsigned long LAST_TS = 0UL;      // last timestamp
long PWM_FADE_LENGTH_MS = 3000L;
byte PWM_FADE_UP_STATE = 1;

// 500 Hz = 1/500 * 1M us = 2000us period
// 10 Hz = 1/10 * 1M us = 100000us period
// 20 Hz = 1/10 * 1M us = 50000us period
// 62.5 Hz = 16000us period
// Experimentation shows the minimum smooth update frequency is ~60Hz

// PWM values
long FREQ_HZ = 120L;

long PERIOD_US = 0L;
long PWM_FADE_MIN_US = 0L;
long PWM_FADE_MAX_US = 0L;

long MUX_PERIOD_US = 0L;
long MUX_FADE_MIN_US = 0L;
long MUX_FADE_MAX_US = 0L;

// predfined routine display params
int tubeSeq[] = {0, 1, 2, 3, 4, 5};
int countDurationMillis = 2000;

int muxDemoStepUpDurationMillis = 6000;
int muxDemoStepUpDelayMs[] = {500, 80, 20, 2};
int muxDemoStepUpRuns[] = {4, 15, 80, 400};
int muxDemoStep = 0;

byte pwmSeqNum = 0;




/* 
 * Caluclate values needed for Pulse Width Modulation (PWM) to acheive fading effects.
 *
 * Based on the given/set frequency, calculate the appropriate value
 * for the period in microseconds, and the min and max durations (in
 * microseconds) that a tub may be switched on within each period, based 
 * on 1% min and 100% max duty cycle.
 * 
 * Also make adjusted calculations for multiplexing with PWM. Multiplexed control
 * assumes the desired requency is *per tube*, and thus overall calculations must be
 * adjusted to accomodate an overall display frequency of Desired Freq * Number of Tubes.
 */
void calculatePwmVals() {
  PERIOD_US = 1000000L / FREQ_HZ; // 1sec in us / cycles-per-sec = period in us
  
  PWM_FADE_MIN_US = PERIOD_US / 100L; // (1/100 = 0.01 = 1% duty minimum)
  PWM_FADE_MAX_US = PERIOD_US; // 100% duty maximum
  
  long adjustedFreq = FREQ_HZ * TUBE_COUNT; // for each tube to update at given freq, adjust "total" freq to scale to num of tubes
  MUX_PERIOD_US = 1000000L / adjustedFreq;
  //MUX_FADE_MIN_US = MUX_PERIOD_US / 100L; // (1/100 = 0.01 = 1% duty)
  MUX_FADE_MIN_US = 1L; // (~0% without actually being 0)
  MUX_FADE_MAX_US = MUX_PERIOD_US; // (100% duty)
  
  Serial.print("PWM Init - Frequency: ");
  Serial.print(FREQ_HZ, DEC);
  Serial.print(" - Period: ");
  Serial.print(PERIOD_US, DEC);
  Serial.print(" - PWM_FADE_MIN_US: ");
  Serial.print(PWM_FADE_MIN_US, DEC);
  Serial.print(" - PWM_FADE_MAX_US: ");
  Serial.print(PWM_FADE_MAX_US, DEC);
  Serial.println("");
  
  Serial.print("MUX tube-count adj. freq: ");
  Serial.print(adjustedFreq, DEC);
  Serial.print(" - MUX period: ");
  Serial.print(MUX_PERIOD_US, DEC);
  Serial.print(" (full ");
  Serial.print((MUX_PERIOD_US * TUBE_COUNT), DEC);
  Serial.print(") - MUX_FADE_MIN_US: ");
  Serial.print(MUX_FADE_MIN_US, DEC);
  Serial.print(" - MUX_FADE_MAX_US: ");
  Serial.print(MUX_FADE_MAX_US, DEC);
  
  Serial.println("");
}

void setup() 
{
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
  
  // initialize anodes
  digitalWrite(PIN_ANODE_1, LOW);
  digitalWrite(PIN_ANODE_2, LOW);
  digitalWrite(PIN_ANODE_3, LOW);
  digitalWrite(PIN_ANODE_4, LOW);
  
  // initialize cathodes to 15 (blank)
  setCathode(true, BLANK_DISPLAY);
  setCathode(false, BLANK_DISPLAY);
  
  //Serial.begin(115200);
  Serial.begin(57600); // need this speed for RTC???????
  
  // initialize PWM Values
  calculatePwmVals();


  // RTC setup

  // power for RTC chip
  pinMode(A3, OUTPUT);
  digitalWrite(A3, HIGH);

  // ground for RTC chip
  pinMode(A2, OUTPUT);
  digitalWrite(A2, LOW);
  
  Wire.begin();
  if (! RTC.begin()) {
    Serial.println("Couldn't find RTC");
  } else {
    Serial.println("Found RTC and begun.");
  }
 
  if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
    
    // following line sets the RTC to the date & time this sketch was compiled
    RTC.adjust(DateTime(__DATE__, __TIME__));
    Serial.println("Set time!!!!!!!");
  
  } else {
    Serial.println("RTC IS running!");
  }

  Serial.println("setup complete.");
  Serial.println();
}


void printTime() {
  DateTime now = RTC.now();
  
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(' ');
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
  
  Serial.print(" since 1970 = ");
  Serial.print(now.unixtime());
  Serial.print("s = ");
  Serial.print(now.unixtime() / 86400L);
  Serial.println("d");
//  
//  // calculate a date which is 7 days and 30 seconds into the future
//  DateTime future (now.unixtime() + 7 * 86400L + 30);
//  
//  Serial.print(" now + 7d + 30s: ");
//  Serial.print(future.year(), DEC);
//  Serial.print('/');
//  Serial.print(future.month(), DEC);
//  Serial.print('/');
//  Serial.print(future.day(), DEC);
//  Serial.print(' ');
//  Serial.print(future.hour(), DEC);
//  Serial.print(':');
//  Serial.print(future.minute(), DEC);
//  Serial.print(':');
//  Serial.print(future.second(), DEC);
//  Serial.println();
  
  Serial.println();
}


void setCathode(boolean ctrl0, byte displayNumber) {
  // 15 by default ( > 9 outputs blank)
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

void displayOnTube(byte tubeIndex, byte displayNumber, boolean exclusive) {
  byte anode = TUBE_ANODES[tubeIndex];
  boolean cathodeCtrl0 = TUBE_CATHODE_CTRL_0[tubeIndex];
  
  switch(anode) {
    case 1:
      if (exclusive) {
        digitalWrite(PIN_ANODE_2, LOW);
        digitalWrite(PIN_ANODE_3, LOW);
      }
      digitalWrite(PIN_ANODE_1, HIGH);
      break;
    case 2:
      if (exclusive) {
        digitalWrite(PIN_ANODE_1, LOW);
        digitalWrite(PIN_ANODE_3, LOW);
      }
      digitalWrite(PIN_ANODE_2, HIGH);
      break;
    case 3:
      if (exclusive) {
        digitalWrite(PIN_ANODE_1, LOW);
        digitalWrite(PIN_ANODE_2, LOW);
      }
      digitalWrite(PIN_ANODE_3, HIGH);
      break;
  }
  
  if (exclusive) {
    setCathode(!cathodeCtrl0, BLANK_DISPLAY);
  }
  setCathode(cathodeCtrl0, displayNumber);
}


void warmup() {
  digitalWrite(PIN_ANODE_1, HIGH);
  digitalWrite(PIN_ANODE_2, HIGH);
  digitalWrite(PIN_ANODE_3, HIGH);
  
  // blink '0' three times
  for (int i=0; i<2; i++) {
    setCathode(true, 0);
    setCathode(false, 0);
    delay(1500);
    setCathode(true, BLANK_DISPLAY);
    setCathode(false, BLANK_DISPLAY);
    delay(300);
  }
  
  setCathode(true, BLANK_DISPLAY);
  setCathode(false, BLANK_DISPLAY);
  delay(300);
  
  // cycle across tubes, starting with 9 down to 0
  int seqNum = 9;
  for(int i=0; i<TUBE_COUNT; i++) {
    displayOnTube(i, seqNum, true);
    delay(65);
    
    if (i == TUBE_COUNT-1 && seqNum > 0) {
      i = -1;
      seqNum--;
    }
  }
  
  setCathode(true, BLANK_DISPLAY);
  setCathode(false, BLANK_DISPLAY);
  digitalWrite(PIN_ANODE_1, HIGH);
  digitalWrite(PIN_ANODE_2, HIGH);
  digitalWrite(PIN_ANODE_3, HIGH);
  delay(300);
  
  // count 0-9 slowly, 2 times
  for (int c=0; c<2; c++) {
    for (int i=0; i<10; i++) {
      setCathode(true, i);
      setCathode(false, i);
      delay(150);
    }
  }
  
  setCathode(true, BLANK_DISPLAY);
  setCathode(false, BLANK_DISPLAY);
  delay(300);
  
  // count 0-9 quickly, 5 times
  for (int c=0; c<5; c++) {
    for (int i=0; i<10; i++) {
      setCathode(true, i);
      setCathode(false, i);
      delay(30);
    }
  }
  
  setCathode(true, BLANK_DISPLAY);
  setCathode(false, BLANK_DISPLAY);
  delay(300);
  
  digitalWrite(PIN_ANODE_1, LOW);
  digitalWrite(PIN_ANODE_2, LOW);
  digitalWrite(PIN_ANODE_3, LOW);
}


void countUp() {
  for (int seqNum=0; seqNum<10; seqNum++) {
    setCathode(true, seqNum);
    setCathode(false, seqNum);
    delay(500);
  }
}



const byte DISP_OFF = 0;
const byte DISP_CONST = 1;
const byte DISP_FADE = 2;
const byte DISP_PULSE = 3;

const byte FADE_DIR_L2H = 0;
const byte FADE_DIR_H2L = 1;

typedef struct
{
  byte displayValue;
  unsigned long lastDirectionChangeTS;
  boolean pwmFadingUp;
  unsigned long currentInstructionStartTS;
} tubeState;

tubeState STATES[TUBE_COUNT] = {
  {0, 0UL, true, 0UL},
  {1, 0UL, true, 0UL},
  {2, 0UL, true, 0UL},
  {3, 0UL, true, 0UL},
  {4, 0UL, true, 0UL},
  {5, 0UL, true, 0UL}
};


typedef struct
{
  byte displayValue;
  int displayDurationMS;
  byte displayMode;
  byte fadeLevelLow;
  byte fadeLevelHigh;
  byte fadeDirection;
  int pulseSegmentDurationMS;
} tubeInstruction;

// constant
//tubeInstruction CURRENT_INSTRUCTIONS[TUBE_COUNT] = {
//  {0, 0, 1, 0, 100, 0, 0},
//  {1, 0, 1, 0, 100, 0, 0},
//  {2, 0, 1, 0, 100, 0, 0},
//  {3, 0, 1, 0, 100, 0, 0},
//  {4, 0, 1, 0, 100, 0, 0},
//  {5, 0, 1, 0, 100, 0, 0}
//};

// pulse
tubeInstruction CURRENT_INSTRUCTIONS[TUBE_COUNT] = {
  {5, 6000, 3, 0, 6, 0, 3000},
  {7, 6200, 2, 0, 5, 0, 3000},
  {5, 6400, 3, 0, 4, 0, 3000},
  {5, 6600, 3, 0, 3, 0, 3000},
  {5, 6800, 3, 0, 2, 0, 3000},
  {8, 0, 2, 0, 1, 1, 3000}
};


/**
 * ============================
 *    MAIN LOOP - CONTINUOUS
 * ============================
 */
void loop() {
  
  // handle possible serial input of different frequency
  if (Serial.available() > 0) {
    int newFreq = Serial.parseInt();
    //byte newFreq = Serial.read();
    
    Serial.print("Entered: ");
    Serial.println(newFreq);
    if (newFreq > 0 && newFreq < 10000) {
      FREQ_HZ = newFreq;
      calculatePwmVals();
    }
  }
  
  // current time
  unsigned long now = millis();
  
  // WARMUP
  if (!WARMED_UP) {
    //warmup();
    WARMED_UP = true;
    
    now = millis();
    LAST_TS = now;
    
    for (int i=0; i<TUBE_COUNT; i++) {
      STATES[i].lastDirectionChangeTS = now;
    }
  }
  
  // LAST_TS = last direction shift / last event
  // diff = time elapsed since last event
  int diff = now - LAST_TS;
  
  
  
  
  
  
  /*****************************
  * --- COMMAND INTERPRETER ---
  ******************************/
  /*
  // multiplex the on/off for pwm on each tube
  for (int i=0; i<TUBE_COUNT; i++) {
    
    // if this is the first time we're processing this instruction, log the time
    if (STATES[i].currentInstructionStartTS <= 0) {
      STATES[i].currentInstructionStartTS = now;
      
      // if the fade direction is high to low, reverse the default direction
      if (CURRENT_INSTRUCTIONS[i].fadeDirection == FADE_DIR_H2L) {
        STATES[i].pwmFadingUp = false;
      }
    }
    
    //int diffSinceLastCheck = now - STATES[i].lastCheckTS;
    //STATES[i].lastCheckTS = now;
    
    int tubeDirectionDiff = now - STATES[i].lastDirectionChangeTS;
    
    // TODO - FIX overflow
    if (tubeDirectionDiff < 0) {
      Serial.println("tubeDirectionDiff overflow");
      
    }
    
    // state
//    byte displayValue;
//    unsigned long lastDirectionChangeTS;
//    boolean pwmFadingUp;
//    unsigned long currentInstructionStartTS;
    
    // instructions
//    byte displayValue;
//    int displayDurationMS;
//    byte displayMode;
//    byte fadeLevelLow;
//    byte fadeLevelHigh;
//    byte fadeDirection;
//    int pulseSegmentDurationMS;

    
    // if instruction duration is not indefinite (> 0), update elapsed time
    if (CURRENT_INSTRUCTIONS[i].displayDurationMS > 0) {
      int elapsedDurationMS = now - STATES[i].currentInstructionStartTS;
      
      // check if over duration
      if (elapsedDurationMS >= CURRENT_INSTRUCTIONS[i].displayDurationMS) {
        // TODO - Next Instruction
        
        // if no next instruction (and always- for now), display blank
        displayOnTube(i, BLANK_DISPLAY, true);
        delayMicroseconds(MUX_FADE_MAX_US);
        continue;
      }
    }
    
    if (CURRENT_INSTRUCTIONS[i].displayMode == DISP_OFF) {
      displayOnTube(i, BLANK_DISPLAY, true);
      delayMicroseconds(MUX_FADE_MAX_US);
    }
    else if (CURRENT_INSTRUCTIONS[i].displayMode == DISP_CONST) {
    }
    else if (CURRENT_INSTRUCTIONS[i].displayMode == DISP_FADE) {
      
      // correct for maximum (diff since last check may be a more than than count duration limit - limit it to this)
      if (tubeDirectionDiff > CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS) {
        tubeDirectionDiff = CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS;
      }
      
      // further constrain min/max for tube-specific min/max
      long tubeFadeMinUS = map(CURRENT_INSTRUCTIONS[i].fadeLevelLow, FADE_LEVEL_MIN, FADE_LEVEL_MAX, MUX_FADE_MIN_US, MUX_FADE_MAX_US);
      long tubeFadeMaxUS = map(CURRENT_INSTRUCTIONS[i].fadeLevelHigh, FADE_LEVEL_MIN, FADE_LEVEL_MAX, MUX_FADE_MIN_US, MUX_FADE_MAX_US);
      
      long litDurationMicros = 0L;
      long offDurationMicros = 0L;
      
      // map time difference (from last direction switch) to equivalent-scale on/off durations
      //   - we can do this for each tub individually, because the lit+off time for each tube will always = tube period, and all 6 will = total period
      if (STATES[i].pwmFadingUp) {
        // fade up
        litDurationMicros = map(tubeDirectionDiff, 0, CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS, tubeFadeMinUS, tubeFadeMaxUS);
        offDurationMicros = map(tubeDirectionDiff, 0, CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS, tubeFadeMaxUS, tubeFadeMinUS);
      } else {
        // fade down
        litDurationMicros = map(tubeDirectionDiff, 0, CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS, tubeFadeMaxUS, tubeFadeMinUS);
        offDurationMicros = map(tubeDirectionDiff, 0, CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS, tubeFadeMinUS, tubeFadeMaxUS);
      }
      
      // display value on given tube for calculated on & off duration within period
      displayOnTube(i, CURRENT_INSTRUCTIONS[i].displayValue, true);
      delayMicroseconds(litDurationMicros);
      
      displayOnTube(i, BLANK_DISPLAY, true);
      delayMicroseconds(offDurationMicros);
      
    }
    else if (CURRENT_INSTRUCTIONS[i].displayMode == DISP_PULSE) {
      
      // correct for maximum (diff since last check may be a more than than count duration limit - limit it to this)
      if (tubeDirectionDiff > CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS) {
        tubeDirectionDiff = CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS;
      }
      
      // further constrain min/max for tube-specific min/max
      long tubeFadeMinUS = map(CURRENT_INSTRUCTIONS[i].fadeLevelLow, FADE_LEVEL_MIN, FADE_LEVEL_MAX, MUX_FADE_MIN_US, MUX_FADE_MAX_US);
      long tubeFadeMaxUS = map(CURRENT_INSTRUCTIONS[i].fadeLevelHigh, FADE_LEVEL_MIN, FADE_LEVEL_MAX, MUX_FADE_MIN_US, MUX_FADE_MAX_US);
      
      long litDurationMicros = 0L;
      long offDurationMicros = 0L;
      
      // map time difference (from last direction switch) to equivalent-scale on/off durations
      //   - we can do this for each tub individually, because the lit+off time for each tube will always = tube period, and all 6 will = total period
      if (STATES[i].pwmFadingUp) {
        // fade up
        litDurationMicros = map(tubeDirectionDiff, 0, CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS, tubeFadeMinUS, tubeFadeMaxUS);
        offDurationMicros = map(tubeDirectionDiff, 0, CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS, tubeFadeMaxUS, tubeFadeMinUS);
      } else {
        // fade down
        litDurationMicros = map(tubeDirectionDiff, 0, CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS, tubeFadeMaxUS, tubeFadeMinUS);
        offDurationMicros = map(tubeDirectionDiff, 0, CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS, tubeFadeMinUS, tubeFadeMaxUS);
      }
      
      // display value on given tube for calculated on & off duration within period
      displayOnTube(i, CURRENT_INSTRUCTIONS[i].displayValue, true);
      delayMicroseconds(litDurationMicros);
      
      displayOnTube(i, BLANK_DISPLAY, true);
      delayMicroseconds(offDurationMicros);
      
      // check if duration has crossed threshold
      if (tubeDirectionDiff >= CURRENT_INSTRUCTIONS[i].pulseSegmentDurationMS) {
        // reset last switch time
        STATES[i].lastDirectionChangeTS = now;
    
        // swap fade direction
        if (STATES[i].pwmFadingUp) {
          STATES[i].pwmFadingUp = false;
        } else {
          STATES[i].pwmFadingUp = true;
        }
      }
    
    } // end if
  
  
  } // end for each tube
  */
  
  
  
  
  
  
  
  
  
  
  
  /************* 
  * multiplex + PWM - count up different num on all tubes with each fade up + down cycle
  *   --- WITH individual tube state tracking ---
  *************/
  /*
  // multiplex the on/off for pwm on each tube
  for (int i=0; i<TUBE_COUNT; i++) {
    int tubeDiff = now - STATES[i].lastDirectionChangeTS;
    
    // correct for maximum (diff since last check may be a more than than count duration limit - limit it to this)
    if (tubeDiff > STATES[i].pulseSegmentDurationMS) {
      tubeDiff = STATES[i].pulseSegmentDurationMS;
    }
    
    long litDurationMicros = 0L;
    long offDurationMicros = 0L;
  
    // map time difference (from last direction switch) to equivalent-scale on/off durations
    //   - we can do this for each tub individually, because the lit+off time for each tube will always = tube period, and all 6 will = total period
    if (STATES[i].pwmFadingUp) {
      // fade up
      litDurationMicros = map(tubeDiff, 0, STATES[i].pulseSegmentDurationMS, MUX_FADE_MIN_US, MUX_FADE_MAX_US);
      offDurationMicros = map(tubeDiff, 0, STATES[i].pulseSegmentDurationMS, MUX_FADE_MAX_US, MUX_FADE_MIN_US);
    } else {
      // fade down
      litDurationMicros = map(tubeDiff, 0, STATES[i].pulseSegmentDurationMS, MUX_FADE_MAX_US, MUX_FADE_MIN_US);
      offDurationMicros = map(tubeDiff, 0, STATES[i].pulseSegmentDurationMS, MUX_FADE_MIN_US, MUX_FADE_MAX_US);
    }
    
    // display value on given tube for calculated on & off duration within period
    displayOnTube(i, STATES[i].displayValue, true);
    delayMicroseconds(litDurationMicros);
    
    displayOnTube(i, BLANK_DISPLAY, true);
    delayMicroseconds(offDurationMicros);
    
    // check if duration has crossed threshold
    if (tubeDiff >= STATES[i].pulseSegmentDurationMS) {
      // reset last switch time
      STATES[i].lastDirectionChangeTS = now;
  
      // swap fade direction
      if (STATES[i].pwmFadingUp) {
        STATES[i].pwmFadingUp = false;
      } else {
        STATES[i].pwmFadingUp = true;
        
        // increment display number on switch to new fade up cycle
        if (STATES[i].displayValue == 9) {
          STATES[i].displayValue = 0;
        } else {
          STATES[i].displayValue++;
        }
      }
    }
    
  }
  */
  
  
  
  
  /************* 
  * basic count up - same value on all tubes
  *************/
  /*
  digitalWrite(PIN_ANODE_1, HIGH);
  digitalWrite(PIN_ANODE_2, HIGH);
  digitalWrite(PIN_ANODE_3, HIGH);
  
  for (int i=0; i<10; i++) {
    setCathode(true, i);
    setCathode(false, i);
    delay(1000);
  }
  */


  /************* 
  * multiplex scroll contant different values on each tube - step up timings
  *************/
  /*
  for (int r=0; r<muxDemoStepUpRuns[muxDemoStep]; r++) {
    for (int i=0; i<TUBE_COUNT; i++) {
      displayOnTube(i, tubeSeq[i], true);
      delay(muxDemoStepUpDelayMs[muxDemoStep]);
    } 
  }
  
  // increment step
  muxDemoStep++;
  if (muxDemoStep > 3) {
    muxDemoStep = 0;
  }
  Serial.print(muxDemoStep);
  */
  
  /************* 
  * multiplex couting up different values on each tube - constant timing
  *************/
  /*
  for (int i=0; i<TUBE_COUNT; i++) {
    displayOnTube(i, tubeSeq[i], true);
    delayMicroseconds(2000);
  }
  
  if (diff > countDurationMillis) {
    // reset last switch time
    LAST_TS = now;

    // increment
    for (int i=0; i<TUBE_COUNT; i++) {
      if (tubeSeq[i] == 9) {
        tubeSeq[i] = 0;
      } else {
        tubeSeq[i]++;
      }
    }
  }
  */
  
  /************* 
   * PWM - count up same num on all tubes with each fade up + down cycle
   *************/
   /*
   if (diff >= PWM_FADE_LENGTH_MS) {
     diff = PWM_FADE_LENGTH_MS;
   }

  long litDurationMicros = 0L;
  long offDurationMicros = 0L;

  if (PWM_FADE_UP_STATE == 1) {
    // fade up
    litDurationMicros = map(diff, 0, PWM_FADE_LENGTH_MS, PWM_FADE_MIN_US, PWM_FADE_MAX_US);
    offDurationMicros = map(diff, 0, PWM_FADE_LENGTH_MS, PWM_FADE_MAX_US, PWM_FADE_MIN_US);
  } else {
    // fade down
    litDurationMicros = map(diff, 0, PWM_FADE_LENGTH_MS, PWM_FADE_MAX_US, PWM_FADE_MIN_US);
    offDurationMicros = map(diff, 0, PWM_FADE_LENGTH_MS, PWM_FADE_MIN_US, PWM_FADE_MAX_US);
  }
  
  setCathode(true, pwmSeqNum);
  setCathode(false, pwmSeqNum);

  digitalWrite(PIN_ANODE_1, HIGH);
  digitalWrite(PIN_ANODE_2, HIGH);
  digitalWrite(PIN_ANODE_3, HIGH);
  delayMicroseconds(litDurationMicros);

  digitalWrite(PIN_ANODE_1, LOW);
  digitalWrite(PIN_ANODE_2, LOW);
  digitalWrite(PIN_ANODE_3, LOW);
  delayMicroseconds(offDurationMicros);
  
  if (diff >= PWM_FADE_LENGTH_MS) {
    // reset last switch time
    LAST_TS = now;

    // swap fade direction
    if (PWM_FADE_UP_STATE == 0) {
      PWM_FADE_UP_STATE = 1;
      
      // increase sequence number
      if (pwmSeqNum < 9) {
        pwmSeqNum++;
      } else {
        pwmSeqNum = 0;
      }
    } else {
      PWM_FADE_UP_STATE = 0;
    }
  }
  */
  
  /************* 
  * multiplex + PWM - count up different num on all tubes with each fade up + down cycle
  *************/
  /*
  // correct for maximum (diff since last check may be a more than than count duration limit - limit it to this)
  if (diff > countDurationMillis) {
    diff = countDurationMillis;
  }
  
  long litDurationMicros = 0L;
  long offDurationMicros = 0L;

  // map time difference (from last direction switch) to equivalent-scale on/off durations
  if (PWM_FADE_UP_STATE == 1) {
    // fade up
    litDurationMicros = map(diff, 0, countDurationMillis, MUX_FADE_MIN_US, MUX_FADE_MAX_US);
    offDurationMicros = map(diff, 0, countDurationMillis, MUX_FADE_MAX_US, MUX_FADE_MIN_US);
  } else {
    // fade down
    litDurationMicros = map(diff, 0, countDurationMillis, MUX_FADE_MAX_US, MUX_FADE_MIN_US);
    offDurationMicros = map(diff, 0, countDurationMillis, MUX_FADE_MIN_US, MUX_FADE_MAX_US);
  }
    
  // multiplex the on/off for pwm on each tube
  for (int i=0; i<TUBE_COUNT; i++) {
    displayOnTube(i, tubeSeq[i], true);
    delayMicroseconds(litDurationMicros);
    
    displayOnTube(i, BLANK_DISPLAY, true);
    delayMicroseconds(offDurationMicros);
  }
  
  if (diff >= countDurationMillis) {
    
    // reset last switch time
    LAST_TS = now;

    // swap fade direction
    if (PWM_FADE_UP_STATE == 0) {
      PWM_FADE_UP_STATE = 1;
      
      // increment display number on switch to new fade up cycle
      for (int i=0; i<TUBE_COUNT; i++) {
        if (tubeSeq[i] == 9) {
          tubeSeq[i] = 0;
        } else {
          tubeSeq[i]++;
        }
      }
    } else {
      PWM_FADE_UP_STATE = 0;
    }
  }
  */

  printTime();
  delay(1000);
  
  
}

