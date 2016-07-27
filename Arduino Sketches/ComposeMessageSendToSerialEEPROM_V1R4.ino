/*  SerialEEPROM_Operations_V1R4  MJL thepiandi.blogspot.com
User can store up to sixty two 1024 byte messages within serial EEPROM for the 
purpose of displaying them on the large 18 character display.

User can compose message, read messages, erase a message, erase all messages, and select
the message to be displayed.

The number of the message to be displayed is stored in byte 1021 ot the ATmega's EEPROM.
The message length is stored in bytes 1022 (upper byte) and 1023 (lower byte).

Version 1:  9/30/2015
Revision 1:  
Revision 2:  10/9/2015  - When selectiong a message,prints the message to the screen
Revision 3:  11/1/2015  - Corrected number of messages from 62 to 31.
                        - If message has been deleted, and has been seleted to be 
                        displayed, replaces messge to be displayed with "Message 
                        Deleted", located in message slot 32.


Revision 4:  7/22/2016  - To allow for weather display at message slot 30, must reduce
                        message slots by 2
*/
#include <avr/io.h>
#include "InputFromTerminal.h"
#include "EEPROM_Functions.h"

TERM_INPUT term_input;
EEPROM_FUNCTIONS eeprom;

/*---------------------------------------------global variables-------------------------------------*/
//opcodes
#define WREN  6  //Write Enable
#define WRDI  4  //Write Disable
#define RDSR  5  //Read Status Register
#define WRSR  1  //Write Status Register
#define READ  3  //Read Data
#define WRITE 2  //Write Data

char characterData[1500];
int noOfChars;
int address;

/*---------------------------------------------how_many-------------------------------------*/
long how_many(long lower_limit, long upper_limit){
  long menu_choice;
  boolean good_choice;
  Serial.read();
  good_choice = false;
  do{
    menu_choice = term_input.termInt();
    if (menu_choice >= lower_limit && menu_choice <= upper_limit){
      good_choice = true;
    }
    else{
      Serial.println(F("Please Try Again"));
    }
  }while (!good_choice);

  return menu_choice; 
}

/*---------------------------------------------yes_or_no-------------------------------------*/
boolean yes_or_no(){
  boolean yes_no = false;
  char keyboard_entry[5];
  do{
    Serial.println(F("Please enter Y or y, or N or n"));        
    while (!Serial.available() > 0);
    Serial.readBytes(keyboard_entry, 1);
    if (keyboard_entry[0] == 'Y' || keyboard_entry[0] == 'y'){
      yes_no = true;
    }
  }while((keyboard_entry[0] != 'y') && (keyboard_entry[0] != 'Y') && (keyboard_entry[0] != 'n') && (keyboard_entry[0] != 'N'));
  return yes_no;
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

/*---------------------------------------------InitializeSPI-------------------------------------*/
void InitializeSPI(){
  byte bitBucket;

  //Data direction:  SCK, MOSI, SS outputs
  DDRB |= 0b00101100;
  //Data direction MISO input
  DDRB &= ~(0b00010000);
  
  PORTB |= 0b00000100;  // SS High
  
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
/*-----------------------------------FindAvailableSlot-----------------------------------*/
int FindAvailableSlot(){
  byte availableSlot;
  
  for (address = 0; address < 124; address += 2){
    availableSlot = ReadSerialEEPROM(address);
    availableSlot += ReadSerialEEPROM(address + 1);
    if (!(availableSlot)){
      break;
    }
  }
  return (address / 2);
}
/*-----------------------------------SelectMessageToDisplay------------------------------*/
void SelectMessageToDisplay(){
  byte messageToDisplay;
  byte messageLengthUpperByte, messageLengthLowerByte;
  
  Serial.println(F("\nWhich Message To Dislay (1 to 29)?"));
  
  messageToDisplay = how_many(1, 29);
  messageToDisplay--;
  
  // get the length of the message 
  address = 2 * messageToDisplay;
  messageLengthUpperByte = ReadSerialEEPROM(address);
  messageLengthLowerByte = ReadSerialEEPROM(address + 1);
  
  noOfChars = 256 * messageLengthUpperByte + messageLengthLowerByte;
  
  if (noOfChars){
    //write to ATmega328P's internal EEPROM
    eeprom.EEPROM_write(1021, char(messageToDisplay));
    eeprom.EEPROM_write(1022, char(messageLengthUpperByte));
    eeprom.EEPROM_write(1023, char(messageLengthLowerByte));

    

    Serial.print(F("Message selected: "));
    Serial.print(messageToDisplay + 1);
    Serial.print(F("\tMessage length: "));
    Serial.println(noOfChars);
    //Serial.println('\n');

    ReadMessageFromSerialEEPROM_1(messageToDisplay);    
    
  }
  else{
    Serial.println(F("No message there\n"));
  }
}
/*-----------------------------------WriteToSerialEEPROM---------------------------------*/
void WriteToSerialEEPROM(){
  int pages;
  int pageNo;
  int i, j;
  int messageNumber;
  int slotAddress;
  int startingPage;
  
  slotAddress = FindAvailableSlot();

  if (slotAddress < 29){
    Serial.print(F("\nThis will be message number: "));
    Serial.print(slotAddress + 1);
    Serial.println('\n');
    
    
    Serial.println(F("\nInput characters (1500 max)"));
  
    while (!Serial.available() > 0);
    noOfChars = Serial.readBytesUntil('\n', characterData, 1500); 
    pages = (noOfChars / 128) + 1;
     
    //Print out so user can see what will be displayed
    Serial.println(F("\n  Here is what will be displayed:"));
    j = 0;
    for (i = 0; i < noOfChars; i++){
      Serial.print(characterData[i]);
      j++;
      if ((characterData[i] == 32) & (j > 80)){
        Serial.print('\n');
        j = 0;
      }
    }
    Serial.println(F("")); 
   
    Serial.println("\nHappy With the Results?");
    if (yes_or_no()){
  
      Serial.println(F("\nWriting to the EEPROM"));
      //write message length in message index (page 0)
      address = 2 * slotAddress;
      
      PORTB &= ~(0b00000100);  // SS Low
      SPIoperation(WREN); //write enable  
      PORTB |= 0b00000100;  // SS High   
      
      PORTB &= ~(0b00000100);  // SS Low
      SPIoperation(WRITE); //transmit write opcode
      
      SPIoperation(address >> 8);   //send MSByte address first
      SPIoperation(address);        //send LSByte address
      
      SPIoperation(noOfChars >> 8); //write upper byte of data length
      SPIoperation(noOfChars);      //write lower byte of data length
      
      PORTB |= 0b00000100;  // SS High
      delay(10);
      
      //write message data to Serial EEPROM
      i = 0;
       
      startingPage = (16 * slotAddress) + 1;
      for (pageNo = 0 ; pageNo < pages; pageNo++){
        address = 128 * (startingPage + pageNo);
                
        PORTB &= ~(0b00000100);  // SS Low
        SPIoperation(WREN); //write enable  
        PORTB |= 0b00000100;  // SS High   
        
        PORTB &= ~(0b00000100);  // SS Low
        SPIoperation(WRITE); //transmit write opcode
        
        SPIoperation(address >> 8);   //send MSByte address first
        SPIoperation(address);      //send LSByte address
        
        //write data
        j = 0;
        while ((i < noOfChars) & (j < 128)){
          SPIoperation(characterData[i]); //write data byte
          i++;
          j++;
        }
        PORTB |= 0b00000100;  // SS High
        
        //wait for eeprom to finish writing
        delay(10);
      }
      Serial.println(F("\nDone Writing to Serial EERPOM\n\n"));
  
    }
    else{
      Serial.println(F("Did not write to Serial EEPROM"));
    } 
    Serial.println("");
  }
  else{
    Serial.println(F("\nNo room for more messages!\n\n"));    
  }
}

/*----------------------------------ReadMessageFromSerialEEPROM_1-------------------------------*/
void ReadMessageFromSerialEEPROM_1(int messageToRead){
  int i, j;
  byte eepromOutputData;
  
  // get the length of the message 
  address = 2 * messageToRead;
  noOfChars =  256 * ReadSerialEEPROM(address);
  noOfChars += ReadSerialEEPROM(address + 1);
  
  if (noOfChars){  
    Serial.print(F("\n  Contents of message "));
    Serial.print(messageToRead + 1);
    Serial.println(F(":"));
    j = 0;
    address = 128 * (16 * messageToRead + 1);
    for (i = 0; i < noOfChars; i++){
      eepromOutputData = ReadSerialEEPROM(address + i);
      Serial.print(char(eepromOutputData));    
      j++;
      if ((eepromOutputData == 32) & (j > 80)){
        Serial.print('\n');
        j = 0;
      }
    }
    Serial.println('\n'); 
  }
  else{
    Serial.println(F("No message there\n"));
  }
  
}
/*----------------------------------ReadMessageFromSerialEEPROM---------------------------------*/
void ReadMessageFromSerialEEPROM(){
  int messageToRead;
  
  Serial.println(F("\nWhich Message to read (1 to 29)?"));
  
  messageToRead = how_many(1, 29);
  messageToRead--;

  ReadMessageFromSerialEEPROM_1(messageToRead);
  
}
/*----------------------------------EraseOneFromSerialEEPROM---------------------------------*/
void EraseOneFromSerialEEPROM(){
  byte eepromOutputData;
  int messageToErase;
  int messageToDisplay;
  
  Serial.println(F("\nWhich Message to read (1 to 29)?"));
  
  messageToErase = how_many(1, 29);
  messageToErase--;
  
   // get the length of the message 
  address = 2 * messageToErase;
  noOfChars =  256 * ReadSerialEEPROM(address);
  noOfChars += ReadSerialEEPROM(address + 1);
  
  if (noOfChars){
    Serial.print(F("\nSure You Want To Erase Message "));
    Serial.print(messageToErase + 1);
    Serial.println(F("?"));
    if (yes_or_no()){    
      PORTB &= ~(0b00000100);  // SS Low
      SPIoperation(WREN); //write enable  
      PORTB |= 0b00000100;  // SS High   
      
      PORTB &= ~(0b00000100);  // SS Low
      SPIoperation(WRITE); //transmit write opcode
      
      SPIoperation(address >> 8);   //send MSByte address first
      SPIoperation(address);      //send LSByte address
      SPIoperation(0); //write data byte
      SPIoperation(0); //write data byte
      PORTB |= 0b00000100;  // SS High
      
      //wait for eeprom to finish writing
      delay(10);
      
      Serial.println(F("\nMessage Erased from Serial EERPOM\n\n")); 

      //find out of the erased message is the one selected to be displayed
      //if so point message to be displayed to "Message Deleted" in slot 32
      messageToDisplay = eeprom.EEPROM_read(1021);
      if (messageToDisplay == messageToErase){
        eeprom.EEPROM_write(1021, char(31));
        eeprom.EEPROM_write(1022, char(0));
        eeprom.EEPROM_write(1023, char(15));
      }
    }
    else{
      Serial.println(F("\nDid Not Erase Message From Serial EERPOM\n\n"));
    }
  }
    
   else{
    Serial.println(F("No message in there\n"));
  }
} 
  
/*----------------------------------EraseAllFromSerialEEPROM---------------------------------*/
void EraseAllFromSerialEEPROM(){

  Serial.println("\nSure You Want To Erase All?");
  if (yes_or_no()){    
    PORTB &= ~(0b00000100);  // SS Low
    SPIoperation(WREN); //write enable  
    PORTB |= 0b00000100;  // SS High   
    
    PORTB &= ~(0b00000100);  // SS Low
    SPIoperation(WRITE); //transmit write opcode
    
    SPIoperation(address >> 8);   //send MSByte address first
    SPIoperation(address);      //send LSByte address
    
    for (address = 0; address < 128; address++){
      SPIoperation(0); //write data byte
    }
    PORTB |= 0b00000100;  // SS High
    
    //wait for eeprom to finish writing
    delay(10);
    
    Serial.println(F("\nAll Messages Erased from Serial EERPOM\n\n"));
  }  
  else{
    Serial.println(F("\nDid Not Erase Serial EERPOM\n\n"));
  }
}


/*_______________________________________________________________________________________*/
/*---------------------------------------------setup-------------------------------------*/
void setup(){
  int choice;
  boolean done = false;
  
  Serial.begin(9600);
  
  Serial.println(F("END OF LINE CHARACTER MUST BE SET TO Newline\n\n"));
    
  InitializeSPI();
 
  do{ 
    Serial.println(F("What Would You Like To Do? Press 1 to 2, 0 to Exit."));
    Serial.println(F("  1.  Select message to display"));
    Serial.println(F("  2.  Store a message in the Serial EEPROM"));
    Serial.println(F("  3.  Read a message from the Serial EEPROM"));
    Serial.println(F("  4.  Erase One message from the Serial EEPROM"));
    Serial.println(F("  5.  Erase All"));
    

    choice = how_many(0, 5);
    
    if (choice == 1){
      SelectMessageToDisplay();
    }  
      
    if (choice == 2){
      WriteToSerialEEPROM(); 
    }
    
    else if (choice == 3){
      ReadMessageFromSerialEEPROM();  
    }

    else if (choice == 4){
      EraseOneFromSerialEEPROM();  
    }
    
    else if (choice == 5){
      EraseAllFromSerialEEPROM();  
    }
    
    else if (choice == 0){
      Serial.println(F("\n\nWe are done"));
      done = true;
    }
    
  }while(!done);
  

}

/*---------------------------------------------loop-------------------------------------*/

void loop(){
}

