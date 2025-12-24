#include "main.hpp"
#include <Arduino.h>
#include <TMC2209.h>

#define BAUD_RATE 9600

// Pins
// Software serial ports should only be used for unidirectional communication
// The RX pin does not need to be connected, but it must be specified when
// creating an instance of a SoftwareSerial object
const int RX_PIN = PIN4;
const int TX_PIN = PIN5;
const int TRIGGER_PIN = PIN3;
const int SHUTTER_PIN = PIN2;
const int RUNNING_PIN = PIN6;

// Delays
const int SHUTTER_DELAY = 300;

// Instantiate TMC2209
TMC2209 driver;
SoftwareSerial soft_serial(RX_PIN, TX_PIN);
bool enabled;

void setup()
{
  bool enabled = digitalRead(RUNNING_PIN);
  // Connect Monitor
  Serial.begin(BAUD_RATE);
  // Init Stepper
  driver.setup(soft_serial);
  // Init shutter
  digitalWrite(SHUTTER_PIN, LOW);
}

void loop()
{
  bool enabled = digitalRead(RUNNING_PIN);
  if (enabled)
  {
    driver.enable();
    scanLoop();
    driver.disable();
  }
  delay(3000);
}

void scanLoop()
{
  while (enabled)
  {
    scanPhoto();
  }
}

void scanPhoto()
{
  moveToNextPhoto();
  takePicture();
}

void moveToNextPhoto()
{
  bool triggerActive = digitalRead(TRIGGER_PIN);
  if (triggerActive)
  {
    // Photo was just scanned, move on until trigger isn't triggered
    Serial.println("Moving on to next photo");
    moveUntilTriggerChanges();
  }
  // Move till next photo
  Serial.println("Waiting for trigger");
  moveUntilTriggerChanges();
}

void moveUntilTriggerChanges()
{
}

void takePicture(){
  Serial.println("Taking Photo");
  digitalWrite(SHUTTER_PIN, HIGH);
  delay(SHUTTER_DELAY);
  digitalWrite(SHUTTER_PIN, LOW);
}
