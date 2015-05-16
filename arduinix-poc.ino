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

boolean warmedUp = false;

// PWM
unsigned long last = 0UL;
long fadeLengthMillis = 3000L;
byte fadeUpState = 1;

// 500 Hz = 1/500 * 1M us = 2000us period
// 10 Hz = 1/10 * 1M us = 100000us period
// 20 Hz = 1/10 * 1M us = 50000us period
// 62.5 Hz = 16000us period
// Experimentation shows the minimum smooth update frequency is ~60Hz
long freq = 62L;
long periodMicros = 0L;
long fadeMinMicros = 0L;
long fadeMaxMicros = 0L;

byte seq = 0;


void calculatePwmVals() {
  periodMicros = 1000000L / freq;
  fadeMinMicros = periodMicros / 100L; // 1% duty minimum
  fadeMaxMicros = periodMicros; // 100% duty maximum
  
  Serial.print("PWM Init - Frequency: ");
  Serial.print(freq, DEC);
  Serial.print(" - Period: ");
  Serial.print(periodMicros, DEC);
  Serial.print(" - fadeMinMicros: ");
  Serial.print(fadeMinMicros, DEC);
  Serial.print(" - fadeMaxMicros: ");
  Serial.print(fadeMaxMicros, DEC);
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
  int seq = 9;
  for(int i=0; i<TUBE_COUNT; i++) {
    displayOnTube(i, seq);
    delay(65);
    
    if (i == TUBE_COUNT-1 && seq > 0) {
      i = -1;
      seq--;
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
  for (int seq=0; seq<10; seq++) {
    setCathode(true, seq);
    setCathode(false, seq);
    delay(500);
  }
}


int tubeSeq[] = {0, 1, 2, 3, 4, 5};
int countDurationMillis = 2000;

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
      freq = newFreq;
      calculatePwmVals();
    }
  }
  
  unsigned long now = millis();
  
  if (!warmedUp) {
    //warmup();
    warmedUp = true;
    
    now = millis();
    last = now;
  }
  
  int diff = now - last;
  
  /************* 
   * multiplex
   *************/
  
  for (int i=0; i<6; i++) {
    displayOnTube(i, tubeSeq[i]);
    delayMicroseconds(2000);
  }
  
  if (diff > countDurationMillis) {
    // reset last switch time
    last = now;

    // increment
    for (int i=0; i<6; i++) {
      if (tubeSeq[i] == 9) {
        tubeSeq[i] = 0;
      } else {
        tubeSeq[i]++;
      }
    }
  }
  
  
  /************* 
   * PWM
   *************/
   /*
   if (diff >= fadeLengthMillis) {
     diff = fadeLengthMillis;
   }

  long litDurationMicros = 0L;
  long offDurationMicros = 0L;

  if (fadeUpState == 1) {
    // fade up
    litDurationMicros = map(diff, 0, fadeLengthMillis, fadeMinMicros, fadeMaxMicros);
    offDurationMicros = map(diff, 0, fadeLengthMillis, fadeMaxMicros, fadeMinMicros);
  } else {
    // fade down
    litDurationMicros = map(diff, 0, fadeLengthMillis, fadeMaxMicros, fadeMinMicros);
    offDurationMicros = map(diff, 0, fadeLengthMillis, fadeMinMicros, fadeMaxMicros);
  }
  
  setCathode(true, seq);
  setCathode(false, seq);

  digitalWrite(PIN_ANODE_1, HIGH);
  digitalWrite(PIN_ANODE_2, HIGH);
  digitalWrite(PIN_ANODE_3, HIGH);
  delayMicroseconds(litDurationMicros);

  digitalWrite(PIN_ANODE_1, LOW);
  digitalWrite(PIN_ANODE_2, LOW);
  digitalWrite(PIN_ANODE_3, LOW);
  delayMicroseconds(offDurationMicros);
  
  if (diff >= fadeLengthMillis) {
    // reset last switch time
    last = now;

    // swap fade direction
    if (fadeUpState == 0) {
      fadeUpState = 1;
      
      // increase sequence number
      if (seq < 9) {
        seq++;
      } else {
        seq = 0;
      }
    } else {
      fadeUpState = 0;
    }
  }
  */
  
}

