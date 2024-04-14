// This will clear (set to zero) the EEPROM locations used in the
// P3ST_Xiao.ino sketch (both the Carney and Marks versions). This
// will make the Xiao "look" like it's never been programmed before.



#include <EEPROM.h>

uint32_t bfoEEPROM = 0;
uint32_t offsetEEPROM = 0;
uint32_t calEEPROM = 0;
uint32_t address15 = 0;  // EEPROM address 15 as used in Peter Marks version of P3ST_Xiao.ino

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

void setup() {

  EEPROM.begin(256);

  Serial.begin(115200);
  while(!Serial);

  Serial.println("Writing all zeros to BFO EEPROM");
  saveUint32(0, bfoEEPROM);

  Serial.println("Writing all zeros to Offset EEPROM");
  saveUint32(5, offsetEEPROM);

  Serial.println("Writing all zeros to Correction EEPROM");
  saveUint32(10, calEEPROM);

Serial.println("Writing all zeros to EEPROM address 15");
  saveUint32(15, address15);  

  Serial.print("BFO EEPROM now reads: "); Serial.println(readUint32(0));
  Serial.print("Offset EEPROM now reads: "); Serial.println(readUint32(5));
  Serial.print("Corection EEPROM now reads: "); Serial.println(readUint32(10));
  Serial.print("EEPROM address 15 now reads: "); Serial.println(readUint32(15));

  Serial.println(); Serial.println("Done");
}

void loop() {
  // put your main code here, to run repeatedly:

}
