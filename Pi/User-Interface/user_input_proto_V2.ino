#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// Arduino Setup:
// - Encoder: 
//   3 pin side: Left pin -> D0, Right pin -> D1, middle pin -> GND
//   2 pin side: Left -> GND, Right -> D2
// - LEDs:
//   Power: red wire -> 5V, black wire -> GND
//   Data: DIN -> ~500 ohms -> D3, DOUT -> ~500 ohms -> GND

static int pinA = 2; // Our first hardware interrupt pin is digital pin 2
static int pinB = 3; // Our second hardware interrupt pin is digital pin 3
static int selectButton = 4; // Digital pin # for selection button
static int dinLED = 5; // Data input for LEDs
const int numLeds = 7;

const String emotions[] = {"happy", "sad", "angry", "anxious", "energetic", "tired", "content"};
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
bool inputReady = true;

static int oneRotation = 24; // 1 rotation = 24 clicks
volatile byte aFlag = 0; // let's us know when we're expecting a rising edge on pinA to signal that the encoder has arrived at a detent
volatile byte bFlag = 0; // let's us know when we're expecting a rising edge on pinB to signal that the encoder has arrived at a detent (opposite direction to when aFlag is set)
volatile byte encoderPos = 0; //this variable stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255
volatile byte oldEncPos = 0; //stores the last encoder position value so we can compare to the current reading and see if it has changed (so we know when to print to the serial monitor)
volatile byte reading = 0; //somewhere to store the direct values we read from our interrupt pins before checking to see if we have moved a whole detent

void setup() {
  pinMode(pinA, INPUT_PULLUP); // set pinA as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  pinMode(pinB, INPUT_PULLUP); // set pinB as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  pinMode(selectButton, INPUT_PULLUP);
  pinMode(dinLED, OUTPUT);
  strip.setBrightness(255);
  attachInterrupt(0,PinA,RISING); // set an interrupt on PinA, looking for a rising edge signal and executing the "PinA" Interrupt Service Routine (below)
  attachInterrupt(1,PinB,RISING); // set an interrupt on PinB, looking for a rising edge signal and executing the "PinB" Interrupt Service Routine (below)
  Serial.begin(115200); // start the serial monitor link
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
  sei(); //restart interrupts
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

void selectEmotion(){
  String Emotion;
  if (encoderPos == 0 && encoderPos < 3){
    ledIndex = 0;
    strip.setPixelColor(ledIndex, red);
    strip.show();
  }
  else if (encoderPos >= 3 && encoderPos < 7){
    ledIndex = 1;
    strip.setPixelColor(ledIndex, orange);
    strip.show();
  }
  else if (encoderPos >= 7 && encoderPos < 10){
    ledIndex = 2;
    strip.setPixelColor(ledIndex, yellow);
    strip.show();
  }  
  else if (encoderPos >= 10 && encoderPos < 13){
    ledIndex = 3;
    strip.setPixelColor(ledIndex, green);
    strip.show();
  }
  else if (encoderPos >= 13 && encoderPos < 17){
    ledIndex = 4;
    strip.setPixelColor(ledIndex, cyan);
    strip.show();
  }
  else if (encoderPos >= 17 && encoderPos < 20){
    ledIndex = 5;
    strip.setPixelColor(ledIndex, blue);
    strip.show();
  }
  else if (encoderPos >= 20){
    ledIndex = 6;
    strip.setPixelColor(ledIndex, magenta);
    strip.show();
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
  delay(200);
}

void requestEvent() {
  // Send data when requested by the master
  Wire.write((byte*)percentages, sizeof(percentages)); // Example: send LED counts
}

void receiveEvent(int bytes) {
  // Receive data sent by the master
  while (Wire.available()) {
    int c = Wire.read();
    if (c == 1) {
      inputReady = true; // Example action
      Serial.print("Ready for user input...");
    }
  }
}
