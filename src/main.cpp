#include <Arduino.h>

#include "scanner_core/StateMachine.h"

// Temporary reference so the Uno build compiles and links the core state
// machine. Replaced by the composition root in Milestone 6.
static scanner::StateMachine machine;

void setup()
{
  machine.scanOneFrame();
  machine.shootCurrentFrame();
  machine.jog();
  machine.startContinuousRun();
  machine.bootCompleted();
  machine.startContinuousRun();
  machine.notchDetected();
}

void loop()
{
}
