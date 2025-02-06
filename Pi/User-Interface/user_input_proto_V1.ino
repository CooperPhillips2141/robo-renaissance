#include <Wire.h>

// This code takes in the user input based on a potentimeter and switch button and sends it to the Raspberry Pi over I2C
// As the potentiometer is spun, a different led associated with 1 of 7 emotions will be illuminated for a visual

// Define the pins
const int potPin = A0; // Analog pin connected to potentiometer
const int ledPins[] = {2,3,4,5,6,7,8}; // Digital pins connected to LEDs
const String emotions[] = {"happy", "sad", "angry", "anxious", "energetic", "tired", "content"};
const int buttonPin = 11; // Digital pin connected to the button
const int numLeds = 7;

// Variables for button and tracking
bool buttonPressed = false;
bool inputReady = true;
bool newAudio = false;
int ledCount[numLeds] = {0}; // Array to store weights for each LED
int pressCount = 0;
int proximity = 0;
static float percentages[numLeds]; // Static to ensure the array persists after function returns

// Function to send data to Raspberry Pi
void sendDataToRaspberryPi(float floatList[], String stringList[], bool flag) {
  // Start transmission with a start character
  Serial.print("<");
  
  // Send float list
  for (int i = 0; i < 7; i++) {
    Serial.print(floatList[i]);
    if (i < 6) Serial.print(","); // Separate with commas
  }
  
  Serial.print("|"); // Separator between floats and strings
  
  // Send string list
  for (int i = 0; i < 7; i++) {
    Serial.print(stringList[i]);
    if (i < 6) Serial.print(","); // Separate with commas
  }
  
  Serial.print("|"); // Separator before the boolean
  
  // Send boolean value
  Serial.print(flag ? "1" : "0");
  
  // End transmission with an end character
  Serial.println(">");
}

// Function to receive data from Raspberry Pi
void receiveDataFromRaspberryPi(float floatList[], String stringList[], bool &flag) {
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('>');
    int floatCount = 0, stringCount = 0;
    int separatorIndex = data.indexOf("|");
    
    // Parse floats
    String floatPart = data.substring(1, separatorIndex);
    int lastIndex = 0;
    while (floatCount < 7 && lastIndex >= 0) {
      int commaIndex = floatPart.indexOf(",", lastIndex);
      String value = floatPart.substring(lastIndex, commaIndex == -1 ? floatPart.length() : commaIndex);
      floatList[floatCount++] = value.toFloat();
      lastIndex = commaIndex == -1 ? -1 : commaIndex + 1;
    }
    
    // Parse strings
    String stringPart = data.substring(separatorIndex + 1, data.lastIndexOf("|"));
    lastIndex = 0;
    while (stringCount < 7 && lastIndex >= 0) {
      int commaIndex = stringPart.indexOf(",", lastIndex);
      stringList[stringCount++] = stringPart.substring(lastIndex, commaIndex == -1 ? stringPart.length() : commaIndex);
      lastIndex = commaIndex == -1 ? -1 : commaIndex + 1;
    }
    
    // Parse boolean
    String boolPart = data.substring(data.lastIndexOf("|") + 1);
    flag = (boolPart.toInt() == 1);
  }
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

void selectEmotion(){
  String emotion;
  // Read the potentiometer value (0-1023)
  int potValue = analogRead(potPin);

  // Map the potentiometer value to an LED index (0 to numLeds-1)
  int ledIndex = map(potValue, 0, 1023, 0, numLeds);

  // Ensure the value is within bounds
  ledIndex = constrain(ledIndex, 0, numLeds - 1);

  // Turn off all LEDs
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], LOW);
  }

  // Turn on the corresponding LED
  digitalWrite(ledPins[ledIndex], HIGH);

  // Check if the button is pressed
  if (digitalRead(buttonPin) == LOW && !buttonPressed){
    newAudio = true;
    buttonPressed = true;
    emotion = emotions[ledIndex];
    ledCount[ledIndex]++;
    
    Serial.print("Emotion selected = ");
    Serial.print(emotion);
    Serial.println();
    float* pressPercentages = amplitude(ledCount, emotions);

    buttonPressed = false;
    printPressCounts(pressPercentages);
  }
}

void setup() {
  Wire.begin(8); // I2C address for Arduino (slave)
  Wire.onRequest(requestEvent); // Register a function to send data
  Wire.onReceive(receiveEvent); // Register a function to receive data
  // Set LED pins as output
  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
  }
  // Set button pin as input with pullup resistor
  pinMode(buttonPin, INPUT_PULLUP);
  // Initialize serial communication
  Serial.begin(9600);
}

void loop() {
  buttonPressed = false;
  selectEmotion();
  // Small delay for stability
  delay(50);
  inputReady = false;
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
