#include <Adafruit_NeoPixel.h>
//#include <Stepper.h>
#include <Wire.h>
#include <EEPROM.h>
#include "DualVNH5019MotorShield.h"
DualVNH5019MotorShield md;

// Motor Function
// Encoder + Switch Reading
// I2C send list
// I2C recieve proximity sensor input
// User Interface + Percentage calculation for List 
// 

const int pinA = 5; // Our first hardware interrupt pin is digital pin 5
const int pinB = 3; // Our second hardware interrupt pin is digital pin 3
const int selectButton = 1; // Digital pin # for selection button
const int dinLED = 13; // Data out for user LEDs
const int proximity_test = 17; //LED pin for proximity sensor
const int numLeds = 58; // num total LEDs with series addressing for UI and Display

#define I2C_ADDRESS 0x08  // Use this address in Python

const String emotions[] = {"happy", "sad", "disgusted", "angry", "fearful", "bad", "surprised"};
const int emotionDisplayIndex [] = {7, 10, 13, 16, 19, 22, 25}; //small flower beginning LED index for each emotion (each flower has a 5 parallel strands of 3 series LEDs)
const int mainFlowerIndex[] = {28, 57}; // main flower beginning and end index (all of 30 LEDs are in series)
int ledIndex = 0;
int ledCount[7] = {0}; // Array to store weights for each LED
static float percentages[7]; // Static to ensure the array persists after function returns

Adafruit_NeoPixel strip(numLeds, dinLED, NEO_RGB + NEO_KHZ800); //User Interface Object
//set user interface strip colors
uint32_t red = strip.Color(0, 255, 0);
uint32_t green = strip.Color(255, 0, 0);
uint32_t blue = strip.Color(0, 0, 255);
uint32_t magenta = strip.Color(0, 200, 255);
uint32_t yellow = strip.Color(255, 255, 0);
uint32_t cyan = strip.Color(255, 0, 255);
uint32_t orange = strip.Color(50, 255, 0);
const uint32_t colors[] = {orange, red, magenta, blue, cyan, green, yellow};
//set display colors

bool buttonPressed = false;
bool userDetected = true;
bool PIRActive = true;
bool flowerOpened = false;
bool flowerDisplay = false;

static int oneRotation = 24; // 1 rotation = 24 clicks
volatile byte aFlag = 0; // let's us know when we're expecting a rising edge on pinA to signal that the encoder has arrived at a detent
volatile byte bFlag = 0; // let's us know when we're expecting a rising edge on pinB to signal that the encoder has arrived at a detent (opposite direction to when aFlag is set)
volatile byte encoderPos = 1; //this variable stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255
volatile byte oldEncPos = 0; //stores the last encoder position value so we can compare to the current reading and see if it has changed (so we know when to print to the serial monitor)
volatile byte reading = 0; //somewhere to store the direct values we read from our interrupt pins before checking to see if we have moved a whole detent
const int EEPROM_ADDRESS = 0; // EEPROM address to store encoderCounts

// The constant below will scale the peak output voltage to the motor by the percentage 
// declared.  For example, if you want the peak voltage to the motor to be equal to VIN,
// set percentOutput to 100.  If you want the peak voltage to the motor to be 80% of VIN, 
// set percentOutput to 80.  You should see what voltage the stepper motor is rated for 
// and set percentOutput appropriately.  The value of percentOutput should be between 0 
// and 100. 
#define VIN = 20; //20V input from 
const byte percentOutput = 16; // 16% of 20V = 3.2V

#define FULL_STEP      4
unsigned char stepMode = FULL_STEP; //keep on FULL_STEP = 200 steps per rev

// Set Number of Rotations to open and close flower
const int flowerRev = 8;
const int stepsPerRevolution = 200; 
const int speedDelay = 3000; // increasing will slow down/decreasing will speed up motor
const int brake = 400; //hard stop 
//Stepper motor(stepsPerRevolution, 6, 9, 10, 11);

void setup() {
  Wire.begin(I2C_ADDRESS);
  Serial.begin(115200); // start the serial monitor link
  Wire.onRequest(sendData);
  Wire.onReceive(receiveEvent);

  pinMode(pinA, INPUT_PULLUP); // set pinA as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  pinMode(pinB, INPUT_PULLUP); // set pinB as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  pinMode(selectButton, INPUT_PULLUP); // set encoder button to input pull-up

  pinMode(dinLED, OUTPUT);
  pinMode(proximity_test, OUTPUT);

  // motor.setSpeed(60);

  strip.setBrightness(255);
  attachInterrupt(digitalPinToInterrupt(pinB), encoderISR, CHANGE);

  //writeEncoderCountsToEEPROM(encoderPos); // comment in this line for the initial arduino programming, after first upload delete line and reupload
  encoderPos = readEncoderCountsFromEEPROM();
  // Serial.print("Initial encoderCounts (from EEPROM): ");
  // Serial.println(encoderPos);
  
  md.init(); //initialize motor

  randomSeed(analogRead(0)); // Generates random seed
}

volatile unsigned long lastEncoderTime = 0;  // Timestamp for debounce
const int debounceDelay = 5;  // 5ms debounce time

void encoderISR() {
  unsigned long currentTime = millis();
  if (currentTime - lastEncoderTime > debounceDelay) {  // Debounce check
    if (digitalRead(pinA) == digitalRead(pinB)) {
        if (encoderPos < 24){
          encoderPos++;  // Clockwise rotation
        }
        else if (encoderPos == 24){
          encoderPos = 1;
        }
    } else {
      if (encoderPos > 1){
          encoderPos--;  // Clockwise rotation
      }
      else if (encoderPos == 1){
          encoderPos = 24;
      }
    }
    if (encoderPos != oldEncPos) {
    // Serial.print("encoderCounts changed to: ");
    // Serial.println(encoderPos);
      writeEncoderCountsToEEPROM(encoderPos);
      oldEncPos = encoderPos; // Update previousCounts
      userDetected = true; // set flag if proximity sensors have not been tripped yet
    }
  delay(100);
  }
  lastEncoderTime = currentTime;  // Update last time
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
  // Serial.println("Value updated in EEPROM.");
}

float* amplitude(int ledCount[7], String emotion[7]) {
  int totalPresses = 0;
  if (buttonPressed == true){
    // Calculate the total number of presses
    for (int i = 0; i < 7; i++) {
      totalPresses += ledCount[i];
    }
    // Handle the case where there are no presses to avoid division by zero
    if (totalPresses == 0) {
      for (int i = 0; i < 7; i++) {
        percentages[i] = 0.0; // Set all to 0 if no presses
      }
      //Serial.println("No button presses recorded. Percentages are all 0.00%");
      return percentages;
    }

    // Calculate the percentage each button is pressed
    for (int i = 0; i < 7; i++) {
      percentages[i] = (ledCount[i] / (float)totalPresses);
    }
    //buttonPressed = false;
  }
  return percentages;
}
void proximityTest(){

}

// Function to read user input and change LEDs 
void selectEmotion(){
  String Emotion;
  strip.clear();
  if (encoderPos <= 3 || encoderPos == 24){
    // HAPPY
    ledIndex = 0;
    strip.setPixelColor(ledIndex, colors[ledIndex]); //set user interface LEDs
    // turn on LEDs for corresponding color & flower for that emotion @ full brightness the rest off, except center flower is alway displaying the color blend
    strip.fill(colors[ledIndex], emotionDisplayIndex[ledIndex], 3);
  }
  else if (encoderPos >= 4 && encoderPos < 7){
    // SAD
    ledIndex = 1;
    strip.setPixelColor(ledIndex, colors[ledIndex]);
    strip.fill(colors[ledIndex], emotionDisplayIndex[ledIndex], 3);
  }
  else if (encoderPos >= 7 && encoderPos < 10){
    // DISGUSTED
    ledIndex = 2;
    strip.setPixelColor(ledIndex, colors[ledIndex]);
    strip.fill(colors[ledIndex], emotionDisplayIndex[ledIndex], 3);
  }  
  else if (encoderPos >= 10 && encoderPos < 13){
    // ANGRY
    ledIndex = 3;
    strip.setPixelColor(ledIndex, colors[ledIndex]);
    strip.fill(colors[ledIndex], emotionDisplayIndex[ledIndex], 3);
  }
  else if (encoderPos >= 13 && encoderPos < 17){
    // FEARFUL
    ledIndex = 4;
    strip.setPixelColor(ledIndex, colors[ledIndex]);
    strip.fill(colors[ledIndex], emotionDisplayIndex[ledIndex], 3);
  }
  else if (encoderPos >= 17 && encoderPos < 20){
    // SAD
    ledIndex = 5;
    strip.setPixelColor(ledIndex, colors[ledIndex]);
    strip.fill(colors[ledIndex], emotionDisplayIndex[ledIndex], 3);
  }
  else if (encoderPos >= 20 && encoderPos < 24){
    // SURPRISED
    ledIndex = 6;
    strip.setPixelColor(ledIndex, colors[ledIndex]);
    strip.fill(colors[ledIndex], emotionDisplayIndex[ledIndex], 3);
  }
  strip.show();
  if (digitalRead(selectButton) == LOW && !buttonPressed){
    delay(50);// prevent debounce
    buttonPressed = true;
    Emotion = emotions[ledIndex];
    ledCount[ledIndex]++;
    // Serial.print("Emotion selected = ");
    // Serial.print(Emotion);
    // Serial.println();
    float* percentages = amplitude(ledCount, emotions);
    // printPressCounts(percentages);
    delay(100);
  }
}

// // Function to change overall display once input is complete
void displayFlowers(float* percentages){
  //strip.clear();
  int loops = 0;
  uint32_t modRed = strip.Color(0, int(255*percentages[1]), 0);
  uint32_t modGreen = strip.Color(int(255*percentages[5]), 0, 0);
  uint32_t modBlue = strip.Color(0, 0, int(255*percentages[3]));
  uint32_t modMagenta = strip.Color(0, int(200*percentages[2]), int(255*percentages[2]));
  uint32_t modYellow = strip.Color(int(255*percentages[6]), int(255*percentages[6]), 0);
  uint32_t modCyan = strip.Color(int(255*percentages[4]), 0, int(255*percentages[4]));
  uint32_t modOrange = strip.Color(int(50*percentages[0]), int(255*percentages[0]), 0);
  const uint32_t modColors[] = {modOrange, modRed, modMagenta, modBlue, modCyan, modGreen, modYellow};
  int redCount = 0;
  int greenCount = 0;
  int blueCount = 0;
  int magentaCount = 0;
  int yellowCount = 0;
  int cyanCount = 0;
  int orangeCount = 0;
  int colorCounts[] = {orangeCount, redCount, magentaCount, blueCount, cyanCount, greenCount, yellowCount};
  for(int i = 0; i < 7; i++) {
    strip.fill(modColors[i], emotionDisplayIndex[i], 3);
  }
  for(int j = 28; j < 58; j++) {
    int randNum = random(0,7);
    loops = 0;
    while(loops < 7){
      if(colorCounts[randNum] < (30*percentages[randNum])){
        strip.setPixelColor(j, modColors[randNum]);
        colorCounts[randNum] += 1;
        break;
      }
      else{
        randNum = (randNum + 1) % 7;
        loops += 1;
      }
    }
  }
  strip.show();
  //delay(10000); // ensure output display is shown for at least 5 seconds after user input
}

// Function to print the button press counts
void printPressCounts(float* percentages) {
  for (int i = 0; i < 7; i++) {
    Serial.print(emotions[i]);
    Serial.print(": ");
    Serial.print(percentages[i]*100);
    Serial.print("%  ");
  }
  Serial.println(); // Additional newline for spacing between prints
}

// This function will set the voltage applied to each coil. 
inline void set_speeds(int m1speed, int m2speed){
	md.setSpeeds(m1speed/2*percentOutput/50, m2speed/2*percentOutput/50);
}
inline void set_brakes(int m1Brake, int m2Brake){
  md.setBrakes(m1Brake, m2Brake);
}

// Advances the stepper motor by one step either clockwise or counterclockwise
// When full stepping, the four states are: 
// forwards: a = 0, 90, 180, 270 degrees
// reverse:  a = 0, 270, 180, 90 degrees
// half stepping comes from: a = 0, 45, 90, 135, 180, 225, 270, 315 degrees
// quarter stepping comes from a = the 16 multiples of 22.5 from 22.5 to 360 deg
void one_step(unsigned char dir){
   // this static variable lets us remember what step we're on so we
   // can change to the appropriate next state in the sequence
   static unsigned char step = 0;

   // compute the next step based on the direction argument dir
   // and the step mode.  Full stepping skips half and quarter steps,
   // and half stepping skips quarter steps.  Quarter stepping cycles
   // through all 16 steps.
   if (dir == 1)
      step += stepMode;
   else
      step -= stepMode;

   switch (step & 15)
   {
      case 0:   // full step (both coils energized at 71%)
         set_speeds(283, 283);
         break;
      case 1:   // quarter step (coil 1 at 38% and coil 2 at 93%)
         set_speeds(153, 370);
         break;
      case 2: // half step (coil 1 at 0% and coil 2 at 100%)
         set_speeds(0, 400);
         break;
      case 3: // quarter step
         set_speeds(-153, 370);
         break;
      case 4: // full step
         set_speeds(-283, 283);
         break;
      case 5: // quarter step
         set_speeds(-370, 153);
         break;
      case 6: // half step
         set_speeds(-400, 0);
         break;
      case 7: // quarter step
         set_speeds(-370, -153);
         break;
      case 8: // full step
         set_speeds(-283, -283);
         break;
      case 9: // quarter step
         set_speeds(-153, -370);
         break;
      case 10: // half step
         set_speeds(0, -400);
         break;
      case 11: // quarter step
         set_speeds(153, -370);
         break;
      case 12: // full step
         set_speeds(283, -283);
         break;
      case 13: // quarter step
         set_speeds(370, -153);
         break;
      case 14: // half step
         set_speeds(400, 0);
         break;
      case 15: // quarter step
         set_speeds(370, 153);
         break;
   }
}

// This is a blocking function that repeatedly takes a single step and then
// delays for step_delay_us microseconds.  When it finishes, the stepper motor
// coils will continued to be energized according to the final step so that
// the stepper motor maintains its position and holding torque.
void multi_step(int steps, unsigned int step_delay_us){
   unsigned char dir = 1;
   if (steps < 0)
   {
      dir = 0;
      steps = -steps;
   }

   while (steps--)
   {
      one_step(dir);
      delayMicroseconds(step_delay_us);
   }
}

void move_flower(bool open){
  if (open == 1){
    multi_step(stepsPerRevolution*flowerRev, speedDelay);
  }
  else{
    multi_step(-stepsPerRevolution*flowerRev, speedDelay);
  }
  delay(100);
}

int prevEncPos = 0;

void loop(){
  // //displayFlowers(percentages);
  Serial.println(userDetected);
  if (PIRActive == false && (encoderPos != oldEncPos || encoderPos != prevEncPos)){
    userDetected = true;
  }
  if ((userDetected == true || encoderPos != prevEncPos) && flowerOpened == false){
    move_flower(1); //open flower
    Serial.println("Opening Flower");
    userDetected = true;
    flowerOpened = true;
  }
  else if (flowerOpened == true && userDetected == false && encoderPos == prevEncPos){
    move_flower(0); //close flower
    Serial.println("Closing Flower");
    flowerOpened = false;
  }
  //oldEncPos = 0;
  if (buttonPressed == false){
    selectEmotion();
    prevEncPos = encoderPos;
    flowerDisplay = false;
  }
  if (buttonPressed == true && flowerDisplay == false){
    //Serial.println("Start Display");
    displayFlowers(percentages);
    flowerDisplay = true;
   //keep display on until encoder is rotated again
  }
  if (encoderPos != prevEncPos){ //Only stop LED display when the encoder is rotated
    buttonPressed = false;
  }
  delay(200);
}


void sendData() {
  Wire.write((byte*)percentages, sizeof(percentages));
}

void receiveEvent(int bytes){
  // Receive data sent by the master
  while (Wire.available()) {
    int c = Wire.read();
    if (c == 1 && PIRActive == true) {
      userDetected = true; // Example action
      //Serial.println("Person detected within 6ft");
    }
    else if (c == 0 && PIRActive == true){
      userDetected = false;
    } 
    else if (c == 2) {
      PIRActive = true;
    } 
    else if (c == 3) {
      PIRActive = false;
    }
  }
}
