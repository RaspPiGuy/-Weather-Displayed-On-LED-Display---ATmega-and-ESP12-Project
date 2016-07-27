/*  TimesSquareScroll_V4R1.ino   
    MJL  thepiandi.blogspot.com
    
Specific to Mike Oshinski's Big Display: one row of 18 5x7 character LED displays

This iteration displays message starting at the right column of the display.
Characters are added to the right and the displays scrolls left one column
at a time.  When all of the characters are displayed, display starts over.

When sketch is launched, the message selected by:
 ComposeMessageSendToSerialEEPROM_VxRx.ino
is displayed. When the switch is pressed, the next message is selected, etc.  

The switch causes an interrupt.

Version 1:
Revision 1:  9/7/2015
Revision 2:  9/10/2015 - added 18 spaces after displayed data so displays clears after
  eash pass.
Revision 3:  9/18/2015 - changed port lines to accomodate serial EEPROM
Revision 4:  9/23/2015 - made the gap between the end of the message and the next
  start of a message a variable so it can be easily changed
Revision 5:  9/27/2015 - now row data comes from EEPROM dynamically

Version 2:
Revision 1:  9/30/2015 - incorporates serial EEPROM to get message
Revision 2: 10/16/2015 - lurn off LEDs when sending data to latches

Version 3:
Revision 1: 11/1/2015  - incorporates the switch to cycle through messeges
Revision 2: 11/2/2015  - removed unnecessary clear column from display function
Revision 3: 11/16/2015 - removed unnecessary while(1), minor cosmetic changes

Version 4:
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

int physicalColumn;
int noOfChars;
byte rowData[90];
int displayCount = 10;  //determines the time each line is displayed
int gap =14; //determines the gap between end of message and start of next

/*-----------------------------delayFunction----------------------------*/
//determines the time the LEDs of one column are displayed
void delayFunction(){
  delayMicroseconds(250);
  //delay(1);
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
//This disables and enables the display
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

/*------------------------------------ClearDisplay--------------------------------------------*/
void ClearDisplay(){
  int latch, column;

  physicalColumn = 0;
  for (latch = 0; latch < 6; latch++){
    PORTD = 0;
    StrobeLatch(latch);
  }
  for (column = 0; column < 15; column++){  
    AdvanceColumn();
  }
}

/*---------------------------------------------SPIoperation------------------------------------*/
byte SPIoperation(byte data)      //used for reading data and writing data and opcodes
{  
  SPDR = data;                    // Start the transmission
  while (!(SPSR & (1<<SPIF)));    // Wait the end of the transmission
  return SPDR;       // return the received byte
}

/*-------------------------------------------ReadSerialEEPROM-----------------------------------*/
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

/*------------------------------InitializeDisplay---------------------------------*/
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

    
  InitializeRowData();
  
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
/*----------------------------ShiftRowData-------------------------------*/
void ShiftRowData(){
  for (int i = 0; i < 90; i++){
    rowData[i] = rowData[i + 1];
  }
}
/*--------------------------InitializeRowData-------------------------------*/
void InitializeRowData(){
  for (int i = 0; i < 90; i++){
    rowData[i] = 0;
  }
}


/*----------------------------Display------------------------------*/
void Display(){
  int latch, column, group;
  
  //group 0 = displays 0, 3, 6, 9, 12, 15;  physicalColumns 0 - 5
  //group 1 = displays 1, 4, 7, 10, 13, 16;  physicalColumns 6 - 10
  //group 2 = displays 2, 5, 8, 11, 14, 17;  physicalColumns 11 - 15
  
  physicalColumn = 0;
  DisableRowOutputs(true); //Turn off LEDs
  for (group = 0; group < 3; group++){
    for (column = 0; column < 5; column++){
      for (latch = 0; latch < 6; latch++){
        PORTD = rowData[15 * latch + 5 * group + column];
        StrobeLatch(latch);
       }
      AdvanceColumn();
      DisableRowOutputs(false); //Turn LEDs on
      delayFunction();
    }
  }  
}

/*___________________________________SetUp__________________________________*/  
void setup() {
  int i;
  int j;
  int charAddress;
  int charIndex;
  byte charToDisplay;
  int messageNumber;
     
  InitializeDisplay(); 
   
  messageNumber = eeprom.EEPROM_read(1021);
  noOfChars = 256 * eeprom.EEPROM_read(1022);
  noOfChars += eeprom.EEPROM_read(1023);

  while(1){    
    charAddress = (128 * (16 * messageNumber + 1) - 1);
    for (i = 0; i < noOfChars; i++){
      for (charIndex = 0; charIndex < 6; charIndex++){
        if (charIndex == 0){
          charAddress++;
          charToDisplay = ReadSerialEEPROM(charAddress);
        }
        ShiftRowData();
        if (charIndex != 5){
          rowData[89] = (eeprom.EEPROM_read((5 * charToDisplay) + charIndex)) ^ 0x7f;
        }
        else{
          //display space after character
          rowData[89] = 0;
        }
        for (j = 0; j < displayCount; j++){
          Display();
        }
      }     
    }
    
    for (i = 0; i < 6* gap; i++){
      ShiftRowData();
      rowData[89] = 0;
      for (j = 0; j < displayCount; j++){
        Display();
      } 
    }
  }
}

void loop() {
}
