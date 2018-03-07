//REF :https://sites.google.com/site/wayneholder/attiny-fuse-reset
//2018-03-06, xiaolaba
//add read full signature 3 bytes, some device has wrong signature byte-x but functional properly
//add read cal, some device always responds 0xff, 0xff !? but functional properly
//add read lock, tiny13 only has two lock bits documented or saying programmable 
//add chip erase

    // AVR High-voltage Serial Fuse Reprogrammer
    // Adapted from code and design by Paul Willoughby 03/20/2010
    //   http://www.rickety.us/2010/03/arduino-avr-high-voltage-serial-programmer/
    //
    // Fuse Calc:
    //   http://www.engbedded.com/fusecalc/

    #define  VCC      2    // Target VCC
    #define  SDO      3    // Target Data Output
    #define  SII      4    // Target Instruction Input
    #define  SDI      5    // Target Data Input
    #define  SCI      6    // Target Clock Input
    #define  RST     13    // Output to level shifter for !RESET from transistor

#define  HFUSE  0x747C
#define  LFUSE  0x646C
#define  EFUSE  0x666E

// Define ATTiny series signatures
#define  ATTINY13   0x1E9007  // L: 0x6A, H: 0xFF             8 pin
#define  ATTINY24   0x1E910B  // L: 0x62, H: 0xDF, E: 0xFF   14 pin
#define  ATTINY25   0x1E9108  // L: 0x62, H: 0xDF, E: 0xFF    8 pin
#define  ATTINY44   0x1E9207  // L: 0x62, H: 0xDF, E: 0xFFF  14 pin
#define  ATTINY45   0x1E9206  // L: 0x62, H: 0xDF, E: 0xFF    8 pin
#define  ATTINY84   0x1E930C  // L: 0x62, H: 0xDF, E: 0xFFF  14 pin
#define  ATTINY85   0x1E930B  // L: 0x62, H: 0xDF, E: 0xFF    8 pin

void setup() {
  pinMode(VCC, OUTPUT);
  pinMode(RST, OUTPUT);
  pinMode(SDI, OUTPUT);
  pinMode(SII, OUTPUT);
  pinMode(SCI, OUTPUT);
  pinMode(SDO, OUTPUT);     // Configured as input when in programming mode
  digitalWrite(RST, HIGH);  // Level shifter is inverting, this shuts off 12V
  Serial.begin(19200);
  Serial.println("AVR High-voltage Serial Fuse Reprogrammer, send a char then will see response\r\n");
}

void loop() {
   if (Serial.available() > 0) {
    Serial.read();
    pinMode(SDO, OUTPUT);     // Set SDO to output
    digitalWrite(SDI, LOW);
    digitalWrite(SII, LOW);
    digitalWrite(SDO, LOW);
    digitalWrite(RST, HIGH);  // 12v Off
    digitalWrite(VCC, HIGH);  // Vcc On
    delayMicroseconds(60);    // wait 20-60us
    digitalWrite(RST, LOW);   // 12v On
    delayMicroseconds(10);    // keep the state for at least 10us
                              // should be entered HV Programming mode
    pinMode(SDO, INPUT);      // Set SDO to input, relase
    delayMicroseconds(300);   // wait for 300us before giving instruction to SDI/SII

    unsigned long sig = readSignature();
    Serial.print("Signature is: 0x");
    Serial.print(sig, HEX);
    if (sig == ATTINY13) {
      Serial.println(" = ATtiny13/13V, ATtiny13A");
    }

    ChipErase ();
    Serial.println("Chip Erased");

    unsigned long cal = readCalibration();
    Serial.print("Calibration bytes: 0x");
    Serial.print(cal>>8, HEX);
    Serial.print(" 0x");
    Serial.println(cal & 0xff, HEX);

    Serial.print("Lock: "); Serial.println(readLock(), HEX);

    Serial.print("before: "); readFuses();

    if (sig == ATTINY13) {
      writeFuse(LFUSE, 0x6A);
      writeFuse(HFUSE, 0xFF);
    } else if (sig == ATTINY24 || sig == ATTINY44 || sig == ATTINY84 ||
               sig == ATTINY25 || sig == ATTINY45 || sig == ATTINY85) {
      writeFuse(LFUSE, 0x62);
      writeFuse(HFUSE, 0xDF);
      writeFuse(EFUSE, 0xFF);
    }

    Serial.print("after:  "); readFuses();

    Serial.println("Power off sequency: set SCI to '0', set RESET to '1', turn Vcc power off\r\n");
    digitalWrite(SCI, LOW);
    digitalWrite(VCC, LOW);    // Vcc Off
    digitalWrite(RST, HIGH);   // 12v Off
  }
}

byte shiftOut (byte val1, byte val2) {
  int inBits = 0;
  //Wait until SDO goes high
  while (!digitalRead(SDO))
    ;
  unsigned int dout = (unsigned int) val1 << 2;
  unsigned int iout = (unsigned int) val2 << 2;
  for (int ii = 10; ii >= 0; ii--)  {
    digitalWrite(SDI, !!(dout & (1 << ii)));
    digitalWrite(SII, !!(iout & (1 << ii)));
    inBits <<= 1;
    inBits |= digitalRead(SDO);
    digitalWrite(SCI, HIGH);
    digitalWrite(SCI, LOW);
  }
  return inBits >> 2;
}

void writeFuse (unsigned int fuse, byte val) {
  shiftOut(0x40, 0x4C);
  shiftOut( val, 0x2C);
  shiftOut(0x00, (byte) (fuse >> 8));
  shiftOut(0x00, (byte) fuse);
}

void readFuses () {
  byte val;
        shiftOut(0x04, 0x4C);  // LFuse
        shiftOut(0x00, 0x68);
  val = shiftOut(0x00, 0x6C);
  Serial.print("LFuse = 0x");
  Serial.print(val, HEX);
        shiftOut(0x04, 0x4C);  // HFuse
        shiftOut(0x00, 0x7A);
  val = shiftOut(0x00, 0x7E);
  Serial.print(", HFuse = 0x");
  Serial.print(val, HEX);
        shiftOut(0x04, 0x4C);  // EFuse
        shiftOut(0x00, 0x6A);
  val = shiftOut(0x00, 0x6E);
  Serial.print(", EFuse = 0x");
  Serial.println(val, HEX);
}

unsigned long readSignature () {  //3 bytes, signature
  unsigned long sig = 0;
  byte val;
  for (int ii = 0; ii < 3; ii++) {
          shiftOut(0x08, 0x4C);
          shiftOut(  ii, 0x0C);
          shiftOut(0x00, 0x68);
    val = shiftOut(0x00, 0x6C);
    sig = (sig << 8) + val;
  }
  return sig;
}

unsigned int readCalibration () {
  unsigned int cal = 0;
  byte val;
  for (int ii = 0; ii < 2; ii++) {
          shiftOut(0b00001000, 0b01001100); //0x80, 0x4C
          shiftOut(  ii, 0x0C);
          shiftOut(0x00, 0x78);
    val = shiftOut(0x00, 0x7C);
    cal = (cal << 8) + val;
  }
  return cal;
}


unsigned int readLock () {
  unsigned int lock = 0;
  byte val;
//  for (int ii = 0; ii < 1; ii++) {
          shiftOut(0x04, 0x4C);
//          shiftOut(  ii, 0x0C);
          shiftOut(0x00, 0x78);
    val = shiftOut(0x00, 0x7C);
    lock = (lock << 8) + val;
//  }
  return lock;
}

void ChipErase () {
  shiftOut(0x80, 0x4C);
  shiftOut(0x00, 0x64);
  shiftOut(0x00, 0x6C);
}
