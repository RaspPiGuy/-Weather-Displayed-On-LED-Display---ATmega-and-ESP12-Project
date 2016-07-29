# Weather Displayed On LED Display - ATmega and ESP12

Large 18 character LED display showing user messages and the current weather and weather forcasts.
The Display requires circuitry for ATmega328P and ESP-12.  Arduino IDE for programming both devices
## The Display

My large LED display is a salvaged item.  It consists of 18, seven row by five column, red LED matrices.  The area of the display board containing the matrices is 27" long by 2" high.  There are empty sockets where the microcontroller and two memory chips once resided.   There is a forth empty socket for, I'm guessing,  a read only memory device. acting as a character generator.  I was left with the LED matrices and support chips.  

The support chips are interesting: six UCN5801s for latching the data for each column, and two UCN5890 shift registers for selecting columns.  Both devices are ideal for driving high power devices.  Each open collector pin can handle 50volts and sink 500ma.  They can drive DC motors (they have output transient protection), so they can easily handle the LEDs.  Too bad they are no longer manufactured.  

The display, without the critical ICs, provided an opportunity and a challenge.  I was able to reverse engineer the board to determine where to attach new circuitry.  I replaced their microcontroller with an ATmega328P, the same device used on the Arduino Uno.  In fact, I started my development by connecting the Uno to the display board.  The ATmega328P has 32K of flash RAM mainly used for program storage, 2K of SRAM, and 1K of internal EEPROM.  
##Interfacing To The Display
Eventually, I mounted the ATmega328P to a breadboard style PCB.  Since I use the Arduino IDE for all my programming, the ATmega328P requires a bootloader. I have a jig I made for that purpose.  The ATmega sits in a zero insertion force socket.  All the information necessary to build this fixture, and to program the bootloader is on my blog: [ATmega bootloader jig](http://thepiandi.blogspot.com/2014/12/jig-to-load-bootloader-and-upload.html).

I needed a lot of IO so I added a 74HC238 three to eight line decoder.  This reduced the microcontroller IO requirements by four (three IO pins select one of seven rows).  The ATmega328P has 20 useful IO pins.  The display, itself needed 14, so it was a struggle utilizing the IO pins effectively.  

Everything that is displayed is stored in a 25AA512 serial EEPROM.  This allows the display to be continuous.  

My interface board has a barrel connector to receive 5V from a wall wart.  There is also a six pin connector that receives a [Sparkfun FTDI Basic Breakout module](https://www.sparkfun.com/products/9716).  This USB to UART interface is used to program the ATmega328P.  
###*Adding to the Interface to Display the Weather*

After using the display for some time to display messages, I wanted it to do more, to be more dynamic, like displaying the weather or news from the internet.  The most cost effective way to do this is via the ESP8266 family of  WiFi modules.  These inexpensive devices can be used as a WiFi server or client, and even have their own access point.  The device has its own microcontroller that can be programmed using the Arduino IDE.  The original ESP8266 had a limited number of useful IO pins.  Several generations of improvements have been made culminating in the ESP12 family, of which the ESP12f is the latest.  These devices have the same basic circuitry as the ESP8266 but have many more IO pins.  

I found the best solution for me was to purchase the ESP8266 development board. The main component is an ESP12e. The advantages are that I don't have to deal with the 2mm. pitch, castellated pads, of the ESP12e.  It contains a 3.3V regulator that can also power other devices.  It has an USB to UART interface for programming and serial communications .  Finally, It uses DTR to flash your Arduino sketch so you don't have to deal with switching the GPIO0 pin.  

The only major component  left to discuss is the one labeled TXB0104.This is a [voltage level translator breakout module from Sparkfun](https://www.sparkfun.com/products/11771) containing a Texas Instrument TXB0104 surface mount chip.  It's a 4-bit bidirectional voltage-level translator with automatic direction sensing.  A very nice device.  This is required because the ESP12e is not 5V tolerant.  There are two signals between the ESP device and the ATmega328P that run through this part.

###*Schematics and Interface Board Layout*

My schematics for the display board and my interface board are included here as well as a diagram of the layout of the interface board.  Schematic pages 2 to 6 are the display board as I reversed engineered it.  Pages 1 and 7 are the schematics of my interface board.  Page 7 includes the ESP8266 development board for providing WiFi and internet access.  It's the last page because it was added much later.
##Writing To The LEDs - Design Concept
The display is divided into six zones.  Each zone is supported by one of the six UCN5801 data latches.  Each zone contains fifteen of the ninety columns (18 matrices times 5)  of the display.  I write to the display by latching, in turn, the seven bits of six columns.  For example, I latch the data for column 0, then for column 15,  30, 45, 60 and 75.  When all six latches have been written to I simultaneously turn on columns 0. 15, 30, 45, 60, and 75.  Next I latch the data for column 1, followed by column 16, followed by column 31, etc.  Then, I simultaneously turn on columns 1, 16, 61, 46, 61, and 76.  I follow this through till I have written to all 90 columns before starting the process over again.  

If I were to access each column, one at a time, an individual LED, programmed to be on, could only be on 1/90 of the time.  Accessing six columns at a time means an individual LED can be accessed 1/15 of the time.  This means the display can be six times brighter.  The columns are turned on by the UCN5890 shift registers.  These are 8 bit devices so we need two to access one of 15.  Each of the 15 connects to six columns.  
##Character Generation
My character generator (converts characters to 5 x 7 bit patterns for the LEDs) is in the ATmetga328P's 1K EEPROM.  Each character requires 5 bytes, one byte for each column.  1024 bytes is plenty of room for the standard 128 ASCII characters and some special characters.  When I first got the display board, one of the 18 matrices was not soldered on the board.  I connected it to my Arduino, designed each character, figured out each of the five bytes and tested it out on the display.  I previously wrote library functions (contained here) to access the ATmega328P's EEPROM so I could write the five bytes for each character. The address of the first byte for each character can be found by multiplying it's ASCII equivalent by 5.
##Utilizing the Serial EEPROM
The 25AA512 serial EEPROM storage capacity is 64Kbytes organizes as five hundred 128 byte pages.  Communications with this device is over SPI interface (I know it's redundant: Serial Peripheral Interface Interface.  It just sounds awkward otherwise).  The ATmega328P supports SPI very well, both in Master and Slave mode.  The ATmega is the master while the EEPROM is the slave.  

I organize the memory as 31 areas.  Each area uses eight pages or 2048 bytes.  
The first 29 areas are for messages.  Area 30 is for the weather, and area 31 is for future use, probably for RSS news feeds.  Because of the SRAM limitations of the ATmega328P, the messages (not the weather) are limited to 1500 bytes.  The entire Gettysburg Address fits, nicely in one message, so 1500 seems sufficient.  These areas start at the second page.

I devote the first 128 byte page as a directory to the messages stored in the device, recording the length of each message.   Two bytes are necessary for the message length.   

For messages (not weather), I have a sketch that allows the user to type a message, read back a message to the computer serial display, erase a message (make it's length zero), and select which message is it to be displayed.  Selecting a message writes the message number and the message length to the last three bytes of the ATmega328P's internal EEPROM.  Once the user selects a message, he/she chooses one of four sketches (see below).  Each sketcg displays the message differently.  These sketches look to the ATmega's internal EEPROM to calculate the address of the message in the serial EEPROM and to know the message length.
###*Differences for Weather*
The ATmega sketches to display messages rely upon another sketch to write the message to the serial EEPROM and to select the message.  This sketch is not used for weather.   Rather than requiring two sketches, one to handle messages, and another to display the messages, the weather sketch combines both in one sketch   It must write the weather data to the serial EEPROM, as well as to read the weather from the serial EEPROM, and display the weather data.

The length of the message is calculated as the weather information is accumulated from the ESP, and the area is always the same, area 30.  Therefore this sketch does not need to record the message number and message length in page 0 of the serial EEPROM or in the last three bytes of the ATmega328P's internal EEPROM.

##ESP Getting the Weather and Sending it to the ATmega
###*About weatherunderground.com*
Weather Underground provides comprehensive weather information from large weather stations like at airports, and from thousands of personal weather stations run by anyone who wants to invest in the equipment.

Besides a wonderful web site, they provide an API or Application Programming Interface.  A user can use this to download the weather and upload it to his own site.  I download the weather information and upload it to my display.  

To avail yourself of this service, you must setup an account with Weather Underground.  They provide you with a key that is included in all HTTP requests you make to their URL.  The service is free if you limit how many times you access it.  Free service means you do not access the API more than 500 call per day or more than 10 calls per minute.  A call is an HTTP request made to their site.  

I access the site every 10 minutes, making two calls each time, one for the current weather conditions, and the other for the forecast.  I'm thinking of adding a third call, for weather alerts.  Running all day, I make about half of the maximum number of calls.

###*Process of Getting the Data From the URL*

Take a look at my sketch **"ESP_toATmega_ForDisplay_V1R2.ino"**.  There is a lot of good information here.  Data comes to me as two .JSON files (you can also choose XML) - one for current conditions, one for forecast..  I just have to parse the data to get what I want.  Thank goodness for the Arduino String object and its methods.  I use them extensively.  

I take the data in the order as it comes in the file.  I search for a string, erase everything before that string, and make a substring starting a certain number of characters following the string I searched for, and ending with a particular character.  I keep doing this for each piece of data I want.  
###*Sending Data To the Atmega328P*
I also use SPI to communicate between the ESP and the ATmega.  However, I had to build my own SPI, a pseudo SPI if you will.  The ESP supports most of the SPI functions contained within the Arduino SPI library.  However, that library only supports SPI master mode and I need the ESP to be the slave.  

On the ATmega328P side, I could have used the same SPI functionality as I used to communicate with the serial EEPROM.  I did not use the Arduino SPI library for this, but used the SPI functionality, directly, as provided by the ATmega itself.  Unfortunately  I could not get that interface to run slow enough for the ESP.  The slowest clock speed I could program was 250kHz.  To get the ESP to reliability pass data, I had to reduce the clock to 3.3kHz.  Therefore I had to design both the master and slave interface.

The signals between the devices are:
* SS_ESP	     SPI Slave for the ESP
* MISO_ESP    Master In Slave Out for the ESP -  data into the ATmega
* SCK         SPI clock - Is the same clock signal for the serial EEPROM

The whole handshaking process is interrupt driven on both sides.  When the ESP is ready to send data to the ATmega, it sets MISO_ESP high for a 100usec.  This triggers an interrupt on the ATmega.  The ATmega does not look to see if the interrupt occurred until it has displayed all of the weather information.  At that time, if there was no interrupt, it simply displays the same weather information again.  

If there was an interrupt, it brings SS_ESP low.  The ESP sees this as an interrupt and sets MISO_ESP to the logic level of the MSB (most significant bit - the one on the left) of the first byte of data.  The ATmega, after half a clock cycle of SCK brings SS_ESP and SCK high and reads the logic level of MISO_ESP.  When the ESP sees SCK high, then low (half a clock cycle), it shifts the data byte one position to the left and sets the logic level of MISO_ESP according to the MSB of the data byte.  The clock SCK goes high and low seven more times to receive the rest of the bits of the data byte.  This process is repeated until all bytes are sent.  The ATmega knows when to stop looking for more bytes when it sees a byte of 0.

##Program Sketches

Software sketches for both the ATmega328P and the ESP device are written using the Arduino IDE.  It is now possible to add programming functionality for the ESP8266 to the Arduino IDE.  [Here is the most comprehensive documentation for doing this](https://github.com/esp8266/Arduino).   All of the sketches are included here:
###*ATmega328P sketches:*
**Sketch: ComposeMessageSendToSerialEEPROM_V1R4.ino:** 
* Not used for the weather.  
* Menu  driven program for:
  * Selecting a message in the serial EEPROM to display
  * Writing a message to the serial EEPROM
  * Reading messages from the serial EEPROM
  * Erasing a message from the serial EEPROM
  * Erasing all messages from the serial EEPROM
	
**Sketch: TimesSquareScroll_V4R1.ino:**
* Not used for the weather.  
* Used for scrolling a message from left to right.

**Sketch: TimesSquareScroll_Reverse_V2R1.ino:**
* Not used for the weather.  
* Scrolls a message from right to left.  
* The challenge here is that the letters in each word have to be reversed.
* Best not to use this with a message with punctuation, it looks weird.

**Sketch: VerticalDisplayScrollsDown_V3R1.ino:**
* Not used for the weather.  
* Scrolls a message from top to bottom one line at a time.
* See the included video for an example
* The challenge here was not to break up words

**Sketch: VerticalDisplayScrollsUp_V2R1.ino:**
* Not used for the weather.  
* Scrolls a message from bottom to top one line at a time.
* The challenge here was not to break up words

**Sketch: TimesSquareScroll_WeatherFromESP_CompleteForecast_V1R1.ino:**
* Only used for displaying the weather
* Displays the current weather plus forecast for today, tonight, tomorrow, tomorrow night, and four more including the next two days, day and night.
* Display is based on the Times Square Scroll, left to right.
* Requires ESP to be powered and its sketch running

**Sketch: TimesSquareScroll_WeatherFromESP_PartialForecast_V1R1.ino:**
* Only used for displaying the weather
* Displays the current weather plus forecast for today.
* Display is based on the Times Square Scroll, left to right.
* Requires ESP to be powered and its sketch running

###*ESP Sketch:*
**Sketch ESP_toATmega_ForDisplay_V1R2.ino:**  
* Connects to home WiFi, connects to Weather Underground API every 10 minutes to retrieves current conditions and forecasts from the API, sends the data to the ATmega328P using handshaking based on a pseudo SPI.

##Some Useful Notes
###*Use of String Object in Arduino Sketches*
I like the ATmega328P, but it does have a few limitations, the worst of which is its limited space for variables - only 2Kbytes.  If you have allocated lots of space for variables like when writing something such as: **"char characterData [1500];"** you will get a compiler warning: "Low memory available, stability problems may occur".  If you make it even larger you may get a compiler error.  But you know beforehand that you have an issue.

You don't declare the size of String objects so the compiler does not know how big it may become.  If your String builds to say 2000 bytes, there will be no compiler warning, your sketch just runs out of memory and freezes.  

The way to avoid problems is by being careful.  You can use the String method **"reserve"** to tell the compiler how large you expect these objects to be.  In my sketch, I can gather over a kilobyte of data from the ESP.  I could have put this data in one String object and written it all to the serial EEPROM in one write process.  Instead, I gather each section of data in a String object and write each section to the serial EEPROM, one at a time.  Even then the String object might get over a hundred bytes. 

My sketch, **"ComposeMessageSendToSerialEEPROM_V1R4.ino"** accumulates one entire message in the character array "charterData[1500]".  I have allocated 1500 of the 2048 bytes of SRAM to just this one variable.  Since this sketch also interfaces to the user it has plenty of print statements.  The text for these statements also go into the variable memory.  That is something you can change by altering your print statements like this: **"Serial.println(F("Text for your statement"));"**  This puts **"Text for your statement"** into program memory, not variable memory.

Luckily, the ESP microcontroller has plenty of SRAM and even DRAM.  I'm not sure exactly how much can be used for variables, the manufacturer does not specify, but it is much more than 2K, probably 32K.  So, I use String objects liberally in my ESP sketch.  The .JSON file for the forecasts is very large, about 7k , this would choke the ATmega328P, but is no problem for the ESP
###*ESP Resets Itself*
While the ESP has a very capable microcontriller, its main responsibility is to service WiFi, not your code.  There has to be time for WiFi to handle pending events.  It usually does this when it encounters delays in your code from the delay() function (not with delayMicroseconds()).  

If the device does not find the time to attend to these services it will reset itself.  If you are running code with the serial monitor open (or using some other terminal program) you will see a message telling you it has reset and your code stops running.  If your ESP is not connected to a computer, you will not see this message.  That is why you will see several "delay(0)" statements in my ESP code. 



