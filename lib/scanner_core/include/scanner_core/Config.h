#pragma once

#include "scanner_core/ButtonConfig.h"
#include "scanner_core/CameraConfig.h"
#include "scanner_core/DriverConfig.h"
#include "scanner_core/LogConfig.h"
#include "scanner_core/MotionConfig.h"
#include "scanner_core/NotchConfig.h"
#include "scanner_core/RunConfig.h"

namespace scanner {

// The aggregate owned by ConfigManager. Each slice is defined next to the class
// that consumes it.
struct Config {
  NotchConfig  notch;
  ButtonConfig button;
  CameraConfig camera;
  MotionConfig motion;
  RunConfig    run;
  DriverConfig driver;
  LogConfig    log;
};

}  // namespace scanner
