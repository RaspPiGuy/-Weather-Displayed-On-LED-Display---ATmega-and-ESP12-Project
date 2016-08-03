/* ESP_toATmega_ForDisplay_V1R3.ino
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
 *  In "host" replaced ip address with host name 
 */

#include <ESP8266WiFi.h>

/* ---------------------Global definations ------------------------- */
const char* ssid     = "Your Router";
const char* password = "Your Password";

const char* host = "api.wunderground.com";
const int httpPort = 80;

String myKey = "Get Your Own Key";
//String station = "KNCCHAPE70"; // Chapel Hill, Morehead
//String station = "KNCCHAPE18"; // Briar Chapel
String station = "KNCPITTS5"; // Sortova Farm
//String station = "INCPITTS2"; // Log Barn Road
//String location = "Log Barn Road";
String location = "Sortova Farm";

WiFiClient client;  //make an instance of WiFiClient

String dataFromHost;
 
String temperature, weather, relativeHumidity, wind, pressure, measTime, precipitation;
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

/* -------------------- the two ISR routines -----------------------*/
void ISO_SS(){
  interruptFoundOnSS = true;
}

void ISO_SCK(){
  interruptFoundOnSCK = true;
}

/* ------------------------- parseDataFromHost ------------------- */
String parseDataFromHost(const char* dataFind, int indexOffset, const char* endOfData){
  int index;
  
  index = dataFromHost.indexOf(dataFind);
  dataFromHost.remove(0, index + indexOffset);
  index = dataFromHost.indexOf(endOfData);

  return dataFromHost.substring(0, index);  
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

void sendIt(char* dataToSend, int byteCount){
  int i;
  byte dataOut;
  int bitCount;
  boolean done;
  unsigned long startTime;
  
  for (i = 0; i < byteCount; i++){
//    startTime = millis();
    done = false;
    dataOut = byte(dataToSend[i]);            
    attachInterrupt(signalSS, ISO_SS, FALLING);
    do{
      delay(0);
      // ********  LOOK FOR SS TO GO LOW
      if(interruptFoundOnSS){       //SS is LOW if true
        interruptFoundOnSS = false;
        detachInterrupt(signalSS);
        attachInterrupt(signalSS, ISO_SS, RISING);
        
        //send the first bit
        if (dataOut > 127){
          digitalWrite(signalMISO, HIGH);
        }
        else{
          digitalWrite(signalMISO, LOW);
       }
        dataOut <<= 1;
        
        // ********   LOOK FOR SS TO GO HIGH AND SCK HIGH
        bitCount = 0;
        attachInterrupt(signalSCK, ISO_SCK, RISING);
        while(1){
          if(interruptFoundOnSS){       //SS is HIGH exit to next character
            interruptFoundOnSS = false;
            detachInterrupt(signalSS);
            detachInterrupt(signalSCK);
            done = true;
            break;
          }          
          if(interruptFoundOnSCK){       //SCK is HIGH do nothing here
            interruptFoundOnSCK = false;
            detachInterrupt(signalSCK);
            attachInterrupt(signalSCK, ISO_SCK, FALLING);
            
            // ********   LOOK FOR SS TO GO HIGH AND SCK LOW
            while(1){
              if(interruptFoundOnSCK){     //SCK is LOW
                interruptFoundOnSCK = false;
                detachInterrupt(signalSCK);
                attachInterrupt(signalSCK, ISO_SCK, RISING);

                //Send a bit
                if (bitCount++ < 7){
                  if (dataOut > 127){
                    digitalWrite(signalMISO, HIGH);
                 }
                  else{
                    digitalWrite(signalMISO, LOW);
                  }
                  dataOut = dataOut<< 1;
                }
                //attachInterrupt(signalSCK, ISO_SCK, RISING);
                break;          
              }
              delay(0);  //delay looking for SCK to go LOW
            }
          }
          delay(0);  //delay looking for SCK to go HIGH and SS to go HIGH            
        }
      }
      delay(0);  //delay looking for SS to go LOW
    }while (!done);
    digitalWrite(signalMISO, LOW);
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
    precipitation = "Could Not Connect";        
    
    return;  // return if connection fails
  }
  
  // Prepare HTTP request get current conditions from Brial Chapel or Morehead
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
    measTime = parseDataFromHost("Last Updated on", 16, "\"");

    // Get Current Conditions   
    weather = parseDataFromHost("weather\":", 10,  "\"");
    temperature = parseDataFromHost("temp_f", 8, ",");
    relativeHumidity = parseDataFromHost("relative_humidity\":", 20, "\"");
    wind = parseDataFromHost("wind_string\":", 14, "\"");
    pressure = parseDataFromHost("pressure_in\":", 14, "\"");
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
    precipitation = "Host is offline";  
        
    return;  //return if no data from host
  }    
}
/* --------------------get Forecast--------------------*/
void getForecast(){
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
    period_1 = parseDataFromHost("title\":", 8,  "\"");
    forecast_1 = parseDataFromHost("fcttext\":", 10, "\"");
    period_2 = parseDataFromHost("title\":", 8,  "\"");
    forecast_2 = parseDataFromHost("fcttext\":", 10, "\"");
    period_3 = parseDataFromHost("title\":", 8,  "\"");
    forecast_3 = parseDataFromHost("fcttext\":", 10, "\"");
    period_4 = parseDataFromHost("title\":", 8,  "\"");
    forecast_4 = parseDataFromHost("fcttext\":", 10, "\"");
    period_5 = parseDataFromHost("title\":", 8,  "\"");
    forecast_5 = parseDataFromHost("fcttext\":", 10, "\"");
    period_6 = parseDataFromHost("title\":", 8,  "\"");
    forecast_6 = parseDataFromHost("fcttext\":", 10, "\"");
    period_7 = parseDataFromHost("title\":", 8,  "\"");
    forecast_7 = parseDataFromHost("fcttext\":", 10, "\"");
    period_8 = parseDataFromHost("title\":", 8,  "\"");
    forecast_8 = parseDataFromHost("fcttext\":", 10, "\"");

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
  digitalWrite(led, LOW);

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
  unsigned long startTime;
  
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

  do{
    delay(1000);      
  }while ((millis() - startTime) < 600000);
  
}
