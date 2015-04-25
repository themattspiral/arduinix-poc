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
byte tubeAnodes[] = {1, 1, 2, 2, 3, 3};
byte tubeCathodeCtrl0[] = {false, true, false, true, false, true};

boolean warmedUp = false;

// PWM
unsigned long last = 0UL;
long fadeLengthMillis = 500;
int fadeUpState = 1;

// 500 Hz = 1/500 * 1M us = 2000us period
long periodMicros = 2000;
long fadeMinMicros = 20; // 1% duty minimum
long fadeMaxMicros = periodMicros - fadeMinMicros;

byte seq = 0;


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

void displayOnTube(byte tube, byte displayNumber) {
  byte anode = tubeAnodes[tube];
  boolean cathodeCtrl0 = tubeCathodeCtrl0[tube];
  
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
  
  setCathode(cathodeCtrl0, displayNumber);
  setCathode(!cathodeCtrl0, BLANK);
}


void warmup() {
  digitalWrite(PIN_ANODE_1, HIGH);
  digitalWrite(PIN_ANODE_2, HIGH);
  digitalWrite(PIN_ANODE_3, HIGH);
  
  for (int i=0; i<2; i++) {
    setCathode(true, 0);
    setCathode(false, 0);
    delay(750);
    setCathode(true, BLANK);
    setCathode(false, BLANK);
    delay(100);
  }
  
  for (int c=0; c<2; c++) {
    for (int i=0; i<10; i++) {
      setCathode(true, i);
      setCathode(false, i);
      delay(100);
    }
  }
  
  for (int c=0; c<5; c++) {
    for (int i=0; i<10; i++) {
      setCathode(true, i);
      setCathode(false, i);
      delay(25);
    }
  }
  
  setCathode(true, BLANK);
  setCathode(false, BLANK);
  
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


/**
 * LOOP
 *
 */
void loop() {
  if (!warmedUp) {
    warmup();
    warmedUp = true;
  }
  
  // multiplex
  /*for (int i=0; i<6; i++) {
    displayOnTube(i, i);
    delayMicroseconds(2000);
  }*/
  
  
  // pwm
  unsigned long now = millis();
  int diff = now - last;

  long litDurationMicros = 0L;

  if (fadeUpState == 1) {
    // fade up
    //litDurationMicros = fadeBaseMicros + diff;
    litDurationMicros = map(diff, 0, fadeLengthMillis, fadeMinMicros, fadeMaxMicros);
  } else {
    // fade down
    //litDurationMicros = fadeMaxMicros - diff;
    litDurationMicros = map(diff, 0, fadeLengthMillis, fadeMaxMicros, fadeMinMicros);
  }

  long offDurationMicros = fadeMaxMicros - litDurationMicros;

  // avoid 'negative' overflow in unsigned long, or values less than base/min
  if (litDurationMicros < fadeMinMicros) {
    litDurationMicros = fadeMinMicros;
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
  
  
  if (diff > fadeLengthMillis) {
    // reset last switch time
    last = now;

    // swap fade direction
    if (fadeUpState == 0) {
      fadeUpState = 1;
      
      if (seq < 9) {
        seq++;
      } else {
        seq = 0;
      }
    } else {
      fadeUpState = 0;
    }
  }
}

