#include <CurieIMU.h>
#include <PulseSensorBPM.h>

//set pin numbers:
const int power = 7;   //pin number for power switch
const int cancel = 8; //pin number for alarm cancel button
const int criticalLED = 2; //pin number for LED for critical warning
const int buzzer = 4; //pin number for buzzer
const int pulseSensor = 13; //pin number for pulse sensor
const int tempSensor = A0; //pin number for temperature sensor

//global variables
int criticalCount;
const int WAIT_TIME = 90; //seconds to push cancel button after sensors read as critical
int switchState;
int buttonState;
const int x = 5; /*MIGHT NEED TO CHANGE THIS VALUE*/
int cc;
int prevTime;
unsigned long loopTime = 0;          // get the time since program started
unsigned long interruptsTime = 0;    // get the time when free fall event is detected

//pulse sensor variables
 const int PIN_INPUT = A0;
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
 PulseSensorBPM pulseDetector(PIN_INPUT, MICROS_PER_READ / 1000L);

void setup() {
  //initialize inputs and outputs
  pinMode(power, INPUT);
  pinMode(cancel, INPUT);
  pinMode(criticalLED, OUTPUT);
  pinMode(buzzer, OUTPUT);

  //initialize variables and states
  criticalCount = 0;
  digitalWrite(criticalLED, LOW);
  digitalWrite(buzzer, LOW);
  cc = (x * pow(10, -6)) / 30;
  prevTime = micros();

  Serial.begin(9600); // initialize Serial communication
  while (!Serial) ;   // wait for serial port to connect.

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
}

void loop() {
  //loop repeatedly
  switchState = digitalRead(power);
  if (switchState == LOW) { //if power switch is off
    return;
  }
  //read monitors
  int heart = readHeartMonitor(); /*MAKE SURE OUTPUT TYPE AND INPUTS ARE SAME AS FUNCTION*/
  int temp = readTempSensor(); /*MAKE SURE OUTPUT TYPE AND INPUTS ARE SAME AS FUNCTION*/
  detectFall();
  if (isCritical(heart, temp) == HIGH) {
    criticalCount++;
  }
  else {
    criticalCount = 0;
  }
  if (criticalCount == cc) {
    emergencyProcedure();
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


int readTempSensor() { /*MIGHT NEED TO CHANGE OUTPUT TYPE AND POSSIBLY ADD INPUTS*/
  
}


bool isCritical(int heart, int temp) { /*CHANGE DATA TYPES TO MATCH OTHERS*/
  if (heart < 40 or heart > 220) {
    return HIGH;
  }
  if (temp) { /*FIX THIS STATEMENT FOR CRITICAL TEMPS*/
    return HIGH;
  }
  else return LOW;
}


void detectFall() { //read accelerometer to detect fall
  //detect freefall
  loopTime = millis();
  if (abs(loopTime - interruptsTime) < 1000 )
    critical();
  else
    return;
}


static void eventCallback() {
  if (CurieIMU.getInterruptStatus(CURIE_IMU_FREEFALL)) {
    Serial.println("free fall detected! ");
    interruptsTime = millis();
  }
}


void critical() {
  unsigned long startTime = millis();
  //turn on LED and buzzer
  digitalWrite(criticalLED, HIGH);
  digitalWrite(buzzer, HIGH);
  while (millis() - startTime < (WAIT_TIME * 1000)) {
    buttonState = digitalRead(cancel);

    if (buttonState == HIGH) { //if person indicates they are okay
      digitalWrite(criticalLED, LOW); //turn of LED and buzzer
      digitalWrite(buzzer, LOW);
      criticalCount = 0;
      return;
    }
  }
  //if button hasnt been pressed, implement emergency procedure
  emergencyProcedure();
}


void emergencyProcedure() { //user read to be in critical condition
  digitalWrite(criticalLED, LOW); //turn off LED to conserve power
  digitalWrite(buzzer, HIGH); //make sure buzzer is on so person can be located easier
  
  while(1){
    /*GPS AND SOS STUFF*/
    buttonState = digitalRead(cancel);
    if (buttonState == HIGH) { //if person indicates they are okay
      digitalWrite(buzzer, LOW);
      criticalCount = 0;
      /*TURN OFF GPS AND SIGNAL*/
      return;
  }
}

void resetJitter() 
{
 // min = a number so large that any value will be smaller than it;
 // max = a number so small that any value will be larger than it.
 minJitterMicros = 60 * 1000L;
 maxJitterMicros = -1;
}
