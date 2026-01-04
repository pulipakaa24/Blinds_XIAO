#include "servo.hpp"
#include "driver/ledc.h"
#include "defines.h"
#include <freertos/FreeRTOS.h>
#include "esp_log.h"
#include "socketIO.hpp"
#include "nvs_flash.h"

std::atomic<bool> calibListen{false};
std::atomic<int32_t> baseDiff{0};
std::atomic<int32_t> target{0};

std::atomic<bool> runningManual{false};
std::atomic<bool> runningServer{false};
std::atomic<bool> clearCalibFlag{false};
std::atomic<bool> savePosFlag{false};
std::atomic<bool> startLess{false};

void servoInit() {
  // LEDC timer configuration (C++ aggregate initialization)
  ledc_timer_config_t ledc_timer = {};
  ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
  ledc_timer.timer_num = LEDC_TIMER_0;
  ledc_timer.duty_resolution = LEDC_TIMER_16_BIT;
  ledc_timer.freq_hz = 50;
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  // LEDC channel configuration
  ledc_channel_config_t ledc_channel = {};
  ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
  ledc_channel.channel = servoLEDCChannel;
  ledc_channel.timer_sel = LEDC_TIMER_0;
  ledc_channel.intr_type = LEDC_INTR_DISABLE;
  ledc_channel.gpio_num = servoPin;
  ledc_channel.duty = offSpeed; // Start off
  ledc_channel.hpoint = 0;
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  // Configure servo power switch pin as output
  gpio_set_direction(servoSwitch, GPIO_MODE_OUTPUT);
  gpio_set_level(servoSwitch, 0); // Start with servo power off

  topEnc->count = servoReadPos();
  if (Calibration::getCalibrated()) initMainLoop();
}

void servoOn(uint8_t dir, uint8_t manOrServer) {
  servoMainSwitch(1);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, servoLEDCChannel, (dir ? ccwSpeed : cwSpeed));
  ledc_update_duty(LEDC_LOW_SPEED_MODE, servoLEDCChannel);
  runningManual = !manOrServer;
  runningServer = manOrServer;
}

void servoOff() {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, servoLEDCChannel, offSpeed);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, servoLEDCChannel);
  runningManual = false;
  runningServer = false;
  servoMainSwitch(0);
}

void servoMainSwitch(uint8_t onOff) {
  gpio_set_level(servoSwitch, onOff ? 1 : 0);
}

bool servoInitCalib() {
  topEnc->pauseWatchdog();

  // get ready for calibration by clearing all these listeners
  bottomEnc->wandListen = false;
  topEnc->wandListen = false;
  topEnc->serverListen = false;
  if (!Calibration::clearCalibrated()) return false;
  if (topEnc == nullptr || bottomEnc == nullptr) {
    printf("ERROR: CALIBRATION STARTED BEFORE SERVO INITIALIZATION\n");
    return false;
  }
  baseDiff = bottomEnc->getCount() - topEnc->getCount();
  calibListen = true;
  return true;
}

void servoCalibListen() {
  int32_t effDiff = (bottomEnc->getCount() - topEnc->getCount()) - baseDiff;
  if (effDiff > 1) servoOn(CCW, manual);
  else if (effDiff < -1) servoOn(CW, manual);
  else servoOff();
}

bool servoBeginDownwardCalib() {
  calibListen = false;
  servoOff();
  vTaskDelay(pdMS_TO_TICKS(1000));
  if (!Calibration::beginDownwardCalib(*topEnc)) return false;
  baseDiff = bottomEnc->getCount() - topEnc->getCount();
  calibListen = true;
  return true;
}

bool servoCompleteCalib() {
  calibListen = false;
  servoOff();
  vTaskDelay(pdMS_TO_TICKS(1000));
  if (!Calibration::completeCalib(*topEnc)) return false;
  initMainLoop();
  return true;
}

void initMainLoop() {
  topEnc->setupWatchdog();
  servoSavePos();
  bottomEnc->wandListen = true;
}

void IRAM_ATTR watchdogCallback(void* arg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  
  if (runningManual || runningServer) {
    // if we're trying to move and our timer ran out, we need to recalibrate
    xEventGroupSetBitsFromISR(g_system_events, EVENT_CLEAR_CALIB, &xHigherPriorityTaskWoken);
    topEnc->pauseWatchdog();

    // get ready for recalibration by clearing all these listeners
    bottomEnc->wandListen = false;
    topEnc->wandListen = false;
    topEnc->serverListen = false;
    servoOff();
  }
  else {
    // if no movement is running, we're fine
    // save current servo-encoder position for reinitialization
    xEventGroupSetBitsFromISR(g_system_events, EVENT_SAVE_POSITION, &xHigherPriorityTaskWoken);
  }
  // clear running flags
  runningManual = false;
  runningServer = false;
  
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void servoSavePos() {
  // save current servo-encoder position for use on reinitialization
  nvs_handle_t servoHandle;
  if (nvs_open(nvsServo, NVS_READWRITE, &servoHandle) == ESP_OK) {
    int32_t topCount = topEnc->getCount();
    if (nvs_set_i32(servoHandle, posTag, topCount) != ESP_OK)
      printf("Error saving current position\n");
    else printf("Success - Current position saved as: %d\n", topCount);
    nvs_commit(servoHandle);
    nvs_close(servoHandle);
  }
  else {
    printf("Error opening servoPos NVS segment.\n");
  }
}

int32_t servoReadPos() {
  // save current servo-encoder position for use on reinitialization
  int32_t val = 0;
  nvs_handle_t servoHandle;
  if (nvs_open(nvsServo, NVS_READONLY, &servoHandle) == ESP_OK) {
    if (nvs_get_i32(servoHandle, posTag, &val) != ESP_OK)
      printf("Error reading current position\n");
    else printf("Success - Current position read as: %d\n", val);
    nvs_close(servoHandle);
  }
  else {
    printf("Error opening servoPos NVS segment.\n");
  }
  return val;
}

void stopServerRun() {
  // stop listener and stop running if serverRun is still active.
  topEnc->serverListen = false;
  if (runningServer) servoOff();
}

void servoWandListen() {
  // stop any remote-initiated movement
  stopServerRun();

  // freeze atomic values
  int32_t upBound = Calibration::UpTicks;
  int32_t downBound = Calibration::DownTicks;
  int32_t bottomCount = bottomEnc->getCount();
  int32_t topCount = topEnc->getCount();

  // ensure the baseDiff doesn't wait on wand to turn all the way back to original range.
  if ((upBound > downBound && bottomCount - baseDiff > upBound)
       || (upBound < downBound && bottomCount - baseDiff < upBound))
    baseDiff = bottomCount - upBound;
  else if ((upBound > downBound && bottomCount - baseDiff < downBound)
            || (upBound < downBound && bottomCount - baseDiff > downBound))
    baseDiff = bottomCount - downBound;
  
  // calculate the difference between wand and top servo
  int32_t effDiff = (bottomCount - topCount) - baseDiff;

  // if we are at either bound, stop servo and servo-listener
  // if effective difference is 0, stop servo and servo-listener
  // otherwise, run servo in whichever direction necessary and
  // ensure servo-listener is active.
  if (abs(topCount - upBound) <= 1 || abs(topCount - downBound) <= 1) {
    servoOff();
    topEnc->wandListen = false;
  }
  else if (effDiff > 1) {
    topEnc->wandListen = true;
    servoOn(CCW, manual);
  }
  else if (effDiff < -1) {
    topEnc->wandListen = true;
    servoOn(CW, manual);
  }
  else {
    servoOff();
    topEnc->wandListen = false;
  }
}

void servoServerListen() {
  // If we have reached or passed our goal, stop running and stop listener.
  if (topEnc->getCount() >= target && startLess) stopServerRun();
  else if (topEnc->getCount() <= target && !startLess) stopServerRun();
  baseDiff = bottomEnc->getCount() - topEnc->getCount();
}

void runToAppPos(uint8_t appPos) {
  // manual control takes precedence over remote control, always.
  // also do not begin operation if not calibrated;
  if (runningManual || !Calibration::getCalibrated()) return;
  servoOff();

  // allow servo position to settle
  vTaskDelay(pdMS_TO_TICKS(500));
  int32_t topCount = topEnc->getCount();
  target = Calibration::convertToTicks(appPos); // calculate target encoder position
  if (abs(topCount - target) <= 1) return;
  startLess = topCount < target;
  if (runningManual) return; // check again before starting remote control
  if (startLess) servoOn(CCW, server); // begin servo movement
  else servoOn(CW, server);
  topEnc->serverListen = true; // start listening for shutoff point
}

// Servo control task - processes encoder events and servo commands
void servoControlTask(void* arg) {
    encoder_event_t enc_event;
    servo_cmd_msg_t cmd;
    
    printf("Servo control task started\n");
    
    while (1) {
        // Block waiting for encoder events (higher priority)
        if (xQueueReceive(g_encoder_event_queue, &enc_event, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Process encoder event (work that was done in ISR before)
            
            // Handle calibration listening
            if (calibListen) {
                int32_t effDiff = (bottomEnc->getCount() - topEnc->getCount()) - baseDiff;
                if (effDiff > 1) {
                    servoOn(CCW, manual);
                }
                else if (effDiff < -1) {
                    servoOn(CW, manual);
                }
                else {
                    servoOff();
                }
            }
            
            // Only process top encoder events for watchdog and listeners
            if (enc_event.is_top_encoder) {
                // Feed watchdog in task context (not ISR)
                if (topEnc->feedWDog) {
                    esp_timer_restart(topEnc->watchdog_handle, 500000);
                }
                
                // Check wand listener - now safe in task context
                if (topEnc->wandListen) {
                    servoWandListen();
                }
                
                // Check server listener - now safe in task context
                if (topEnc->serverListen) {
                    servoServerListen();
                }
            }
        }
        
        // Check for direct servo commands (lower priority)
        if (xQueueReceive(g_servo_command_queue, &cmd, 0) == pdTRUE) {
            switch (cmd.command) {
                case SERVO_CMD_STOP:
                    servoOff();
                    break;
                case SERVO_CMD_MOVE_CCW:
                    servoOn(CCW, cmd.is_manual ? manual : server);
                    break;
                case SERVO_CMD_MOVE_CW:
                    servoOn(CW, cmd.is_manual ? manual : server);
                    break;
                case SERVO_CMD_MOVE_TO_POSITION:
                    runToAppPos(cmd.target_position);
                    break;
            }
        }
    }
}