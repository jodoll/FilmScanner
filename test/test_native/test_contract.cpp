#include <gtest/gtest.h>

#include "scanner_core/ButtonConfig.h"
#include "scanner_core/CameraConfig.h"
#include "scanner_core/CommandParser.h"
#include "scanner_core/Config.h"
#include "scanner_core/ConfigRecord.h"
#include "scanner_core/DriverConfig.h"
#include "scanner_core/IClock.h"
#include "scanner_core/IEventSink.h"
#include "scanner_core/IInput.h"
#include "scanner_core/ILogSink.h"
#include "scanner_core/IMotionController.h"
#include "scanner_core/INonVolatileStore.h"
#include "scanner_core/IOutput.h"
#include "scanner_core/IStepperMotor.h"
#include "scanner_core/IStatusSink.h"
#include "scanner_core/ITmcDriver.h"
#include "scanner_core/LogConfig.h"
#include "scanner_core/MotionConfig.h"
#include "scanner_core/NotchConfig.h"
#include "scanner_core/RunConfig.h"
#include "scanner_core/StateMachine.h"
#include "scanner_core/Types.h"

namespace scanner {
namespace {

// Every header must be self-contained and compile on its own; this file includes
// all of them in one translation unit to prove that.
TEST(Contract, MotionConfigIsNotPacked) {
  // A packed domain type would make every slice reference potentially
  // unaligned. See ADR-0010.
  EXPECT_EQ(alignof(MotionConfig), alignof(int32_t));
}

}  // namespace
}  // namespace scanner
