/************************************************************************
 * @file main.cpp
 * @date 2025/09/22 (Final Version)
 *
 ***********************************************************************/

/************************************************************************
 * Includes
 ************************************************************************/
#include <Arduino.h>
#include <version_generated.h>

#include <edugrid_states.h>
#include <edugrid_filesystem.h>
#include <edugrid_webserver.h>
#include <edugrid_pwm_control.h>
#include <edugrid_mpp_algorithm.h>
#include <edugrid_measurement.h>
#include <edugrid_logging.h>
#include <edugrid_telemetry.h>

/************************************************************************
 * Defines
 ************************************************************************/
/************************************************************************
 * RTOS Task Handles
 ************************************************************************/
// FreeRTOS allows us to pin work to a specific CPU core on the ESP32.  We keep
// one core busy with all networking tasks (HTTP + WebSocket pumping) and the
// other core dedicated to the fast MPPT + sensing loop so that timing stays
// deterministic even when a client is connected to the UI.
TaskHandle_t core2; // WebSocket & WiFi (core 0)
TaskHandle_t core3; // MPPT Algorithm and Sensors (core 1)

/************************************************************************
 * Task 2: WebSocket pump
 ************************************************************************/
void coreTwo(void *pvParameters)
{
  (void)pvParameters;
  for (;;)
  {
    // The websocket loop performs two things: housekeeping for the underlying
    // AsyncWebServer stack and the actual JSON broadcast of the live data.  We
    // call it periodically instead of from loop() so the UI stays responsive
    // regardless of what the rest of the firmware is doing.
    edugrid_webserver::webSocketLoop();
    // A short delay yields to other RTOS tasks while keeping the broadcast
    // cadence defined in edugrid_states.h.
    vTaskDelay(pdMS_TO_TICKS(TASK_WEBSOCKET_INTERVAL_MS));
  }
}

/************************************************************************
 * Task 3: Measurements + Borders + MPPT
 ************************************************************************/
void coreThree(void *pvParameters)
{
  (void)pvParameters;
  for (;;)
  {
    /* 1) Always update sensor cache first */
    // Update the cached measurements from both INA228s.  All other modules read
    // from this cache so it must be refreshed first.
    edugrid_measurement::getSensors();

    /* 2) Keep duty within safe/allowed borders (this is good practice) */
    // Keep the converter duty inside the configured safe window and honour the
    // manual slew limiter that makes slider movements smooth.
    edugrid_pwm_control::checkAndSetPwmBorders();
    edugrid_pwm_control::serviceManualRamp();

    /* 3) Execute the logic for the current operating mode */
    switch (edugrid_mpp_algorithm::get_mode_state())
    {
      case MANUALLY:
        // In Manual mode, the UI sets the PWM directly. Nothing to do here.
        break;

      case AUTO:
        // The find_mpp() function has its own internal timer.
        // We call it every loop, and it will only act when it's time.
        // Perturb & Observe algorithm adjusts the duty cycle when the MPPT
        // timer inside the module says it is time to sample again.
        edugrid_mpp_algorithm::find_mpp();
        break;

      case IV_SWEEP:
        // The iv_sweep_step() function acts as a state machine.
        // Calling it every loop tick drives the sweep forward one step at a time.
        // Advance the IV sweep state machine one step.  The helper manages its
        // own timing so we simply call it as fast as the task cadence allows.
        edugrid_mpp_algorithm::iv_sweep_step();
        break;
    }

#ifdef EDUGRID_TELEMETRY_ON
    edugrid_telemetry::telemetryPrint();
#endif

    /* 4) Loop timing */
    // Use a single, consistent delay for the whole task.
    // This makes the loop predictable and responsive.
    vTaskDelay(pdMS_TO_TICKS(TASK_CONTROL_INTERVAL_MS));
  }
}


/************************************************************************
 * setup()
 ************************************************************************/
void setup()
{
  /* Serial — start this FIRST so we see all boot logs */
  Serial.begin(EDUGRID_SERIAL_BAUD);
  delay(200);
  Serial.println();
  Serial.println(F("[BOOT] EduGrid starting..."));
  Serial.print  (F("[BOOT] Firmware version: ")); Serial.println(EDUGRID_VERSION);

#ifdef EDUGRID_GLOBAL_DEBUG
#warning "Debug mode is active"
  Serial.println(F("|WARN| Debug mode is ACTIVE"));
#endif

  /* Filesystem & Config */
  Serial.println(F("[FS] init_filesystem()"));
  edugrid_filesystem::init_filesystem();
  Serial.println(F("[FS] loadConfig()"));
  edugrid_filesystem::loadConfig();

  /* Network / Web server */
  Serial.println(F("[WIFI] initWiFi()"));
  edugrid_webserver::initWiFi();
  Serial.println(F("[WIFI] initWiFi() done"));

  /* PWM power stage */
  Serial.print  (F("[PWM] initPwmPowerConverter freq[Hz]="));
  Serial.println(CONVERTER_FREQUENCY);
  edugrid_pwm_control::initPwmPowerConverter(CONVERTER_FREQUENCY, PIN_POWER_CONVERTER_PWM);

  /* IR2104 gate driver enable */
  Serial.print  (F("[PWM] IR2104 SD pin="));
  Serial.print  (PIN_SD_ENABLE);
  Serial.println(F(" -> HIGH (enable)"));
  pinMode(PIN_SD_ENABLE, OUTPUT);
  digitalWrite(PIN_SD_ENABLE, HIGH);

  /* Measurements backend (INA228) */
  Serial.println(F("[MEAS] edugrid_measurement::init()"));
  edugrid_measurement::init();

  // Start in MANUAL mode with a low duty cycle for safety on boot.
  edugrid_mpp_algorithm::set_mode_state(MANUALLY);
  edugrid_pwm_control::setPWM(10); // Start at 10% duty so the converter is safe

  /****** END OF SETUP, START TASKS ******/
  Serial.println(F("[RTOS] starting tasks..."));

  // Task 2: Websocket/WiFi on core 0
  xTaskCreatePinnedToCore(coreTwo,   "coreTwo",   10000, nullptr, 1, &core2, 0);

  // Task 3: MPPT & sensors on core 1
  xTaskCreatePinnedToCore(coreThree, "coreThree", 10000, nullptr, 1, &core3, 1);

#ifdef OTA_UPDATES_ENABLE
  Serial.println(F("[OTA] OTA Updates are ENABLED"));
#endif

  vTaskDelay(10);
  Serial.print(F("| OK | EduGrid "));
  Serial.print(EDUGRID_VERSION);
  Serial.println(F(" running stable"));
  Serial.println(F("[HINT] If using AP mode: connect to the ESP32 Wi‑Fi and open http://192.168.1.1"));
}

/************************************************************************
 * loop(): display/logging tick
 ************************************************************************/
void loop()
{
  // Persist one line of CSV data to the log buffer each second.  The logging
  // module takes care of checking whether logging is active and when to flush
  // the buffered lines to flash.
  edugrid_logging::appendLog(
      edugrid_measurement::V_in,
      edugrid_measurement::V_out,
      edugrid_measurement::I_in,
      edugrid_measurement::I_out);

  delay(TASK_LOOP_INTERVAL_MS);
}
