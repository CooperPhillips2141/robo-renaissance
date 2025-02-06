#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <EEPROM.h>

// Arduino Setup:
// - Encoder: 
//   3 pin side: Left pin -> D2, Right pin -> D3, middle pin -> GND
//   2 pin side: Left -> GND, Right -> D4
// - LEDs:
//   Power: red wire -> 5V, black wire -> GND
//   Data: DIN -> ~500 ohms -> D5, DOUT -> ~500 ohms -> GND

static int pinA = 2; // Our first hardware interrupt pin is digital pin 2
static int pinB = 3; // Our second hardware interrupt pin is digital pin 3
static int selectButton = 4; // Digital pin # for selection button
static int dinLED = 5; // Data input for LEDs
static int proximity_test = 6; //LED pin for proximity sensor
const int numLeds = 7;

#define I2C_ADDRESS 0x08  // Use this address in Python

const String emotions[] = {"happy", "sad", "surprised", "bad", "fearful", "angry", "disgusted"};
int ledIndex = 0;
int ledCount[numLeds] = {0}; // Array to store weights for each LED
static float percentages[numLeds]; // Static to ensure the array persists after function returns

Adafruit_NeoPixel strip(numLeds, dinLED, NEO_RGB + NEO_KHZ800);
uint32_t red = strip.Color(0, 255, 0);
uint32_t green = strip.Color(255, 0, 0);
uint32_t blue = strip.Color(0, 0, 255);
uint32_t magenta = strip.Color(0, 200, 255);
uint32_t yellow = strip.Color(255, 255, 0);
uint32_t cyan = strip.Color(255, 0, 255);
uint32_t orange = strip.Color(50, 255, 0);

bool buttonPressed = false;
bool userDetected = false;

static int oneRotation = 24; // 1 rotation = 24 clicks
volatile byte aFlag = 0; // let's us know when we're expecting a rising edge on pinA to signal that the encoder has arrived at a detent
volatile byte bFlag = 0; // let's us know when we're expecting a rising edge on pinB to signal that the encoder has arrived at a detent (opposite direction to when aFlag is set)
volatile byte encoderPos = 0; //this variable stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255
volatile byte oldEncPos = 0; //stores the last encoder position value so we can compare to the current reading and see if it has changed (so we know when to print to the serial monitor)
volatile byte reading = 0; //somewhere to store the direct values we read from our interrupt pins before checking to see if we have moved a whole detent
const int EEPROM_ADDRESS = 0; // EEPROM address to store encoderCounts

void setup() {
  Wire.begin(I2C_ADDRESS);
  Wire.onRequest(sendData);
  Wire.onReceive(receiveEvent);

  pinMode(pinA, INPUT_PULLUP); // set pinA as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  pinMode(pinB, INPUT_PULLUP); // set pinB as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  pinMode(selectButton, INPUT_PULLUP);

  pinMode(dinLED, OUTPUT);
  pinMode(proximity_test, OUTPUT);
  strip.setBrightness(255);
  attachInterrupt(0,PinA,RISING); // set an interrupt on PinA, looking for a rising edge signal and executing the "PinA" Interrupt Service Routine (below)
  attachInterrupt(1,PinB,RISING); // set an interrupt on PinB, looking for a rising edge signal and executing the "PinB" Interrupt Service Routine (below)
  Serial.begin(115200); // start the serial monitor link
  
  encoderPos = readEncoderCountsFromEEPROM();
  Serial.print("Initial encoderCounts (from EEPROM): ");
  Serial.println(encoderPos);
}

void PinA(){
  cli(); //stop interrupts happening before we read pin values
  reading = PIND & 0xC; // read all eight pin values then strip away all but pinA and pinB's values
  if(reading == B00001100 && aFlag && encoderPos > 0) { //check that we have both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
    encoderPos --; //decrement the encoder's position count
    bFlag = 0; //reset flags for the next turn
    aFlag = 0; //reset flags for the next turn
  }
  else if (reading == B00001100 && aFlag && encoderPos == 0){
    encoderPos = 24;
  }
  
  else if (reading == B00000100) bFlag = 1; //signal that we're expecting pinB to signal the transition to detent from free rotation
    // Update EEPROM if the value has changed
  if (encoderPos != oldEncPos) {
    Serial.print("encoderCounts changed to: ");
    Serial.println(encoderPos);
    writeEncoderCountsToEEPROM(encoderPos);
    oldEncPos = encoderPos; // Update previousCounts
  }
  sei(); //restart interrupts
}

void PinB(){
  cli(); //stop interrupts happening before we read pin values
  reading = PIND & 0xC; //read all eight pin values then strip away all but pinA and pinB's values
  if (reading == B00001100 && bFlag && encoderPos < 24) { //check that we have both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
    encoderPos ++; //increment the encoder's position count
    bFlag = 0; //reset flags for the next turn
    aFlag = 0; //reset flags for the next turn
  }
  else if (reading == B00001100 && bFlag && encoderPos == 24){
    encoderPos = 0;
  }
  else if (reading == B00001000) aFlag = 1; //signal that we're expecting pinA to signal the transition to detent from free rotation
    // Update EEPROM if the value has changed
  if (encoderPos != oldEncPos) {
    Serial.print("encoderCounts changed to: ");
    Serial.println(encoderPos);
    writeEncoderCountsToEEPROM(encoderPos);
    oldEncPos = encoderPos; // Update previousCounts
  }
  sei(); //restart interrupts
}

int readEncoderCountsFromEEPROM() {
  // Read a 2-byte integer from EEPROM starting at EEPROM_ADDRESS
  int value = 0;
  EEPROM.get(EEPROM_ADDRESS, value); // Use EEPROM.get to read the value
  return value;
}

void writeEncoderCountsToEEPROM(int value) {
  // Write the integer value to EEPROM starting at EEPROM_ADDRESS
  EEPROM.put(EEPROM_ADDRESS, value); // Use EEPROM.put to write the value
  Serial.println("Value updated in EEPROM.");
}

float* amplitude(int ledCount[numLeds], String emotion[numLeds]) {
  int totalPresses = 0;

  if (buttonPressed == true){
    // Calculate the total number of presses
    for (int i = 0; i < numLeds; i++) {
      totalPresses += ledCount[i];
    }

    // Handle the case where there are no presses to avoid division by zero
    if (totalPresses == 0) {
      for (int i = 0; i < numLeds; i++) {
        percentages[i] = 0.0; // Set all to 0 if no presses
      }
      Serial.println("No button presses recorded. Percentages are all 0.00%");
      return percentages;
    }

    // Calculate the percentage each button is pressed
    for (int i = 0; i < numLeds; i++) {
      percentages[i] = (ledCount[i] / (float)totalPresses);
    }
    buttonPressed = false;
  }
  return percentages;
}
void proximityTest(){
  if (userDetected == true){
    //Serial.println("Raspi Proximity Sensor Success!");
    digitalWrite(proximity_test, HIGH);
    delay(100);
  }
  else{
    digitalWrite(proximity_test, LOW);
    //Serial.println("No Motion Dectected");
  }
}

void selectEmotion(){
  String Emotion;
  strip.clear();
  if (encoderPos <= 3 || encoderPos == 24){
    ledIndex = 0;
    strip.setPixelColor(ledIndex, red);
  }
  else if (encoderPos >= 4 && encoderPos < 7){
    ledIndex = 1;
    strip.setPixelColor(ledIndex, orange);
  }
  else if (encoderPos >= 7 && encoderPos < 10){
    ledIndex = 2;
    strip.setPixelColor(ledIndex, yellow);
  }  
  else if (encoderPos >= 10 && encoderPos < 13){
    ledIndex = 3;
    strip.setPixelColor(ledIndex, green);
  }
  else if (encoderPos >= 13 && encoderPos < 17){
    ledIndex = 4;
    strip.setPixelColor(ledIndex, cyan);
  }
  else if (encoderPos >= 17 && encoderPos < 20){
    ledIndex = 5;
    strip.setPixelColor(ledIndex, blue);
  }
  else if (encoderPos >= 20 && encoderPos < 24){
    ledIndex = 6;
    strip.setPixelColor(ledIndex, magenta);
  }
  if (digitalRead(selectButton) == LOW && !buttonPressed){
    buttonPressed = true;
    Emotion = emotions[ledIndex];
    ledCount[ledIndex]++;
    Serial.print("Emotion selected = ");
    Serial.print(Emotion);
    Serial.println();
    float* pressPercentages = amplitude(ledCount, emotions);
    buttonPressed = false;
    printPressCounts(pressPercentages);
  }
}

// Function to print the button press counts
void printPressCounts(float* percentages) {
  Serial.println("Button Press Counts:");
  for (int i = 0; i < numLeds; i++) {
    Serial.print(emotions[i]);
    Serial.print(": ");
    Serial.print(percentages[i]*100);
    Serial.print("%  ");
  }
  Serial.println(); // Additional newline for spacing between prints
}
void loop(){
  selectEmotion();
  strip.show();
  proximityTest();
  delay(100);
}


// void requestEvent(){
//   // Send data when requested by the master
//   Wire.write((byte*)percentages, sizeof(percentages)); // Example: send LED counts
// }
// Function to send 7-float list to Master

void sendData() {
    Wire.write((byte*)percentages, sizeof(percentages));  
}

void receiveEvent(int bytes){
  // Receive data sent by the master
  while (Wire.available()) {
    int c = Wire.read();
    if (c == 1) {
      userDetected = true; // Example action
      //Serial.println("Person detected within 6ft");
    }
    else if (c == 0){
      userDetected = false;
    }
  }
  }
