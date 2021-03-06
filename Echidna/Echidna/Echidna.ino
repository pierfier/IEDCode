#include <CurieIMU.h>
#include <PulseSensorBPM.h>
#include <SoftwareSerial.h>
#define NOTE 330 //pitch that speaker works at. Can be changed, 330 = E4

//Notes for start sounds
#define NOTE_E6 1319
#define NOTE_G6 1568
#define NOTE_E7 2637
#define NOTE_C7 2093
#define NOTE_D7 2349
#define NOTE_G7 3136

//set pin numbers:
const int power = 7;   //pin number for power switch
const int cancel = 8; //pin number for alarm cancel button
const int criticalLED = 2; //pin number for LED for critical warning
const int buzzer = 4; //pin number for buzzer
const int pulseSensor = A0; //pin number for pulse sensor
const int tempSensor = A1; //pin number for temperature sensor

const int wantPrint = HIGH; //will it be connected to the computer at this demo?
const int highTemp = 103; //high critical threshold for temperature
const int lowTemp = 93; //low critical threshold for temperature
const int highRate = 220; //high critical threshold for pulse
const int lowRate = 40; //low critical threshold for pulse

//global variables
unsigned int criticalCount; //counts number of times user has been in critical range
const int WAIT_TIME = 15; //seconds to push cancel button after sensors read as critical
int switchState; //used to read the on/off switch
int buttonState; //used to read the cancel button
const int x = 5; /*MIGHT NEED TO CHANGE THIS VALUE*/ //i don't know what this is.
//int cc; //do not use this anymore. Need an unsigned int.
unsigned int cc;
int prevTime;
unsigned long loopTime = 0;          // get the time since program started
unsigned long interruptsTime = 0;    // get the time when free fall event is detected
int prevSS; //previous switch state
int lastPrint;

//pulse sensor variables, used for internal workings of pulse sensor ONLY
 const unsigned long MICROS_PER_READ = 2 * 1000L;
 const boolean REPORT_JITTER_AND_HANG = HIGH;
 const long OFFSET_MICROS = 1L;
 unsigned long wantMicros;
 long minJitterMicros;
 long maxJitterMicros;
 unsigned long lastReportMicros;
 byte samplesUntilReport;
 const byte SAMPLES_PER_SERIAL_SAMPLE = 20;
 int BPM; //This will have the value of the Beats Per Minute of the person's heart rate.
 PulseSensorBPM pulseDetector(pulseSensor, MICROS_PER_READ / 1000L);

 //GPS variables
 SoftwareSerial gps(0,1); //gps is an item
 String location; //location of user
 String data; //information received from GPS locator
 char c; 

void setup() {
  delay(1000); //wait a moment so avoid triggering free fall mode
  //initialize inputs and outputs
  pinMode(power, INPUT);
  pinMode(cancel, INPUT);
  pinMode(criticalLED, OUTPUT);
  //pinMode(buzzer, OUTPUT); //don't need this anymore for speaker

  //initialize variables and states
  criticalCount = 0;
  digitalWrite(criticalLED, LOW);
  //digitalWrite(buzzer, LOW); //speaker
  noTone(buzzer);
  //cc = (30/(x * pow(10, -6))) * 0.9; //90% of the reads taken in 30s
  //cc = 5400000; //this is the actual value for 30 seconds
  cc = 100000; //testing purposes: Lowering time it takes to trigger value
  prevTime = micros();
  prevSS = digitalRead(power);
  lastPrint = millis();

  Serial.begin(9600); // initialize Serial communication
  if (wantPrint)
  {
    while (!Serial) ;   // wait for serial port to connect.
  }

  //Initialise the IMU 
  CurieIMU.begin();
  CurieIMU.attachInterrupt(eventCallback);

  //Enable Free Fall Detection 
  CurieIMU.setDetectionThreshold(CURIE_IMU_FREEFALL, 1000); // 1g=1000mg
  CurieIMU.setDetectionDuration(CURIE_IMU_FREEFALL, 50);  // 50ms
  CurieIMU.interrupts(CURIE_IMU_FREEFALL);

  //Initialize PulseSensor
  samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;
  lastReportMicros = 0L;
  resetJitter();
  wantMicros = micros() + MICROS_PER_READ;

  //Initialize GPS
  gps.begin(9600);
  if(wantPrint == HIGH)
  {
    Serial.println("Starting The Echidna.");
    Serial.println();
  }
  switchState = digitalRead(power);

  if(switchState == HIGH) // if power switch is on
  {
    startSound();
  }
}

void loop() 
{
  switchState = digitalRead(power);

  if (switchState == LOW) { //if power switch is off
    if (wantPrint == HIGH)
    {
      Serial.println("The Echidna is off. Switch to on to begin operation.");
      Serial.println();
      delay(500); //wait a moment if printing
    }
    prevSS = switchState;
    return;
  }

  if (prevSS == LOW) { //if system was off
    //reset variables
    prevTime = micros();
    criticalCount = 0;
    digitalWrite(criticalLED, LOW);
    //digitalWrite(buzzer, LOW); //speaker
    noTone(buzzer); //turn of speaker
    samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;
    lastReportMicros = 0L;
    resetJitter();
    wantMicros = micros() + MICROS_PER_READ;
    lastPrint = millis();
    gps.begin(9600);
    startSound();
  }
  prevSS = switchState;
  //read monitors
  int heart = readHeartMonitor();
  int temp = readTempSensor(); 
  detectFall();
  //readGPS();
  
  if (isCritical(heart, temp) == HIGH) 
  {
    criticalCount++;
  }
  
  else if (criticalCount > 0) 
  {
    criticalCount--;
  }
  
  if (criticalCount >= cc) {
    critical();
  }

  if (wantPrint == HIGH && millis() - lastPrint >= 2000){
    lastPrint = millis();
    printForDemo(heart, temp);
  }

  delayMicroseconds(x); //wait a short amount of time before reading sensors again

}

int readHeartMonitor() {
 unsigned long nowMicros = micros();
 if ((long) (wantMicros - nowMicros) > 1000L) {
   return BPM;
 }
 
 if ((long) (wantMicros - nowMicros) > 3L + OFFSET_MICROS) {
   delayMicroseconds((unsigned int) (wantMicros - nowMicros) - OFFSET_MICROS);
   nowMicros = micros();    
 }
 
 long jitterMicros = (long) (nowMicros - wantMicros);
 
 if (minJitterMicros > jitterMicros) {
   minJitterMicros = jitterMicros;
 }
 
 if (maxJitterMicros < jitterMicros) {
   maxJitterMicros = jitterMicros;
 }
 
 wantMicros = nowMicros + MICROS_PER_READ;
 boolean QS = pulseDetector.readSensor();
 
 if (--samplesUntilReport == (byte) 0) {
   samplesUntilReport = SAMPLES_PER_SERIAL_SAMPLE;
 }
 
 if (QS) {
   BPM = pulseDetector.getBPM();
 }
 return BPM;

}


int readTempSensor() { 
  int reading = analogRead(tempSensor); //reads the value on the pin

  //converts reading into a voltage
  float voltage = reading * 5.0; 
  voltage /= 1024.0;

  //converts reading to fahrenheit
  float temperatureF = (((voltage - 0.5) * 100) * 9.0 / 5.0) + 32.0;

  return (int) temperatureF;
}


bool isCritical(int heart, int temp) { 
  if (heart < lowRate or (heart > highRate and heart < 300))
  {
    return HIGH;
  }
  if (temp < lowTemp or temp > highTemp) { 
    return HIGH;
  }
  else return LOW;
}


void detectFall() { //read accelerometer to detect fall
  //detect freefall
  loopTime = millis();
  if (abs(loopTime - interruptsTime) < 1000 )
  {
    critical();
  }  
  else
    return;
}


static void eventCallback() {
  if (CurieIMU.getInterruptStatus(CURIE_IMU_FREEFALL)) {
    if(wantPrint == HIGH)
      Serial.println("free fall detected! ");
      Serial.println();
    interruptsTime = millis();
  }
}


void critical() {
  unsigned long startTime = millis();
  //turn on LED and buzzer
  digitalWrite(criticalLED, HIGH);
  //digitalWrite(buzzer, HIGH); //speaker now
  tone(buzzer, NOTE);
  while (millis() - startTime < (WAIT_TIME * 1000)) 
  {
    buttonState = digitalRead(cancel);
    if (wantPrint == HIGH && millis() - lastPrint >= 1000)
    {
      Serial.println("CRITICAL: Push button if no help is required.\n");
      lastPrint = millis();
    }
    if (buttonState == LOW) { //if person indicates they are okay
      digitalWrite(criticalLED, LOW); //turn of LED and buzzer
      //digitalWrite(buzzer, LOW); //speaker now
      noTone(buzzer);
      criticalCount = 0;
      if (wantPrint == HIGH)
      {
        Serial.println("User has indicated that no help is required.");
        Serial.println("Resuming regular operation.");
        Serial.println();
      }
      return;
    }
  }
  //if button hasnt been pressed, implement emergency procedure
  emergencyProcedure();
}


void emergencyProcedure() { //user read to be in critical condition
  digitalWrite(criticalLED, LOW); //turn off LED to conserve power
  //digitalWrite(buzzer, HIGH); //make sure buzzer is on so person can be located easier
  tone(buzzer, NOTE);
  //gps.flush(); //flush previous data from gps
  gps.begin(4800);
  while(1){
    sendSOS();
    if (wantPrint == HIGH && millis() - lastPrint >= 1000){
      Serial.println("DANGER: Help is being contacted. Press button if no help is required.");
      Serial.println();
      lastPrint = millis();
    }
    buttonState = digitalRead(cancel);
    if (buttonState == LOW) { //if person indicates they are okay
      //digitalWrite(buzzer, LOW); //speaker now
      noTone(buzzer);
      criticalCount = 0;
      if (wantPrint == HIGH)
      {
        Serial.println("User has indicated that no help is required.");
        Serial.println("Resuming regular operation.");
        Serial.println();
      }
      //gps.begin(9600);
      return;
    }
  }
}

void resetJitter() 
{
 // min = a number so large that any value will be smaller than it;
 // max = a number so small that any value will be larger than it.
 minJitterMicros = 60 * 1000L;
 maxJitterMicros = -1;
}

void parseData(){
  //Check if the line is GPGLL
  if(data.substring(1, 6) == "GPGLL"){
    //Check if it is East
    if(data.indexOf('E') > 0){
      location = data.substring(7, data.indexOf('E') + 1);
    }
    //It is West
    else{
      location = data.substring(7, data.indexOf('W') + 1); 
    }

  }
}

void readGPS() { /*THIS MIGHT NEED INPUTS OR SOMETHING*/
   if(gps.available()){
    //Serial.println("Grabbing GPS Data.");
    c = gps.read();
    //Check to see if there was a newline character
    if(c == '\n'){
      parseData();
      //Serial.println("New Line Being Printed");
      data = "";
    }
    else{
      data += c;
    }
    //Serial.write(c);
    
    /*
    byte buffer[location.length()];
    location.getBytes(buffer, location.length());  
    
    for(int i = 0; i < ){
      Serial.write  
    }
    */
  }

  if(Serial.available()){
    //Serial.println(Serial.read());
  }
}

void sendSOS()
{
  String message = "SOS Location is " + location + "+";

  for(int i = 0; i < message.length(); ++i)
  {
    gps.write(message[i]);
  }
}

void printForDemo(int heart, int temp) {
  Serial.print("Heart Rate: ");
  Serial.println(heart);
  Serial.print("Temperature: ");
  Serial.println(temp);
  Serial.print("Location: ");
  Serial.println(location);
  Serial.print("Critical Count: ");
  Serial.println(criticalCount);
  Serial.println("");
}

void startSound(){
  tone(buzzer,NOTE_E6,125);
  delay(130);
  tone(buzzer,NOTE_G6,125);
  delay(130);
  tone(buzzer,NOTE_E7,125);
  delay(130);
  tone(buzzer,NOTE_C7,125);
  delay(130);
  tone(buzzer,NOTE_D7,125);
  delay(130);
  tone(buzzer,NOTE_G7,125);
  delay(125);
  noTone(buzzer);
  delay(1000);
  
}
