/* VerticalDisplayScrollsUp_V3R1.ino  

MJL  thepiandi.blogspot.com

Specific to Mike Oshinski's Big Display: one row of 18 5x7 character LED displays

Takes a message stored in the EEPROM and parses the message so that no words are
split between lines.  Displays the message line by line.  Starts with the bottom
row of the display line displaying that row then each row above until the 
top row of the line is displayed.  Has the effedt of scrolling down one row
at a time.  When top row is displayed, display moves to the next line.

Sketch runs continously.

When sketch is launched, the message selected by:
 ComposeMessageSendToSerialEEPROM_VxRx.ino is displayed. 

Inherits from ParseAndDisplayMessage_V3R1.ino

Version 1:
Revision 1:  11/29/2015

Version 2:
Revision 1:  12/1/2015 - Stops the display momentarily when the complete text is displayed.

Version 3:
Revision 1: 7/19/2016  - Since the switch was removed to recover a GPIO pin
  references to the switch are removed.

*/

#include <avr/io.h>
#include "EEPROM_Functions.h"


EEPROM_FUNCTIONS eeprom;

/*----------------------------Global Variables-----------------------------*/
//SPI opcodes
#define WREN  6  //Write Enable
#define WRDI  4  //Write Disable
#define RDSR  5  //Read Status Register
#define WRSR  1  //Write Status Register
#define READ  3  //Read Data
#define WRITE 2  //Write Data

int noOfChars;
int physicalColumn;
byte characterData[18];
byte rowData[90];
byte verticalDisplay[90];
int displayCount = 7;  //determines the time each line is displayed
int displayCount1 = 50; //determines the time for stopped display

/*-----------------------------delayFunction----------------------------*/
//determines the time the LEDs of one column are displayed
void delayFunction(){
  delay(1);
}

/*----------------------------StrobeLatch--------------------------------*/
void StrobeLatch(int strobe){
  byte lowerBits = PORTC & 0b00100011;

  //set selected strobe high then low     
  PORTC = lowerBits + 4 * (strobe + 1);
  
  delayMicroseconds(1);
  PORTC = lowerBits;  //selects strobe 0 whidh does not exist
  
}

/*----------------------------ClearColumn----------------------------------*/
void ClearColumn(){
  //Turns all columns off
  PORTC &= ~3;
  delayMicroseconds(1);
  
  for (int i = 0; i < 16; i++){
    PORTC |= 2;
    delayMicroseconds(1);
    PORTC &= ~3;
    delayMicroseconds(1);
  }
}

/*----------------------------AdvanceColumn--------------------------------*/
void AdvanceColumn(){
  //Advance to next column

  PORTC &= ~3;
  PORTC |= (physicalColumn == 0);
  delayMicroseconds(1);
  PORTC |= 2;
  delayMicroseconds(1);  
  PORTC &= ~3;  

  physicalColumn++; 
}

/*----------------------------ClearRowData--------------------------------*/
//have not seen the need to use this function
void ClearRowData(){
  //true clears all row data
  //invokes Clear line
  //create a 1 microsecond positive clear pulse
  
  PORTB &= ~(1 << 2); 
  delayMicroseconds(1);
  PORTB |= (1 << 2);
}

/*----------------------------DisableRowOutputs--------------------------------*/
//This disables the display
//Have not seen a need to disable the display
void DisableRowOutputs(boolean disableRows){
  //true disables row outputs
  //invokes Output_Enable line 
  if (disableRows){
    PORTB |= (1 << 0);
  }
  else{
    PORTB &= ~(1 << 0);
  }
}

/*----------------------------Display------------------------------*/
//where all the magic happens
void Display(){
  int latch, column, group;
  int i;
  int frame;
  int persistence;
  
  //group 0 = displays 0, 3, 6, 9, 12, 15;  physicalColumns 0 - 5
  //group 1 = displays 1, 4, 7, 10, 13, 16;  physicalColumns 6 - 10
  //group 2 = displays 2, 5, 8, 11, 14, 17;  physicalColumns 11 - 15
    
  for (frame = 0; frame < 8; frame++){
    if (frame == 7){
      persistence = displayCount1;
    }
    else{
      persistence = displayCount;
    }
    for (i = 0; i < 90; i++){
      verticalDisplay[i] >>= 1;
      if (rowData[i] & (1 << (frame))){
        verticalDisplay[i] += 0x80;
      }
    }
    for (i = 0; i < persistence; i++){
      ClearColumn();
      physicalColumn = 0;
      for (group = 0; group < 3; group++){
        for (column = 0; column < 5; column++){
          for (latch = 0; latch < 6; latch++){
            PORTD = verticalDisplay[15 * latch + 5 * group + column];
            StrobeLatch(latch);
           }
          AdvanceColumn();
          delayFunction();
        }
      }
    }
  }  
}
/*----------------------------ClearDisplay------------------------------*/
void ClearDisplay(){
  //byte data = 0x00;
  int latch, column;

  physicalColumn = 0;
  for (latch = 0; latch < 6; latch++){
    PORTD = 0;
    //PORTD = data;
    StrobeLatch(latch);
  }
  for (column = 0; column < 15; column++){  
    AdvanceColumn();
   // delayFunction();
  }
}
/*----------------------------PopulateRowDataArray--------------------------*/
void PopulateRowDataArray(byte characterData[]){
  int characterCount, columnCount;
  int addressInEEPROM;
  
  for (characterCount = 0; characterCount < 18; characterCount++){
    addressInEEPROM = 5 * characterData[characterCount];
    
    for (columnCount = 0; columnCount < 5; columnCount++){
      rowData[5 * characterCount + columnCount] = eeprom.EEPROM_read(addressInEEPROM + columnCount);   
      rowData[5 * characterCount + columnCount] ^= 0x7F;  //will remove this later
    }
  }
}

/*---------------------------------------------SPIoperation------------------------------------*/
byte SPIoperation(byte data)      //used for reading data and writing data and opcodes
{  
  SPDR = data;                    // Start the transmission
  while (!(SPSR & (1<<SPIF)));    // Wait the end of the transmission
  return SPDR;       // return the received byte
}

/*---------------------------------------------ReadSerialEEPROM------------------------------------*/
byte ReadSerialEEPROM(int EEPROM_address)
{
  int data;
  
  PORTB &= ~(0b00000100);  // SS Low
  SPIoperation(READ); //transmit read opcode
  
  SPIoperation(EEPROM_address>>8);   //send MSByte address first
  SPIoperation(EEPROM_address);      //send LSByte address
  
  data = SPIoperation(0xFF); //get data byte. Must write something to read something
  PORTB |= 0b00000100;  // SS High
  
  return data;
}
/*----------------------------InitializeDisplay-------------------------------*/
void InitializeDisplay(){
  byte bitBucket;

  //  Latch Control PORTC2: A0, PORTC3: A1, PORTC4: A3, PORTB0: OutputEnable, PORTB1: Clear row
  //  RoW Control: PORTD0 through PORTD7: Row 0 through Row 7
  //  Column Control: PORTC0: SerialIn, PORTC1: Column_Clock
  //  SPI Control: PORTB2: SS, PORTB3: MOSI, PORTB4: MISO (input), PORTB5: SCK
  
  //Set PORT B Bits 0 - 3, 5 as outputs  
  DDRB = 0b00101111;
  //Set PORT D Bits 0 - 7 as outputs
  DDRD = 0b11111111;
  //Set PORT C Bits 0 - 4 as outputs
  DDRC = 0b00011111;
  
  //Initialize Ports B, C, and D
  PORTB = 0b00000100;  //SPI SS High
  PORTC = 0;
  PORTD = 0;

  //Clear out any column data
  ClearColumn();
  //Send 0 to all columns in turn
  ClearDisplay(); 
  
  //enable display
  DisableRowOutputs(false); 
  
  /* Setting the SPI control register: SPCR  
    Bit 7 - SPIE: interrupt disabled (0)
    Bit 6 - SPE: spi enabled (1)
    Bit 5 - DORD: most significant bit first (0)
    Bit 4 - MSTR: Arduino in master mode (1)
    Bit 3 - CPOL: sclk low when idle (0)
    Bit 2 - CPHA: sample data on rising edge of sclk (0)
    Bits 1 and 0 - SPR1 and SPRO system clock/4 rate (fastest) (0)
  */
  SPCR = 0b01010000;

  // Clear out the shift register and status register
  bitBucket=SPSR;
  bitBucket=SPDR;
  
}

/*-------------------------------nextMessage---------------------------------*/
int nextMessage(int messageToDisplay){
  int address;
  byte messageLengthUpperByte, messageLengthLowerByte;

  while(1){
    messageToDisplay++;
        
    if (messageToDisplay > 30){
      messageToDisplay = 0;
    }
    address = 2 * messageToDisplay;
  
    messageLengthUpperByte = ReadSerialEEPROM(address);
    messageLengthLowerByte = ReadSerialEEPROM(address + 1);
  
    noOfChars = 256 * messageLengthUpperByte + messageLengthLowerByte;

    if(noOfChars){  
      return messageToDisplay;
    }
  }
}

//*___________________________________SetUp__________________________________*/  
void setup() {
  //Serial.begin(9600);
  
  int i, j, k;
  int charCount;
  int messageNumber;
  int lineStartIndex, lineEndIndex;
  int lastSpaceIndex, countAlongLine;
  int numberOfLineChars;
  int leadingSpaces;
  int charAddress;
  byte charToDisplay;
  
  messageNumber = eeprom.EEPROM_read(1021);
  noOfChars = 256 * eeprom.EEPROM_read(1022);
  noOfChars += eeprom.EEPROM_read(1023);
 
  InitializeDisplay();
 
  for (i = 0; i < 90; i++){
    verticalDisplay[i] = 0;
  }
  
  while(1){ 
    charAddress = 128 * (16 * messageNumber + 1);
    lineStartIndex, lineEndIndex = 0;
    charCount = 0; //index of character in charData[] currently being addressed
  
    //This handles everything except the last line
    while (charCount < (noOfChars - 18)){    //at start of new line
      countAlongLine = 0;
      lineStartIndex = charCount;
      
      //accounts for spaces at start of line
      while(ReadSerialEEPROM(charAddress + charCount) == 32){
        charCount++;
        countAlongLine++;
        lineStartIndex++;
      }
      do {
        do{
          charCount++;
          countAlongLine++;     //goes from 0 to 18 as we progress along a line 
        }while(ReadSerialEEPROM(charAddress + charCount) != 32);  //stops at a space
        
        if (countAlongLine < 19){
          lastSpaceIndex = charCount;  //lastSpaceIndex shows index last space within data
        }
      }while (countAlongLine < 19);  //when this breaks out we have come to the end of a line
      
      lineEndIndex = lastSpaceIndex;
      charCount = lastSpaceIndex + 1;
      
      //to center display and adding necessary spaces
      numberOfLineChars = lineEndIndex - lineStartIndex + 1;
      leadingSpaces = ((18 - numberOfLineChars) / 2.0) + 0.5;
      
      for (i = 0; i <  leadingSpaces; i++){
        characterData[i] = 32;
      }
      
      for (j = 0; j < numberOfLineChars; j++){
        characterData[j + i] = ReadSerialEEPROM(charAddress + lineStartIndex);
        lineStartIndex++;
      }
      
      for (k = (j + i); k < 18; k++){
        characterData[k] = 32;
      }
      
      PopulateRowDataArray(characterData);
      Display();
  
      lineStartIndex = lastSpaceIndex;   
    }
    
    //handles the last line here
    numberOfLineChars = noOfChars - charCount;
    
    leadingSpaces = ((18 - numberOfLineChars) / 2.0) + 0.5;
    
    for (i = 0; i <  leadingSpaces; i++){
      characterData[i] = 32;
    }
    
    for (j = 0; j < numberOfLineChars; j++){
      characterData[j + i] = ReadSerialEEPROM(charAddress + charCount);
      charCount++;
    }
  
  
    for (k =  (i + j); k < 18; k++){
      characterData[k] = 32;
    }
    
    PopulateRowDataArray(characterData);
    Display();
    
    //leave blank display at end of message
    for (k = 0; k < 18; k++){
      characterData[k] = 32;
    }
    
    PopulateRowDataArray(characterData);
    Display();
  }
}

void loop() {
}
