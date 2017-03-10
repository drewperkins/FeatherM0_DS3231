

// AdaFruit Feather M0 sleep with DS3231 RTC and logging test


//wakes, sleeps, and alarms using internal RTC, time maintained from external RTC
//write to SD at interval

////INclude libararies////

#include <SPI.h>
#include "SdFat.h"
#include <RTCZero.h>
#include <Wire.h>
#include "RTClib.h"

//#define ECHO_TO_SERIAL  //allows serial output if uncommented

////Pin definitions////
#define cardSelect 4 //card select pin on adalogger shield = 10, featherlogger = 4
#define RED 13 //onboard LED
#define GREEN 8// green led on featherlogger
#define VBATPIN A7 //battery voltage pin
#define PIRINPUT A0 //glolab DP-002ADIR set for single trigger 200ms dwell
#define cardDetect 7

//#define TRIGGERLED 12 //led on pin12 to indicate count happened
/*
#ifdef ARDUINO_SAMD_ZERO
#define Serial SerialUSB   // re-defines USB serial from M0 chip so it appears as regular serial
#endif
*/

////Settings////
#define SampleIntSec 30 // //rtc sample interval in seconds
char timeZone[ ] = "-07:00"; //could get this from text file


////Global Objects////

RTCZero rtcINT; //internal RTC object for alarm and sleep
RTC_DS3231 rtcEXT; //External RTC object for time kept on DS3231



SdFat sd;
File nameFile;
File logFile; //file object

char counterName[200] = "";
char firstWriteDateTime[18] = "";
char lastWriteDateTime[18] = "";
char currentFilename[255] = "";
char newFilename[255] = "";

float measuredvbat; //battery voltage
int NextAlarmMin; //variable holds next alarm time in minutes
unsigned int pirCount = 0; //number of samples in current cycle, used for USD flush call.


//Interrupt flags and functions
volatile boolean pirTriggered = false;
volatile boolean alarmTriggered = false;

void pirISR(){
pirTriggered = true;
}

void alarmISR(){
alarmTriggered = true;
}


////SETUP////
void setup(){
delay(5000);
#ifdef ECHO_TO_SERIAL
  while (!Serial);//wait for serial port
  Serial.begin(115200);
  Serial.println("Feather M0 Logger");
#endif


//initialize RTCs  PCF8523 doesn't have an alarm library handy. DS3231 should and could be used for all time keeping in next version. Using the internal RTC makes timer function much simpler. A custom PCB could hard wire the interrupt signal to one of the pins or I could actually solder it.
rtcEXT.begin();
if(! rtcEXT.lostPower()){ //if rtcEXT not intialized set with computer clock at compile
  rtcEXT.adjust(DateTime(F(__DATE__), F(__TIME__))); //need to implement a way to set time without recompiling.
}

DateTime now = rtcEXT.now();

#ifdef ECHO_TO_SERIAL
  Serial.print("EXTERNAL RTC:");
  // Print date...
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
#endif


rtcINT.begin();
now = rtcEXT.now();
rtcINT.setTime(now.hour(),now.minute(),now.second()); //set internal RTC to external RTC.
rtcINT.setDate(now.day(),now.month(),(now.year()-2000)); //year needs to be 2 digit I think



//set pin modes
pinMode(RED, OUTPUT);
pinMode(PIRINPUT, INPUT_PULLDOWN);
pinMode(cardDetect, INPUT_PULLUP);
//pinMode(TRIGGERLED,OUTPUT);

digitalWrite(RED,LOW); //turn off led


#ifdef ECHO_TO_SERIAL
  Serial.print("internal RTC time is ");
  // Print date...
  print2digits(rtcINT.getDay());
  Serial.print("/");
  print2digits(rtcINT.getMonth());
  Serial.print("/");
  print2digits(rtcINT.getYear());
  Serial.print(" ");

  // ...and time
  print2digits(rtcINT.getHours());
  Serial.print(":");
  print2digits(rtcINT.getMinutes());
  Serial.print(":");
  print2digits(rtcINT.getSeconds());
  Serial.println();
  Serial.println("beginning logging");
#endif

//set interrupts
interrupts();
attachInterrupt(digitalPinToInterrupt(PIRINPUT),pirISR,HIGH); //external interrupts
NextAlarmMin = 0; // Sets alarm for next sample time on the hour - zero for on the hour
rtcINT.setAlarmMinutes(NextAlarmMin); //rtc time to wake
rtcINT.enableAlarm(rtcINT.MATCH_MMSS); //match minutes/seconds on alarm for hourly
rtcINT.attachInterrupt(alarmISR);

//sleep mode standby
SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
//Sleep mode IDLE 2
//SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
//PM->SLEEP.reg |= PM_SLEEP_IDLE_APB; //turns off CPU, AFB, APB //Advanced Peripheral Bus (APB)   Advanced High-performance Bus (AHB)
}

void loop(){

while(!(alarmTriggered|pirTriggered)){ //don't sleep unless both are set false
__WFI();
}

digitalWrite(RED, HIGH);


if(pirTriggered){
pirCount++;
blink(RED,1);

pirTriggered = false;

digitalWrite(RED, LOW);
#ifdef ECHO_TO_SERIAL
  Serial.println("PIRtriggered");
  Serial.println(pirCount);
#endif


}


//if timer triggered interrupt
if(alarmTriggered){

  SdInitialize();// get logFile set to correct file.
  #ifdef ECHO_TO_SERIAL
    SerialOutput(); //log to serial if uncomment at top
  #endif
  #ifdef ECHO_TO_SERIAL
      Serial.println("alarm triggered");
      Serial.println("logFile.flush() called");
  #endif
  pirCount = 0;
  blink(GREEN, 10); //blink a bunch of times to show alarm went off//update internal RTC
  DateTime now = rtcEXT.now();
  rtcINT.setTime(now.hour(),now.minute(),now.second());
  rtcINT.setDate(now.day(),now.month(),(now.year()-2000));
  alarmTriggered = false;
}



//code starts here after sleep


}

////FUNCTIONS////

// Debbugging output of time/date and battery voltage
void SerialOutput() {


Serial.print(rtcINT.getDay());
Serial.print("/");
Serial.print(rtcINT.getMonth());
Serial.print("/");
Serial.print(rtcINT.getYear()+2000);
Serial.print(" ");
Serial.print(rtcINT.getHours());
Serial.print(":");
if(rtcINT.getMinutes() < 10)
Serial.print('0');      // Trick to add leading zero for formatting
Serial.print(rtcINT.getMinutes());
Serial.print(":");
if(rtcINT.getSeconds() < 10)
Serial.print('0');      // Trick to add leading zero for formatting
Serial.print(rtcINT.getSeconds());
Serial.print(",");
Serial.print(pirCount);
Serial.print(",");
Serial.println(BatteryVoltage ());   // Print battery voltage
}

// Print data and time followed by battery voltage to SD card

void SdInitialize(){
  int f; //variable for sprintf functions

  //is there a card present?
  if(digitalRead(cardDetect) == HIGH){
    //does current file exist on sd card?
    if(sd.begin(cardSelect, SPI_FULL_SPEED)){
      //card error blink GREEN led 30 times
      if(sd.exists(currentFilename)){ //file exists
        //open file
        logFile.open(currentFilename,O_WRITE | O_AT_END);
        SdOutput();
        //update file timestamp
        logFile.timestamp(T_WRITE, rtcINT.getYear()+2000, rtcINT.getMonth(), rtcINT.getDay(), rtcINT.getHours(), rtcINT.getMinutes(), rtcINT.getSeconds());
        //close file
        logFile.close();//should write all data to file
        //rename file
        //get dates/times into strings, needed for leading zeros
        setCurrentDateTimeString(lastWriteDateTime);
        f = sprintf(newFilename,"%s_%s_TO_%s.csv",counterName,firstWriteDateTime,lastWriteDateTime);
        sd.rename(currentFilename,newFilename);
        strcpy(currentFilename,newFilename); //update current filename

      }
      else{   //file does not exist
        //read logger name
        getLoggerName(); //updates counterName with value stored in name.txt file
        //create file name
        strcpy(newFilename,""); //make sure it is empty
        setCurrentDateTimeString(firstWriteDateTime);
        strcpy(lastWriteDateTime, firstWriteDateTime);//they are the same when created
        f = sprintf(newFilename,"%s_%s_TO_%s.csv",counterName,firstWriteDateTime,lastWriteDateTime);
        logFile.open(newFilename, O_CREAT|O_WRITE|O_AT_END);
        //write header
        writeHeader();
        //write output
        SdOutput();
        //close and write to file
        logFile.timestamp(T_CREATE, rtcINT.getYear()+2000, rtcINT.getMonth(), rtcINT.getDay(), rtcINT.getHours(), rtcINT.getMinutes(), rtcINT.getSeconds());
        logFile.close();
        //update current file name
        strcpy(currentFilename,newFilename);
      }
    }
    else{
      blink(RED,30); //sd.begin failure blink led failure
    }
  }
  else{
    blink(RED,30);//no card blink error code and don't try to write data
  }
}
void getLoggerName(){
  // read from the file until there's nothing else in it:
  char setting[100] = "";
  char value[200] = "";
  char character[2] = {'1','\0'};

  if(!nameFile.open("name.txt",O_READ)){
    strcpy(counterName,"counter"); //if not there, default to counter
  }

  if (nameFile.available()) { //searches for [setting=value] format in text file, only data inside [] will be read.

      while (nameFile.available()) {
        character[0] = nameFile.read();
        while((nameFile.available()) && (character[0] != '[')){ //skip until[
           character[0] = nameFile.read();
        }
        character[0] = nameFile.read();//jump ahead
        while((nameFile.available()) && (character[0] != '=')){ //skip at =
          strcat(setting, character);
          character[0] = nameFile.read();
        }
        character[0] = nameFile.read();//jump ahead
        while((nameFile.available()) && (character[0] != ']')){//if not ] start writing
          strcat(value,character);
          character[0] = nameFile.read();
        }
        if(character[0] == ']'){
          if(strcmp(setting,"counterName") == 0){
            strcpy(counterName,value);
          }
          //if(strcmp(setting,"timeZone") == 0){
          //  strcpy(timeZone,value); //set time zone (char[6])
          //}
          strcpy(setting,"");//reset so we can look for something else
          strcpy(value,"");
          }

        }
     }
  nameFile.close();
  }


void setCurrentDateTimeString(char a[]){ //takes 18char long array and fills it with current time
  int n;
  char currentYYYY[5] = "";
  char currentMM[3] = "";
  char currentDD[3] = "";
  char currentHH[3] = "";
  char currentmm[3] = "";
  char currentSS[3] = "";


  n = sprintf(currentYYYY,"%d",(rtcINT.getYear()+2000));
  if(rtcINT.getMonth()<10){
    n = sprintf(currentMM,"%d%d",0,rtcINT.getMonth());
  }
  else{
    n = sprintf(currentMM,"%d",rtcINT.getMonth());
  }
  if(rtcINT.getDay()<10){
    n = sprintf(currentDD,"%d%d",0,rtcINT.getDay());
  }
  else{
    n = sprintf(currentDD,"%d",rtcINT.getDay());
  }
  if(rtcINT.getHours()<10){
    n = sprintf(currentHH,"%d%d",0,rtcINT.getHours());
  }
  else{
    n = sprintf(currentHH,"%d",rtcINT.getHours());
  }
  if(rtcINT.getMinutes()<10){
    n = sprintf(currentmm,"%d%d",0,rtcINT.getMinutes());
  }
  else{
    n = sprintf(currentmm,"%d",rtcINT.getMinutes());
  }
  n = sprintf(currentSS,"%d%d",0,0);//seconds always 00 in filename
  n = sprintf(a,"%s-%s-%sT%s%s%s",currentYYYY,currentMM,currentDD,currentHH,currentmm,currentSS);
}

void SdOutput() {
// Formatting for file out put yyyy-mm-ddThh:mm:ss(+/-)hh:mm, [pir count], [battery voltage]
  logFile.print(counterName);
  logFile.print(",");
  logFile.print(rtcINT.getYear()+2000);
  logFile.print("-");
  if(rtcINT.getMonth() < 10)
    logFile.print('0');
  logFile.print(rtcINT.getMonth());
  logFile.print("-");
  if(rtcINT.getDay() < 10)
    logFile.print('0');
  logFile.print(rtcINT.getDay());
  logFile.print("T");
  logFile.print(rtcINT.getHours());
  logFile.print(":");
  if(rtcINT.getMinutes() < 10)
    logFile.print('0');      // Trick to add leading zero for formatting
  logFile.print(rtcINT.getMinutes());
  logFile.print(":");
  if(rtcINT.getSeconds() < 10)
    logFile.print('0');      // Trick to add leading zero for formatting
  //logFile.print(rtcINT.getSeconds());
  logFile.print('0'); //time rounded down to the hour instead of HH:00:01
  logFile.print(timeZone); // PST
  logFile.print(",");
  logFile.print(pirCount);
  logFile.print(",");
  logFile.println(BatteryVoltage());   // Print battery voltage
}

// Write data header.
void writeHeader() {
logFile.println("counterName,yyyy-mm-ddThh:mm:ss(+/-)hh:mm,pirCount,Battery Voltage");
}


// blink out an error code, Red on pin #13 or Green on pin #8
void blink(uint8_t LED, uint8_t flashes) {
  uint8_t i;
  for (i=0; i<flashes; i++) {
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    if(flashes>1){
      delay(200);
    }
  }
}

// Measure battery voltage using divider on Feather M0 - Only works on Feathers !!
float BatteryVoltage () {
  measuredvbat = analogRead(VBATPIN);   //Measure the battery voltage at pin A7
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  return measuredvbat;
}

void print2digits(int number) {
  if (number < 10) {
  Serial.print("0"); // print a 0 before if the number is < than 10
  }
Serial.print(number);
}
