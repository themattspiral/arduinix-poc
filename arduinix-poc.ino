/**
 * arduinix-poc
 */

// Controller 1 (SN74141/K155ID1)
byte PIN_CATHODE_0_A = 2;                
byte PIN_CATHODE_0_B = 3;
byte PIN_CATHODE_0_C = 4;
byte PIN_CATHODE_0_D = 5;

// Controller 2 (SN74141/K155ID1)
byte PIN_CATHODE_1_A = 6;                
byte PIN_CATHODE_1_B = 7;
byte PIN_CATHODE_1_C = 8;
byte PIN_CATHODE_1_D = 9;

// anode pins
byte PIN_ANODE_1 = 10;
byte PIN_ANODE_2 = 11;
byte PIN_ANODE_3 = 12;
byte PIN_ANODE_4 = 13;

byte BLANK = 15;

// 6 tubes, each wired to a unique combination of anode pin and cathode controller
byte TUBE_COUNT = 6;
byte TUBE_ANODES[] = {1, 1, 2, 2, 3, 3};
byte TUBE_CATHODE_CTRL_0[] = {false, true, false, true, false, true};

boolean WARMED_UP = false;

// timing
unsigned long LAST_TS = 0UL;
long PWM_FADE_LENGTH_MS = 3000L;
byte PWM_FADE_UP_STATE = 1;

// 500 Hz = 1/500 * 1M us = 2000us period
// 10 Hz = 1/10 * 1M us = 100000us period
// 20 Hz = 1/10 * 1M us = 50000us period
// 62.5 Hz = 16000us period
// Experimentation shows the minimum smooth update frequency is ~60Hz
long FREQ = 62L;
long PERIOD_US = 0L;

long PWM_FADE_MIN_US = 0L;
long PWM_FADE_MAX_US = 0L;

long MUX_PERIOD_US = 0L;
long MUX_ON_US = 0L;
long MUX_OFF_US = 0L;

byte SEQ = 0;


void calculatePwmVals() {
  PERIOD_US = 1000000L / FREQ;
  
  PWM_FADE_MIN_US = PERIOD_US / 100L; // (1/100 = 0.01 = 1% duty minimum)
  PWM_FADE_MAX_US = PERIOD_US; // 100% duty maximum
  
  long adjustedFreq = FREQ * TUBE_COUNT;
  MUX_PERIOD_US = 1000000L / adjustedFreq;
  MUX_ON_US = MUX_PERIOD_US; // (100% duty)
  //MUX_ON_US = MUX_PERIOD_US / 2L; // (1/2 = .5 = 50% duty)
  //MUX_ON_US = MUX_PERIOD_US / 5L; // (1/5 = .2 = 20% duty)
  MUX_OFF_US = MUX_PERIOD_US - MUX_ON_US;
  
  long muxMinOffUs = MUX_PERIOD_US / 100L;
  if (MUX_OFF_US < muxMinOffUs) {
    MUX_OFF_US = muxMinOffUs;
  }
  
  Serial.print("PWM Init - Frequency: ");
  Serial.print(FREQ, DEC);
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
  Serial.print(") - MUX_ON_US: ");
  Serial.print(MUX_ON_US, DEC);
  Serial.print(" - MUX_OFF_US: ");
  Serial.print(MUX_OFF_US, DEC);
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
  setCathode(true, BLANK);
  setCathode(false, BLANK);
  
  Serial.begin(115200);
  
  // initialize PWM Values
  calculatePwmVals();
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

void displayOnTube(byte tubeIndex, byte displayNumber) {
  byte anode = TUBE_ANODES[tubeIndex];
  boolean cathodeCtrl0 = TUBE_CATHODE_CTRL_0[tubeIndex];
  
  switch(anode) {
    case 1:
      digitalWrite(PIN_ANODE_2, LOW);
      digitalWrite(PIN_ANODE_3, LOW);
      digitalWrite(PIN_ANODE_1, HIGH);
      break;
    case 2:
      digitalWrite(PIN_ANODE_1, LOW);
      digitalWrite(PIN_ANODE_3, LOW);
      digitalWrite(PIN_ANODE_2, HIGH);
      break;
    case 3:
      digitalWrite(PIN_ANODE_1, LOW);
      digitalWrite(PIN_ANODE_2, LOW);
      digitalWrite(PIN_ANODE_3, HIGH);
      break;
  }
  
  setCathode(!cathodeCtrl0, BLANK);
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
    setCathode(true, BLANK);
    setCathode(false, BLANK);
    delay(300);
  }
  
  setCathode(true, BLANK);
  setCathode(false, BLANK);
  delay(300);
  
  // cycle across tubes, starting with 9 down to 0
  int SEQ = 9;
  for(int i=0; i<TUBE_COUNT; i++) {
    displayOnTube(i, SEQ);
    delay(65);
    
    if (i == TUBE_COUNT-1 && SEQ > 0) {
      i = -1;
      SEQ--;
    }
  }
  
  setCathode(true, BLANK);
  setCathode(false, BLANK);
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
  
  setCathode(true, BLANK);
  setCathode(false, BLANK);
  delay(300);
  
  // count 0-9 quickly, 5 times
  for (int c=0; c<5; c++) {
    for (int i=0; i<10; i++) {
      setCathode(true, i);
      setCathode(false, i);
      delay(30);
    }
  }
  
  setCathode(true, BLANK);
  setCathode(false, BLANK);
  delay(300);
  
  digitalWrite(PIN_ANODE_1, LOW);
  digitalWrite(PIN_ANODE_2, LOW);
  digitalWrite(PIN_ANODE_3, LOW);
}


void countUp() {
  for (int SEQ=0; SEQ<10; SEQ++) {
    setCathode(true, SEQ);
    setCathode(false, SEQ);
    delay(500);
  }
}


int tubeSeq[] = {0, 1, 2, 3, 4, 5};
int countDurationMillis = 2000;

int muxDemoStepUpDurationMillis = 6000;
int muxDemoStepUpDelayMs[] = {500, 80, 20, 2};
int muxDemoStepUpRuns[] = {4, 15, 80, 400};
int muxDemoStep = 0;

/**
 * LOOP
 *
 */
void loop() {
  if (Serial.available() > 0) {
    int newFreq = Serial.parseInt();
    //byte newFreq = Serial.read();
    
    Serial.print("Entered: ");
    Serial.println(newFreq);
    if (newFreq > 0 && newFreq < 10000) {
      FREQ = newFreq;
      calculatePwmVals();
    }
  }
  
  unsigned long now = millis();
  
  /************* 
   * warmup
   *************/
   
  if (!WARMED_UP) {
    //warmup();
    WARMED_UP = true;
    
    now = millis();
    LAST_TS = now;
  }
  
  int diff = now - LAST_TS;
  
  /************* 
  * basic count up
  *************/
  digitalWrite(PIN_ANODE_1, HIGH);
  digitalWrite(PIN_ANODE_2, HIGH);
  digitalWrite(PIN_ANODE_3, HIGH);
  
  for (int i=0; i<10; i++) {
    setCathode(true, i);
    setCathode(false, i);
    delay(1000);
  }
    


  /************* 
  * multiplex step up
  *************/
  /*
  for (int r=0; r<muxDemoStepUpRuns[muxDemoStep]; r++) {
    for (int i=0; i<6; i++) {
      displayOnTube(i, tubeSeq[i]);
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
  * multiplex 
  *************/
  /*
  for (int i=0; i<6; i++) {
    displayOnTube(i, tubeSeq[i]);
    delayMicroseconds(2000);
  }
  
  if (diff > countDurationMillis) {
    // reset last switch time
    LAST_TS = now;

    // increment
    for (int i=0; i<6; i++) {
      if (tubeSeq[i] == 9) {
        tubeSeq[i] = 0;
      } else {
        tubeSeq[i]++;
      }
    }
  }
  */
  
  /************* 
   * PWM
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
  
  setCathode(true, SEQ);
  setCathode(false, SEQ);

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
      if (SEQ < 9) {
        SEQ++;
      } else {
        SEQ = 0;
      }
    } else {
      PWM_FADE_UP_STATE = 0;
    }
  }
  */
  
  /************* 
  * multiplex + PWM
  *************/
  /*
  for (int i=0; i<6; i++) {
    displayOnTube(i, tubeSeq[i]);
    //delayMicroseconds(2000);
    delayMicroseconds(MUX_ON_US);
    
    displayOnTube(i, BLANK);
    delayMicroseconds(MUX_OFF_US);
  }
  
  if (diff > countDurationMillis) {
    // reset last switch time
    LAST_TS = now;

    // increment
    for (int i=0; i<6; i++) {
      if (tubeSeq[i] == 9) {
        tubeSeq[i] = 0;
      } else {
        tubeSeq[i]++;
      }
    }
  }
  */
  
}

