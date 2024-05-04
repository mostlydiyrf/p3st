
// Arduino "sketch" for use with the P3ST Xiao Digital VFO/BFO (v.2.1.2 beta -- 4.14.24)
// (C) T.F. Carney (K7TFC). For use under the terms of the
// Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International license.
// See https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode.en

#include <xiaoRP2040pinNums.h>  // Keep this library above the #defines.

#define NOP __asm__ __volatile__ ("nop\n\t")  // NOP needed to follow "skip" labels.
#define i2cSDA D4
#define i2cSCL D5
#define encoderButton D2
#define encoderA D0
#define encoderB D1

//========================================
//=============  LIBRARIES ===============
//========================================
#include <EEPROM.h>               // Author: Earle Philhower and Ivan Grokhotkov. Emulates EEPROM in program-flash 
                                  // memory. This library is part of the RP2040 board-manager package at:
                                  // ~/.arduino15/packages/rp2040/hardware/rp2040/3.1.0/libraries/EEPROM.
                                  // Do not confuse with libraries of the same name for other architectures.  
#include <Wire.h>
#include <LiquidCrystal_I2C.h>    // Author: Schwartz.
#include <Rotary.h>               // Author: Ben Buxton.
#include <PinButton.h>            // Author: Martin Poelstra (part of MultiButton library with 
                                  // long press modified by K7TFC to 1000ms).
#include <si5351.h>               // Author: Jason NT7S. V.2.1.4.
#include <Adafruit_NeoPixel.h>    // Used to confirm program upload. Can be removed.

//========================================
//======== GLOBAL DECLARATIONS ===========
//======================================== 
uint32_t calFactor = 160100;       //////*** DEFAULT VALUE. CHANGE FOR A DIFFERENT CALIBRATION FACTOR AS FOUND FROM
                                   //////*** THE ETHERKIT Si5351_calibration.ino SKETCH AS FOUND IN THE
                                   //////*** ARDUINO IDE: File/Examples/Etherkit Si5351. *OR* USE 
uint32_t correctionFactor;
uint32_t lastUsedVFO = 9085000;
uint32_t displayOffset = 4915000;    
uint32_t lastUsedBFO = 4917500;    // Starting value. Later read from "EEPROM"
int steps[] = {10,100,1000,10000}; // Tuning steps to increment frequency (in Hz) each encoder detent.
int step = 1000;                   // Step on startup. THIS *MUST* REMAIN A REGULAR *SIGNED* INTEGER!
int Power = 11;                    // For RGB LED on startup.
int PIN  = 12;                     // For RGB LED on startup.

//========================================
//============ INSTANTIATIONS ============
//========================================
Si5351 si5351;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Rotary tuningEncoder = Rotary(D0, D1);
PinButton button(encoderButton);
Adafruit_NeoPixel pixels(1, PIN, NEO_GRB + NEO_KHZ800);

////===================================== 
////******* FUNCTION: testProg ********
////=====================================
void testProg() {

  pixels.begin();
  pinMode(Power,OUTPUT);
  digitalWrite(Power, HIGH);
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(15, 25, 205));
  delay(400);
  pixels.show();
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(103, 25, 205));
  delay(400);
  pixels.show();
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(233, 242, 205));
  delay(400);
  pixels.show();
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(233, 23, 23));
  delay(400);
  pixels.show();
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(12, 66, 101));
  delay(400);
  pixels.show();
  delay(500);
    digitalWrite(Power, LOW);
}

////===================================== 
////******* FUNCTION: saveInt ********
////=====================================
void saveInt(int address, int number) {
 
  EEPROM.write(address, number >> 8);
  EEPROM.write(address + 1, number & 0xFF);
}

//////===================================== 
//////******* FUNCTION: readInt ********
//////=====================================
int readInt(int address) {

  byte byte1 = EEPROM.read(address);
  byte byte2 = EEPROM.read(address + 1);
  return (byte1 << 8) + byte2;
}

////===================================== 
////******* FUNCTION: saveUint32 ********
////=====================================
void saveUint32(int address, uint32_t number) {

  EEPROM.write(address, (number >> 24) & 0xFF);      // These lines encode the 32-bit variable into
  EEPROM.write(address + 1, (number >> 16) & 0xFF);  // 4 bytes (8-bits each) and writes them to
  EEPROM.write(address + 2, (number >> 8) & 0xFF);   // consecutive eeprom bytes starting with the
  EEPROM.write(address + 3, number & 0xFF);          // address byte. Using write() instead of update()
  EEPROM.commit();
}                                                    // to save time and because update() won't help
                                                     // save write cycles for *each* byte of the 4-byte blocks.
////===================================
//// ***** FUNCTION: readUint32 *******
///===================================
uint32_t readUint32(int address) {

  return ((uint32_t)EEPROM.read(address) << 24) +      // These lines decode 4 consecutive eeprom bytes (8-
         ((uint32_t)EEPROM.read(address + 1) << 16) +  // bits each) into a 32-bit variable (starting with the
         ((uint32_t)EEPROM.read(address + 2) << 8) +   // address byte) and returns to the calling statement.
         (uint32_t)EEPROM.read(address + 3);           // Example: uint32_t myNumber = readUint32(0);
}

////========================================
////***** FUNCTION: lcdClearLine ***********
////========================================
void lcdClearLine(byte lineNum) {

    lcd.setCursor(0,lineNum);
    lcd.print("                ");
    lcd.setCursor(0,lineNum);
}

////========================================
////***** FUNCTION: displayFreqLine ********  
////========================================
void displayFreqLine(byte lineNum, uint32_t freqValue) {
 
  String valueStr;
  String mhzOnly;
  String khzOnly;
  String hzOnly;
  
  if(lineNum == 0) {
    lcd.setCursor(13, 0);
    lcd.print("USB");
    }
    
  valueStr = String(freqValue);
  if (valueStr.length() == 8) {
    mhzOnly = valueStr.substring(0, 2);
    khzOnly = valueStr.substring(2,5);
    hzOnly = valueStr.substring(5,8);
  }
  else if (valueStr.length() == 7) {
    mhzOnly = valueStr.substring(0,1);
    khzOnly = valueStr.substring(1,4);
    hzOnly = valueStr.substring(4,7);
  }
                                                                            
   if (lineNum == 0) {
     lcd.setCursor(0, lineNum); 
   }

  if(valueStr.length() == 8) {
    lcd.print(mhzOnly + "." + khzOnly + "." + hzOnly);  // For frequencies >=10,000KHz (no leading blank space).
  }
  else {
    lcd.print(mhzOnly + "." + khzOnly + "." + hzOnly);  // For frequencies <=9,999KHz (adds leading blank space).
  }
} // End displayFreqLine()

////========================================
////***** FUNCTION: displayTuningStep ******  
////========================================
void displayTuningStep(int Step, byte lineNum) {
                  
  switch (Step) {
    case 10:
      lcd.setCursor(11, lineNum);
      lcd.print("     ");
      lcd.setCursor(12, lineNum);
      lcd.print("10Hz");
      lcd.setCursor(0, 1);
      lcd.print("P3ST");
      break;
    case 100:
      lcd.setCursor(11, lineNum);
      lcd.print("     ");
      lcdClearLine(lineNum);
      lcd.setCursor(11, lineNum);
      lcd.print("100Hz");
      lcd.setCursor(0, 1);
      lcd.print("P3ST");
      break;
    case 1000:
      lcd.setCursor(11, lineNum);
      lcd.print("     ");
      lcdClearLine(lineNum);
      lcd.setCursor(12, lineNum);
      lcd.print("1KHz");
      lcd.setCursor(0, 1);
      lcd.print("P3ST");
      break;
    case 10000:
      lcd.setCursor(11, lineNum);
      lcd.print("     ");
      lcdClearLine(lineNum);
      lcd.setCursor(11, lineNum);
      lcd.print("10KHz");
      lcd.setCursor(0, 1);
      lcd.print("P3ST");
      break;  
  } // End of switch-case

} //End displayTuningStep()

////========================================
////***** FUNCTION: setDisplayOffset() *****
////========================================
void setDisplayOffset() {

  int encoder;
  int counter = 0;
  uint32_t offsetValue = readUint32(5);

  lcdClearLine(1);

  lcd.setCursor(0,1);
  lcd.print("Offset ");
  displayFreqLine(1,offsetValue);
  //lcd.setCursor(9, 1);
  //lcd.print(offsetValue);
  button.update();
  
  while (!button.isSingleClick()) {
    //counter = 0;
    encoder = tuningEncoder.process();

    if (encoder == DIR_CCW) {
    counter++;
    }
    else if (encoder == DIR_CW) {
    counter--;
    }

    button.update();

    if (counter == 0) {   //Skip remainder of loop if there's no change in either encoder or button
      continue;
    }
    else {
      offsetValue += (counter * steps[0]);
      counter = 0;
      lcd.setCursor(7, 1);
      displayFreqLine(1,offsetValue);
    }
  }

  displayOffset = offsetValue;
  saveUint32(5, displayOffset);
  
  // reset BFO display
    lcdClearLine(0);
    lcdClearLine(1);
    displayTuningStep(steps[0], 0);
    lcd.setCursor(0, 1);
    lcd.print("BFO:");
    lcd.setCursor(7, 1);
    displayFreqLine(1, lastUsedBFO);  

    return;
}

////========================================
////******** FUNCTION: bfoFreq *********
////========================================
void bfoFreq() { 

    button.update();

    int bfoStep = steps[0];
    uint32_t bfoValue = lastUsedBFO;
    int counter;

    lcdClearLine(0);
    lcdClearLine(1);
    displayTuningStep(bfoStep, 0);
    lcd.setCursor(0, 1);
    lcd.print("BFO:");
    lcd.setCursor(7, 1);
    displayFreqLine(1, bfoValue);  

    while (!button.isLongClick()) {       // Loop until a long press-and-release of encoder button
      unsigned char encoder;
      counter = 0;
      button.update();

    if (button.isSingleClick()) {
      setDisplayOffset();
    }

    // Read tuning encoder and set Si5351 accordingly
    /////////////////////////////////////////////////
    encoder = tuningEncoder.process();
    if (encoder == DIR_CCW) {
    counter++;
    }
    else if (encoder == DIR_CW) {
    counter--;
    }
 
    // Skip to end of loop() unless there's change on either encoder or button
    // so LCD and Si5351 aren't constantly updating (and generating RFI).
    if (counter == 0 && !button.isClick()) {
      goto skip;
    }
    else {
      bfoValue += (counter * bfoStep);
      lastUsedBFO = bfoValue;
    }

   si5351.set_freq(lastUsedBFO * 100, SI5351_CLK2); //BFO frequency set within the loop for real-time adjustment.
   lcd.setCursor(7, 1);
   displayFreqLine(1,bfoValue);  //Parameters: LCD line (0 or 1), frequency value.

  skip: 
  NOP;
  } // End of while loop.
  
  // At this point (after long press-and-hold of encoder button),
  // save new BFO freq to "EEPROM" and restore the VFO display before returning.

  lastUsedBFO = bfoValue;
  saveUint32(0, lastUsedBFO);  

  lcdClearLine(0);
  lcdClearLine(1);
  lcd.setCursor(0,1);
  displayFreqLine(0,lastUsedVFO + displayOffset);  
  displayTuningStep(step, 1);
  lcd.setCursor(0, 1);
  lcd.print("P3ST");
 
  return;
} // End of bfoFreq() 

//========================================
//*** FUNCTION: si5351CorrectionFactor ***
//========================================
void si5351CorrectionFactor() {

  int correctionStep = steps[0];
  int counter;
  uint32_t currentVFOnum = lastUsedVFO;
  // uint32_t correctionFactor = 0;
  //float correctionFactorRaw = 0;
  //float correctionFactorFloat;

  String sp10 = "          ";

  lcdClearLine(0);
  lcd.print("Set known Freq:");
  lcdClearLine(1);
  displayFreqLine(1, currentVFOnum + displayOffset);    
  
  button.update();
  while (!button.isDoubleClick()) {
     unsigned char encoder;
    counter = 0;
    button.update();
      
    encoder = tuningEncoder.process();
    if (encoder == DIR_CCW) {
    counter++;
    }
    else if (encoder == DIR_CW) {
    counter--;
    }
 
    if (counter == 0 && !button.isClick()) {
      continue;
    }
    else {
      currentVFOnum += (counter * correctionStep);
    }

   lcd.setCursor(0, 1);
   displayFreqLine(1,currentVFOnum + displayOffset);  
    
    //button.update();
  } // end while loop

  correctionFactor = ((lastUsedVFO - currentVFOnum) * -1); // This is the factor in whole Hz.
  si5351.set_correction(correctionFactor * 100, SI5351_PLL_INPUT_XO);   // Multiply by 100 to convert to 100ths of Hz the library requires. 
  lastUsedVFO = currentVFOnum;
  saveUint32(10, (correctionFactor * 100) + 10000);  // Pad correctionFactor with 10000 to prevent integer underflow
                                                  // of negative values.
  //Now fully restore normal VFO display
  lcdClearLine(0);
  displayFreqLine(0,lastUsedVFO + displayOffset);
  lcd.setCursor(13,0); lcd.print("USB");
  lcd.setCursor(0, 1); lcd.print(sp10); lcd.setCursor(0, 1); lcd.print("P3ST");
  displayTuningStep(step, 1);

  return;
}

////========================================
////****** FUNCTION: printVariables ********
////========================================
void printVariables() {            // .

  Serial.print("displayOffset is: "); Serial.println(displayOffset); 
  Serial.print("correctionFactor is: "); Serial.println(correctionFactor); // This is the value with padding
  Serial.print("correctionFactor saved is: "); Serial.println(readUint32(10));
  
  Serial.print("lastUsedBFO is: "); Serial.println(lastUsedBFO);
  Serial.end();
  
  return;
}

////========================================
////******** FUNCTION: setup ***************
////========================================
void setup() {

  uint32_t displayOffsetTest;
  uint32_t bfoTest;
  uint32_t calFactorTest;
  
  lcd.init();
  lcd.backlight();

  Wire.begin();
  EEPROM.begin(256);
  Serial.begin(115200);

  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  //////////////////////////////////////
  calFactorTest = readUint32(10);
  if (calFactorTest == 0) {
    saveUint32(10, calFactor + 10000);  // 10000 padding added to prevent underflow for negative cal factors.
  }
   calFactor = readUint32(10);
   si5351.set_correction(calFactor - 10000, SI5351_PLL_INPUT_XO); // Removing the 10000 padding.
  /////////////////////////////////////
  Serial.print("The displayOffset default is: "); Serial.println(displayOffset);
  displayOffsetTest = readUint32(5);
  if (displayOffsetTest == 0) {         // Tests for first-time use (no offset saved in eeprom).
    saveUint32(5, displayOffset);       // Saves default value in eeprom.
  }
  displayOffset = readUint32(5);
  Serial.print("After being read from EEPROM: "); Serial.println(displayOffset);
  ///////////////////////////////////
  bfoTest = readUint32(0);
  if (bfoTest == 0) {              // Tests for first-time use (no BFO saved in eeprom).
    saveUint32(0, lastUsedBFO);    // Saves default value in eeprom.
  }
  lastUsedBFO = readUint32(0);
  
  si5351.set_freq(lastUsedBFO * 100, SI5351_CLK2);
  si5351.set_freq(lastUsedVFO * 100, SI5351_CLK0);
  displayFreqLine(0,lastUsedVFO + displayOffset);  //Parameters: LCD line (0 or 1), frequency value.
  displayTuningStep(step, 1);      //Parameters: displayTuningStep(int Step, byte lineNum)
  lcd.setCursor(0, 1);
  lcd.print("P3ST");
  
  testProg();    // A series of flashing RGB colors on startup
   
} // End of setup()

//========================================
//********* FUNCTION: (main)loop *********
//========================================
void loop() {

  unsigned char encoder;
  int counter = 0;
  char x;

  // Check serial input for print-variables request.
  // Called by any character (including [RETURN]) sent from serial monitor.
  if (Serial.available() > 0) {
    x = Serial.read();
    printVariables();
  }

  // Read tuning encoder and set Si5351 accordingly
  /////////////////////////////////////////////////
  button.update();
  encoder = tuningEncoder.process();  

// Button activity on tuning encoder          
   if (button.isLongClick()) {                           
    bfoFreq();                // Long press-and-release will call BFO-setting function.
   }
   else if(button.isDoubleClick()) {
    si5351CorrectionFactor();
   }
   else if(button.isSingleClick() && step == steps[3]) {    // These else-if statements respond to single (short click) button pushes to step-through   
    step = steps[0];                          // the tuning increments (10Hz, 100Hz, 1KHz, 10KHz) for each detent of the tuning encoder.
    displayTuningStep(step, 1);               // A short click on 10KHz loops back to 10Hz. The default step is 1KHz.
   }
   else if (button.isSingleClick() && step == steps[0]) {
    step = steps[1];
    displayTuningStep(step, 1);
    }
   else if (button.isSingleClick() && step == steps[1]) {
    step = steps[2];
    displayTuningStep(step, 1);
    }
   else if (button.isSingleClick() && step == steps[2]) {
    step = steps[3];
    displayTuningStep(step, 1);
   }

   uint32_t vfoValue = lastUsedVFO;

// Skip to end of loop() unless there's change on either encoder or button
  // so LCD and Si5351 aren't constantly updating (and generating RFI).
  if (!encoder && !button.isClick()) {
    goto skip;
  }
  else if (encoder == DIR_CCW) {
    counter++;
  }
  else if (encoder == DIR_CW) {
    counter--;
  }
    vfoValue += (counter * step);
    si5351.set_freq(vfoValue * 100, SI5351_CLK0);  // Si5351 is set in 0.01 Hz increments. "vfoValue" is in integer Hz.
    lastUsedVFO = vfoValue;

  // LCD display ///////////////////
  displayFreqLine(0,lastUsedVFO + displayOffset);
 
skip:   // This label is where the loop goes if there are no inputs.
NOP;    // C/C++ rules say a label must be followed by something. This "something" does nothing.

}  // closes main loop() 

   
