/*  TimesSquareScroll_WeatherFromESP_V1R1.ino
 *   
 * thepiandi@blogspot.com  MJL 
 * 
 * Specific to Mike Oshinski's Big Display: one row of 18 5x7 character LED displays
 * ATmega328P and ESP12 Development Module mounted on Large LED Display Board
 * 
 * This iteration displays message starting at the right column of the display.
 * Characters are added to the right and the displays scrolls left one column
 * at a time.  When all of the characters are displayed, display starts over.
 * 
 * Data that is displayed is the current weather and weather forcase from Weather
 * Underground.
 * 
 * Weather conditions and forcast are captured by ESP12 Development Module every 10 minutes.
 * Via a pseudo SPI, where ATmega is the master and ESP12 is the slave, data is sent
 * from the ESP12 to the ATmega.  ATmega stores the weather data in message slot 29 in
 * the Serial EEPROM.  This is done by real SPI.  ATmega as master and Serial EEPROM as slave.
 * 
 * This version saves weather to the Serial EEPROM
 * 
 * Implements a pseudo SPI master to connect to the ESP12
 * 
 * 
 * This sketch evolved from:
 *    TimesSquareScroll_WeatherFromESP_PartialForecast_V1R2.ino
 *      and
 *    TimesSquareScroll_WeatherFromESP_CompleteForecast_V1R2.ino
 *  and combines the two into one sketch.
 *  
 *  If variable "allForecasts" is true, sketch displays Current conditions and 
 *  forcasts for today, tonight, and six more (three days and three nights).
 *  If variable "allForecasts" is false, sketch displays current conditions and
 *  forecast for today only.  Comment out one of the two choices,
 *  
 * Also added Heat Index as "Feels Like" temperature  
 * 
 * Versision 1: 
 *  Revision 1:  08/16/16
 * 
 * 
 */
 
#include <avr/io.h>
#include "EEPROM_Functions.h"

EEPROM_FUNCTIONS eeprom;
/*---------------------------------------------global variables-------------------------------------*/
//do we display eight forcasts or one forecast?  Comment out one of the two lines below.
boolean allForecasts = 0;   //display only today's forecast
//boolean allForecasts = 1;   //display all eight forecastss 

//opcodes
#define WREN  6  //Write Enable
#define WRDI  4  //Write Disable
#define RDSR  5  //Read Status Register
#define WRSR  1  //Write Status Register
#define READ  3  //Read Data
#define WRITE 2  //Write Data

int clockDelay = 150;
byte datamask = 0b00100000;
int EEPROM_index;

int physicalColumn;
int noOfChars;
byte rowData[90];
int displayCount = 10;  //determines the time each line is displayed
int gap =14; //determines the gap between end of message and start of next

volatile boolean foundInterrupt;

/*-----------------------------delayFunction----------------------------*/
//determines the time the LEDs of one column are displayed
void delayFunction(){
  delayMicroseconds(250);
  //delay(1);
}
/*-----------------------------Interrupt Service Routine----------------*/
ISR (PCINT1_vect){
  foundInterrupt = true;  
}

/* __________________________ ESP12 Routines ___________________________*/
/* ----------------------------Read ESP---------------------------------*/
// gets data one bit at a time from ESP - implements a pseudo SPI master
// returns one byte of data
byte readESP(){
  int i;
  byte val;

  val = 0;
  delayMicroseconds(clockDelay);
  PORTB &= ~0b00000010;  //SS_ESP Low
  delayMicroseconds(clockDelay);
  for (i = 0; i < 8; i++){
    PORTB |= 0b00100000;  // SCK High
    val <<= 1;
    if (PINC & datamask){
      val+= 1;
    }
    delayMicroseconds(clockDelay);
    PORTB &= ~0b00100000;  // SCK Low      
    delayMicroseconds(clockDelay);
  }
  PORTB |= 0b00000010;  // SS_ESP High
  
  return val;
}

/* ------------------------------Get Data-------------------------------*/
// Calls readESP to get each byte in a message.  Looks for a terminating 
// byte = 0 to indicate the end of a data stream.  Then exits with the 
// whole message
String getData(){
  String retString = "";
  byte byteFromESP;

  while(1){
    byteFromESP = readESP();
    retString += String(char(byteFromESP));
    if (byteFromESP == 0){
      return retString;
    }
  } 
}

/*------------------------------GetWeatherFromESP----------------------------*/
void getWeatherFromESP(){
  int i;
  byte val;
  String weatherData;
  int messageLength;
  String forcastDay, forcastValue;

  //disable interrupt from ESP prevents interrupt allows normal use of MISO_ESP
  PCICR = 0;
  PCMSK1 = 0;
  delayMicroseconds(1000);

  noOfChars = 0;
  EEPROM_index = 0;

  // let's get some weather data from the ESP

  // Get and store Location and Time and Date
//  weatherData = "weatherunderground.com -- Current Weather at " + getData() + ", ";  //gets location
  weatherData = "Current Weather at " + getData() + ", ";  //gets location
  weatherData += getData() + ": ";                           //adds time and data
  messageLength = weatherData.length();
  noOfChars += messageLength;
  writeToSerialEEPROM(weatherData, messageLength); 

  
  // Get and store current weather
  weatherData = getData() + " *** ";
  messageLength = weatherData.length();
  noOfChars += messageLength;
  writeToSerialEEPROM(weatherData, messageLength); 
    
  // Get and store temperature
  weatherData = "Temperature: " + getData() + "F *** ";
  messageLength = weatherData.length();
  noOfChars += messageLength;
  writeToSerialEEPROM(weatherData, messageLength);   

  // Get and store feels like temperature
  weatherData = "Feels Like: " + getData() + "F *** ";
  messageLength = weatherData.length();
  noOfChars += messageLength;
  writeToSerialEEPROM(weatherData, messageLength);   
    
  // Get and store relative humidify
  weatherData = "RH: " + getData() + " *** ";
  messageLength = weatherData.length();
  noOfChars += messageLength;
  writeToSerialEEPROM(weatherData, messageLength); 
  
  // Get and store wind conditions
  weatherData = "Winds: " + getData() + " *** ";
  messageLength = weatherData.length();
  noOfChars += messageLength;
  writeToSerialEEPROM(weatherData, messageLength); 

  // Get and store baronetric pressure and pressure trend
  weatherData = "Barometer: " + getData() + "\"Hg., ";
  weatherData += getData() + " *** ";   //pressure trend
  messageLength = weatherData.length();
  noOfChars += messageLength;
  writeToSerialEEPROM(weatherData, messageLength); 

  // Get and store precipitation
  weatherData = getData();         //precipitation
  if (weatherData == ""){
     weatherData = "0";
  }
  weatherData = "Rainfall: " + weatherData + "\" *** ";
  messageLength = weatherData.length();
  noOfChars += messageLength;
  writeToSerialEEPROM(weatherData, messageLength); 

  // Forecasts:
  
  // Get and store Period 1 Forecast
  forcastDay = getData(); //not used
  delay(100);
  forcastValue = getData();
  weatherData = " Forecast for today: " + forcastValue + " *** ";
  messageLength = weatherData.length();
  noOfChars += messageLength;
  writeToSerialEEPROM(weatherData, messageLength);   
  
  if (allForecasts){    //if true, store the additional seven forecasts
    // Get and store Period 2 Forecast
    forcastDay = getData(); //not used
    delay(100);
    forcastValue = getData();
    weatherData = " Forecast for tonight: " + forcastValue + " *** ";
    messageLength = weatherData.length();
    noOfChars += messageLength;
    writeToSerialEEPROM(weatherData, messageLength); 
   
    // Get and store Period 3 through Period 8 Forecasts
    for (i = 0; i < 6; i++){
      forcastDay = getData();
      delay(100);
      forcastValue = getData();
      weatherData = " Forecast for " + forcastDay + ": " + forcastValue + " *** ";
      messageLength = weatherData.length();
      noOfChars += messageLength;
      writeToSerialEEPROM(weatherData, messageLength); 
    }
  }

  else{           //if allForecasts false, display only today's forcast
    // To remain in sych with ESP12, get the other seven forcasts
    // but don't use 
    for (i = 0; i < 14; i++){
      getData();
      delay(100);
    }    
  }

  // Enable Interrupt from MISO_ESP
  PCICR = 0b00000010;   //to enable interrupt on port C
  PCMSK1 = 0b00100000;  //to enable interrupt on PC5, MISO_ESP 

  foundInterrupt = false;
}
/* ______________________________Serial EEPROM Routones ____________________________*/
/*--------------------------------------SPIoperation--------------------------------*/
byte SPIoperation(byte data)      //used for reading data and writing data and opcodes
{  
  SPDR = data;                    // Start the transmission 
  while (!(SPSR & (1<<SPIF)));    // Wait the end of the transmission
  return SPDR;       // return the received byte
}

/*-----------------------------WriteToSerialEEPROM-----------------------------------*/
void writeToSerialEEPROM(String dataToWrite, int noOfChars){
  unsigned int address;
  int messageNumber = 29;
  int i;
  
  char dataToEEPROM[noOfChars];     //declaraqtion
  dataToWrite.toCharArray(dataToEEPROM, noOfChars);
  
  address = 128 * ((16 * messageNumber) + 1) + EEPROM_index;
  
  SPCR = 0b01010000; //enable Serial EEPROM SPI
  
  PORTB &= ~(0b00000100);  // SS_EEPROM Low
  SPIoperation(WREN);      // write enable 
  PORTB |= 0b00000100;     // SS_EEPROM High   

  PORTB &= ~(0b00000100);  // SS_EEPROM Low
  SPIoperation(WRITE);     // transmit write opcode
  
  SPIoperation(address >> 8);   //send MSByte address first
  SPIoperation(address);        //send LSByte address

  for (i = 0; i < noOfChars; i++){   
    SPIoperation(dataToWrite[i]); //write data byte
    address++;
    if ((address % 128) == 0){  // Page Boundry
      PORTB |= 0b00000100;    // SS_EEPROM High
      delay(10);              // wait for eeprom to finish writing
      
      PORTB &= ~(0b00000100);  // SS_EEPROM Low
      SPIoperation(WREN);      // write enable  
      PORTB |= 0b00000100;     // SS_EEPROM High   
      
      PORTB &= ~(0b00000100);  // SS_EEPROM Low
      SPIoperation(WRITE);     //transmit write opcode
      
      SPIoperation(address >> 8);   //send MSByte address first
      SPIoperation(address);        //send LSByte address
    }   
  }
  PORTB |= 0b00000100;  // SS_EEPROM High
  delay(10);  //wait for eeprom to finish writing

  SPCR = 0b00010000;  //disable Serial EEPROM SPI  

  EEPROM_index += noOfChars;  //gets this ready for next write
    
}
  
/*-----------------------------------------ReadSerialEEPROM-----------------------------*/
byte ReadSerialEEPROM(int EEPROM_address)
{
  int data;
  
  SPCR = 0b01010000; //enable Serial EEPROM SPI

  PORTB &= ~(0b00000100);  // SS Low
  SPIoperation(READ); //transmit read opcode
  
  SPIoperation(EEPROM_address>>8);   //send MSByte address first
  SPIoperation(EEPROM_address);      //send LSByte address
  
  data = SPIoperation(0xFF); //get data byte. Must write something to read something
  PORTB |= 0b00000100;  // SS High
  
  SPCR = 0b00010000;  //disable Serial EEPROM SPI  

  return data;
}

/* __________________________Display Routones ____________________________*/
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

/*------------------------------------ClearDisplay--------------------------------*/
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

/*--------------------------------Display-----------------------------------*/
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

/*---------------------------------DisplayWeather----------------------------*/
void displayWeather(){
  int i;
  int j;
  unsigned int charAddress;
  int charIndex;
  byte charToDisplay;
  int messageNumber = 29;   //Fixed For Weather

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


/*---------------------------------Initialize All----------------------------*/
void InitializeAll(){
  byte bitBucket;

  /* Setting the ATmega GPIO Ports
   ATmega Port B;
    PB7: CRYSTAL                 INPUT
    PB6: CRYSTAL                 INPUT
    PB5: ESP & EEPROM - SCK      OUTPUT
    PB4: EEPROM - MISO_EEPROM    INPUT
    PB3: EEPROM - MOSI_EEPROM    OUTPUT
    PB2: EEPROM - SS_EEPROM      OUTPUT
    PB1: ESP - SS_ESP            OUTPUT
    PB0: Display - OutputEnable  OUTPUT 
    
   ATmega Port C;
    PC7: No Connection           INPUT
    PC6: RESET                   INPUT
    PC5: ESP - MISO_ESP          INPUT
    PC4: Display - C4            OUTPUT
    PC3: Display - C3            OUTPUT
    PC2: Display - C2            OUTPUT
    PC1: Display - ColumnClock   OUTPUT
    PC0: Display - SerialIn      OUTPUT 
    
   ATmega Port D (Display Row Control;
    PD7: Display - D7            OUTPUT
    PD6: Display - D6            OUTPUT
    PD5: Display - D5            OUTPUT
    PD4: Display - D4            OUTPUT
    PD3: Display - D3            OUTPUT
    PD2: Display - D2            OUTPUT
    PD1: Display - D1            OUTPUT
    PD0: Display - D0            OUTPUT
   */
   
  DDRB = 0b00101111;   
  DDRC = 0b00011111;
  DDRD = 0b11111111;

  //Initalize Ports:
  PORTB = 0;
  PORTB |= 0b00000110; //SS_EEPROM & SS_ESP HIGH 
  PORTC = 0;
  PORTD = 0;

  
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
  
  // Clear out the Serial EEPROM SPI shift register and status register
  bitBucket=SPSR;
  bitBucket=SPDR;

  // Initalize Diaplay 
  ClearColumn();              //Clear out any column data  
  ClearDisplay();             //Send 0 to all columns in turn 
  InitializeRowData();  
  DisableRowOutputs(false);   //enable display  
}

/*_________________________________Setup______________________________*/
void setup() {
  String initialMessage = "Waiting For ESP";

  foundInterrupt = false; 
  
  InitializeAll();  

  // Write Inialitizing Message To Display
  noOfChars = initialMessage.length();
  writeToSerialEEPROM(initialMessage, noOfChars);   
  
  // Enable Interrupt from MISO_ESP
  PCICR = 0b00000010;   //to enable interrupt on port C
  PCMSK1 = 0b00100000;  //to enable interrupt on PC5, MISO_ESP

}

/*_________________________________loop______________________________*/
void loop() {  
  byte MISOlevel;
  
  if (foundInterrupt){
    delayMicroseconds(150);
    MISOlevel = PINC & datamask;
    if (MISOlevel == 0){  //exits if MISO_ESP > 200 uSec. 
      DisableRowOutputs(true);    //disable display
      getWeatherFromESP();  
      DisableRowOutputs(false);   //enable display  
    }
    else{
      do{
        delay(100);
        MISOlevel = PINC & datamask;       
      }while (MISOlevel);  //looking for MISO_ESP to go back low
      foundInterrupt = false;
    }
    delay(1);
  }
  else{
    displayWeather();      
  }
}
