/* ESP_toATmega_ForDisplay_V1R8.ino
 *  
 * thepiandi@blogspot.com  MJL
 * 
 * ATmega328P and ESP12 Development Module on Large LED display board
 * Implements a pseudo SPI slave
 * 
 * Developed from ESP_toArduino sketch used with Arduino and ESP12 Interface Board
 * 
 * Version 1:
 * Revision 1: 07/17/2016
 *
 * Revision 2: 07/18/2016
 *  Made changes to Barametric Pressure Trend calculation
 * 
 * Revision 3: 07/22/2016
 *  Turns on red LED
 *  In "host", replaced ip address with host name 
 *  
 * Revision 4: 08/03/2016
 *  Trimemd time text from .JSON file to eliminate "EDT" or "EST"
 *  
 * Revision 5: 08/08/2016
 *  Rewrote "sendIt" function to avoid hangup if ATmega does not respond
 *  Turned off red LED
 *  
 *  Revision 6: 08/16/2016
 *    Added "Feels Like" temperature
 *    
 *  Revision 7: 11/12/2016
 *    Changed parseDataFromHost() to account for missing data
 *    
 *  Revision 8: 04/27/2017
 *    Corrected getForecast().  After changing parseDataFromHost() at last 
 *      revision, found that all eight forecasts (periods and forecasts) 
 *      were identical.  This revision fixed that problem.
 *      
 *  Version 2:
 *  Revision 1: 05/04/2017
 *    A new update now occurs in sync with the 10 minute tick of the clock.
 *      Added uptateTime() which connects to google.com to get current time.
 *      We use the units digit of current minutes and current seconds to 
 *      calculate how many seconds to the next 10 minutes of time.  This
 *      is used as a delay between updates.  Thanks to Daniel Eichhorn for
 *      the method
 */

#include <ESP8266WiFi.h>

/* ---------------------Global definations ------------------------- */
const char* ssid     = "Your SSID";
const char* password = "Your Password";

const char* host = "api.wunderground.com";  //weather underground API IP address
const int httpPort = 80;

String myKey = "Get Your Own Key";

//String station = "KNCCHAPE70"; // Chapel Hill, Morehead
//String station = "KNCCHAPE18"; // Briar Chapel
String station = "KNCPITTS5"; // Sortova Farm
//String station = "KNCPITTS1"; // Jordan Lake
//String station = "INCPITTS2"; // Log Barn Road
//String location = "Log Barn Road";
String location = "Sortova Farm";
//String location = "Jordan Lake";

WiFiClient client;  //make an instance of WiFiClient

String dataFromHost;
 
String temperature, weather, relativeHumidity, wind, pressure, measTime, precipitation, feelslike;
String forecast_1, forecast_2, forecast_3, forecast_4, forecast_5, forecast_6, forecast_7, forecast_8;
String period_1, period_2, period_3, period_4, period_5, period_6, period_7, period_8;

int signalSS = 12;
int signalSCK = 14;
int signalMISO = 13;
int led = 16;

volatile boolean interruptFoundOnSS;
volatile boolean interruptFoundOnSCK;

float barametricPressure = 0.00;
String pressureTrend = "ND";

unsigned long startTime;

int secondsToGo;

/* -------------------- the two ISR routines -----------------------*/
void ISO_SS(){
  interruptFoundOnSS = true;
}

void ISO_SCK(){
  interruptFoundOnSCK = true;
}

/* ------------------------- parseDataFromHost ------------------- */
String parseDataFromHost(const char* dataFind, int indexOffset, const char* endOfData, int firstIndex = 0){
  int startIndex, endIndex;
  
  startIndex = dataFromHost.indexOf(dataFind, firstIndex);
  if (startIndex != -1){
    endIndex = dataFromHost.indexOf(endOfData, (startIndex + indexOffset));
    return dataFromHost.substring((startIndex + indexOffset), endIndex);
  }
  else{
    return String("NoData");
  }
}

/* ---------------------------updateTime ------------------------- */
//calculates the number of seconds to delay to get weather on the 
//10 minute mark

//connects to google.com server to get the date/time.  Uses the
//units digit of minutes and the seconds to calculate a delay
//to get to the next 10 minute mark.  That figure updates secondsToGo
//returns true only if connection is made and all works
boolean updateTime() {
  boolean success = false;
  String line;
  int repeatCounter = 0;
  const int httpPort = 80;
  int minutes;
  int seconds;
  
  if (!client.connect("www.google.com", httpPort)) {
    return success;
  }
  
  // This will send the request to the server
  client.print(String("GET / HTTP/1.1\r\n") +
               String("Host: www.google.com\r\n") + 
               String("Connection: close\r\n\r\n"));
               
  while(!client.available() && repeatCounter < 10) {
    delay(1000); 
    repeatCounter++;
  }

  client.setNoDelay(false);
  if((client.available()) > 0) {
    line = client.readStringUntil('\n');
    if (line.indexOf("HTTP/1.1 200 OK") != -1){
      while((client.available()) > 0){
        line = client.readStringUntil('\n');
        if (line.startsWith("Date: ")) {
          minutes = line.substring(27, 28).toInt();
          seconds = line.substring(29, 31).toInt();          
          secondsToGo = 600 - (60 * minutes + seconds);
          success = true;
          break;
        }
      }
    }
  }
  //We have only read a fraction of data available so
  //connection remains open.  Cannot open WeatherUnderground
  //connectiion with Google connection open.  Hence next line
  client.stop();
  return success;
}

/* --------------------- transmitData ----------------------------*/
// Converts the original string to a character array and passes
// the array and array length on to sendIt()

void transmitData(String transmitData){
  int lengthOfString;
  lengthOfString = transmitData.length() + 1;
  
  char dataToSendAsChars[lengthOfString];  // declaration
  transmitData.toCharArray(dataToSendAsChars, lengthOfString);  //make the array
  
  sendIt(dataToSendAsChars, lengthOfString);
}

/* ------------------------- sendIt ----------------------------*/
// Here is where the data is sent to listener. Looks for SS to go
// low, then for eight clock pulses from listener.  MISO is altered
// according to the data when SS goes low and when SCK goes low

// If all the bytes are not sent within 9.5 minutes, then we exit
// this prevents a hangup if the ATmega328P does not respond to the
// interrupt

void sendIt(char* dataToSend, int byteCount){
  int i;
  byte dataOut;
  int bitCount;

  //startTime was intiialized in loop()
  
  for (i = 0; i < byteCount; i++){
    dataOut = byte(dataToSend[i]);            
    delay(0);
    
    // ********  LOOK FOR SS TO GO LOW
    attachInterrupt(signalSS, ISO_SS, FALLING);
    while(1){
      if(interruptFoundOnSS){       //SS is LOW if true
        interruptFoundOnSS = false;
        detachInterrupt(signalSS);
        
        //send the first bit, the MSB of the byte
        if (dataOut > 127){
          digitalWrite(signalMISO, HIGH);
        }
        else{
          digitalWrite(signalMISO, LOW);
        }
        dataOut <<= 1;   //rotate left to get the next bit
        break;
      }
      else if((millis() - startTime) > 570000){
        detachInterrupt(signalSS);
        return;
      }
      delay(0);  //delay looking for SS to go LOW
    }

    // Send the next seven bits
    for (bitCount = 1; bitCount < 8; bitCount++){

      // ********   LOOK FOR SCK TO GO HIGH
      attachInterrupt(signalSCK, ISO_SCK, RISING);
      while(1){
        if(interruptFoundOnSCK){       //SCK is HIGH it true, do nothing here
          interruptFoundOnSCK = false;
          detachInterrupt(signalSCK);
          break;
        }
        else if ((millis() - startTime) > 570000){
          detachInterrupt(signalSCK);
          return;
        }
        delay(0);  //delay looking for SCK to go HIGH
      }
      
      // ********   LOOK FOR SCK TO GO LOW
      attachInterrupt(signalSCK, ISO_SCK, FALLING);
      while(1){
        if(interruptFoundOnSCK){     //SCK is LOW if true, send a bit
          interruptFoundOnSCK = false;
          detachInterrupt(signalSCK);
          
          //Send the next bit
          if (dataOut > 127){
              digitalWrite(signalMISO, HIGH);
          }
          else{
            digitalWrite(signalMISO, LOW);
          }
          dataOut = dataOut<< 1;
          break;          
        }
        else if ((millis() - startTime) > 570000){
          detachInterrupt(signalSCK);
          return;
        }            
        delay(0);  //delay looking for SCK to go LOW
      }          
    }
    
    // ********   LOOK FOR SS TO GO HIGH TO END DATA TRANSFER OF ONE BYTE
    attachInterrupt(signalSS, ISO_SS, RISING);
    while(1){
      if(interruptFoundOnSS){       //SS is HIGH exit to next character
        interruptFoundOnSS = false;
        detachInterrupt(signalSS);
        break;
      }          
      else if ((millis() - startTime) > 570000){
        detachInterrupt(signalSS);
        return;
      }
      delay(0);  //delay looking for SS to go HIGH
    }
  }                    
}
/* --------------------getCurrentConditions --------------------*/
void getCurrentConditions(){
  //  Make TCP/IP connection to Weather Underground API

  if(!client.connect(host, httpPort)){
    measTime = "Could Not Connect";
    weather = "Could Not Connect";
    temperature = "Could Not Connect";
    relativeHumidity = "Could Not Connect";
    wind = "Could Not Connect";
    pressure = "Could Not Connect";
    feelslike = "Could Not Connect";
    precipitation = "Could Not Connect";        
    
    return;  // return if connection fails
  }
  
  // Prepare HTTP request get current conditions from weather station
  String uri = "http://api.wunderground.com/api/" + myKey + "/conditions/q/pws:" + station + ".json";
  String getString = "GET " + uri + " HTTP/1.1\r\nConnection: close\r\n\r\n";

  // Send request to API
  client.print(getString);

  delay(2000);

  // Receive Current Conditions data from the host
  while(client.available()){
    dataFromHost = client.readString();
  }

  if (dataFromHost.indexOf("HTTP/1.1 200 OK") != -1){  
    // Get date and time
    measTime = parseDataFromHost("Last Updated on", 16, " E");

    // Get Current Conditions   
    weather = parseDataFromHost("weather\":", 10,  "\"");
    temperature = parseDataFromHost("temp_f", 8, ",");
    relativeHumidity = parseDataFromHost("relative_humidity\":", 20, "\"");
    wind = parseDataFromHost("wind_string\":", 14, "\"");
    pressure = parseDataFromHost("pressure_in\":", 14, "\"");
    feelslike = parseDataFromHost("feelslike_f\":", 14, "\"");
    precipitation = parseDataFromHost("precip_today_in\":", 18, "\""); 

    float pres = pressure.toFloat();
    if (pres > 27.00 && pres < 33.00){
      if (barametricPressure > 27.00){
        if ((pres - barametricPressure) > 0.015){
          pressureTrend = "Rising";
          barametricPressure = pres;
        }
        else if ((pres - barametricPressure) < -0.015){
          pressureTrend = "Falling";
          barametricPressure = pres;       
        }
      }
      else{
        pressureTrend = "ND";
        barametricPressure = pres;             
      }
    }
  }

  else{
    measTime = "Host is offline";
    weather = "Host is offline";
    temperature = "Host is offline";
    relativeHumidity = "Host is offline";
    wind = "Host is offline";
    pressure = "Host is offline";
    feelslike = "Host is offline";
    precipitation = "Host is offline";  
        
    return;  //return if no data from host
  }    
}
/* --------------------get Forecast--------------------*/
void getForecast(){
  int index;
  delay(2000);
  
  //  Make TCP/IP connection to Weather Underground API
  if(!client.connect(host, httpPort)){
    
    period_1 = "Could Not Connect";
    forecast_1 = "Could Not Connect";
    period_2 = "Could Not Connect";
    forecast_2 = "Could Not Connect";
    period_3 = "Could Not Connect";
    forecast_3 = "Could Not Connect";
    period_4 = "Could Not Connect";
    forecast_4 = "Could Not Connect";
    period_5 = "Could Not Connect";
    forecast_5 = "Could Not Connect";
    period_6 = "Could Not Connect";
    forecast_6 = "Could Not Connect";
    period_7 = "Could Not Connect";
    forecast_7 = "Could Not Connect";
    period_8 = "Could Not Connect";
    forecast_8 = "Could Not Connect";
    
    return;  // return if connection fails
  }

  // Prepare HTTP request fror forecast
  String uri1 = "http://api.wunderground.com/api/" + myKey + "/forecast/q/pws:" + station + ".json"; 
  String getString1 = "GET " + uri1 + " HTTP/1.1\r\nConnection: close\r\n\r\n";

  // Send request to API
  client.print(getString1);

  delay(2000);

  // Receive forecast from host
  while(client.available()){
    dataFromHost = client.readString();
  }

  if (dataFromHost.indexOf("HTTP/1.1 200 OK") != -1){
    //Serial.print(dataFromHost);

    // Get Forecasts--------------------*/   
    index = dataFromHost.indexOf("period\":0");
    period_1 = parseDataFromHost("title\":", 8,  "\"", index);
    forecast_1 = parseDataFromHost("fcttext\":", 10, "\"", index);
    index = dataFromHost.indexOf("period\":1");
    period_2 = parseDataFromHost("title\":", 8,  "\"", index);
    forecast_2 = parseDataFromHost("fcttext\":", 10, "\"", index);
    index = dataFromHost.indexOf("period\":2");
    period_3 = parseDataFromHost("title\":", 8,  "\"", index);
    forecast_3 = parseDataFromHost("fcttext\":", 10, "\"", index);
    index = dataFromHost.indexOf("period\":3");
    period_4 = parseDataFromHost("title\":", 8,  "\"", index);
    forecast_4 = parseDataFromHost("fcttext\":", 10, "\"", index);
    index = dataFromHost.indexOf("period\":4");
    period_5 = parseDataFromHost("title\":", 8,  "\"", index);
    forecast_5 = parseDataFromHost("fcttext\":", 10, "\"", index);
    index = dataFromHost.indexOf("period\":5");
    period_6 = parseDataFromHost("title\":", 8,  "\"", index);
    forecast_6 = parseDataFromHost("fcttext\":", 10, "\"", index);
    index = dataFromHost.indexOf("period\":6");
    period_7 = parseDataFromHost("title\":", 8,  "\"", index);
    forecast_7 = parseDataFromHost("fcttext\":", 10, "\"", index);
    index = dataFromHost.indexOf("period\":7");
    period_8 = parseDataFromHost("title\":", 8,  "\"", index);
    forecast_8 = parseDataFromHost("fcttext\":", 10, "\"", index);

  }
  else{
    period_1 = "Host is offline";
    forecast_1 = "Host is offline";
    period_2 = "Host is offline";
    forecast_2 = "Host is offline";
    period_3 = "Host is offline";
    forecast_3 = "Host is offline";
    period_4 = "Host is offline";
    forecast_4 = "Host is offline";
    period_5 = "Host is offline";
    forecast_5 = "Host is offline";
    period_6 = "Host is offline";
    forecast_6 = "Host is offline";
    period_7 = "Host is offline";
    forecast_7 = "Host is offline";
    period_8 = "Host is offline";
    forecast_8 = "Host is offline";
    
    return;  //return if no data from host
  }  
}
/*_____________________________Setup____________________________*/

void setup() { 
  pinMode(signalSS, INPUT_PULLUP);
  pinMode(signalSCK, INPUT_PULLUP);
  pinMode(signalMISO, OUTPUT);
  pinMode(led, OUTPUT);
  
  interruptFoundOnSS = false;
  interruptFoundOnSCK = false;

  digitalWrite(signalMISO, LOW);
  digitalWrite(led, HIGH);

  delay(10);
  WiFi.mode(WIFI_STA);  //declare station mode

  // Connecting to WiFi network: home router  
  WiFi.begin(ssid, password);
   
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  delay(1000);  
}
/*_________________________________loop______________________________*/
void loop() {
  delay(10000);
 
  startTime = millis();
  getCurrentConditions();
  getForecast();


  // Signal Listener 
  digitalWrite(signalMISO, HIGH);
  delayMicroseconds(100);
  digitalWrite(signalMISO, LOW);
  
  // Transmit Data
  transmitData(location);
  transmitData(measTime);
  transmitData(weather);
  transmitData(temperature);
  transmitData(feelslike);
  transmitData(relativeHumidity);
  transmitData(wind);
  transmitData(pressure);
  transmitData(pressureTrend); 
  transmitData(precipitation);

  transmitData(period_1);
  transmitData(forecast_1);
  transmitData(period_2);
  transmitData(forecast_2);
  transmitData(period_3);
  transmitData(forecast_3);
  transmitData(period_4);
  transmitData(forecast_4);
  transmitData(period_5);
  transmitData(forecast_5);
  transmitData(period_6);
  transmitData(forecast_6);
  transmitData(period_7);
  transmitData(forecast_7);
  transmitData(period_8);
  transmitData(forecast_8);

  //creates delay to get to next 10 minute mark

  if (updateTime()){
    delay(secondsToGo * 1000);
  }

  //if updateTime() is unsuccessful use brute force delay
  else{
    do{
      delay(1000);      
    }while ((millis() - startTime) < 600000);
    
  }
  
}
