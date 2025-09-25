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
#define BAUD_RATE (115200)
#define CYCLE_TIME_TASK_LOOP_MS      (1000)   // [ms]  Display & Logging
#define CYCLE_TIME_TASK_WEBSOCKET_MS (100)    // [ms]  WebSocket tick
#define CYCLE_TIME_TASK_CONTROL_MS   (20)     // [ms]  How often the control task runs (50 Hz)

/************************************************************************
 * RTOS Task Handles
 ************************************************************************/
TaskHandle_t core2; // WebSocket & WiFi
TaskHandle_t core3; // MPPT Algorithm and Sensors

/************************************************************************
 * Task 2: WebSocket pump
 ************************************************************************/
void coreTwo(void *pvParameters)
{
  (void)pvParameters;
  for (;;)
  {
    edugrid_webserver::webSocketLoop();
    vTaskDelay(pdMS_TO_TICKS(CYCLE_TIME_TASK_WEBSOCKET_MS));
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
    edugrid_measurement::getSensors();

    /* 2) Keep duty within safe/allowed borders (this is good practice) */
    edugrid_pwm_control::checkAndSetPwmBorders();

    /* 3) Execute the logic for the current operating mode */
    switch (edugrid_mpp_algorithm::get_mode_state())
    {
      case MANUALLY:
        // In Manual mode, the UI sets the PWM directly. Nothing to do here.
        break;

      case AUTO:
        // The find_mpp() function has its own internal timer.
        // We call it every loop, and it will only act when it's time.
        edugrid_mpp_algorithm::find_mpp();
        break;

      case IV_SWEEP:
        // The iv_sweep_step() function acts as a state machine.
        // Calling it every loop tick drives the sweep forward one step at a time.
        edugrid_mpp_algorithm::iv_sweep_step();
        break;
    }

#ifdef EDUGRID_TELEMETRY_ON
    edugrid_telemetry::telemetryPrint();
#endif

    /* 4) Loop timing */
    // Use a single, consistent delay for the whole task.
    // This makes the loop predictable and responsive.
    vTaskDelay(pdMS_TO_TICKS(CYCLE_TIME_TASK_CONTROL_MS));
  }
}


/************************************************************************
 * setup()
 ************************************************************************/
void setup()
{
  /* Serial — start this FIRST so we see all boot logs */
  Serial.begin(BAUD_RATE);
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
  edugrid_pwm_control::setPWM(10, /*auto_mode=*/false); // Start at 10% duty

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
  edugrid_logging::appendLog(
      String(edugrid_measurement::V_in),
      String(edugrid_measurement::V_out),
      String(edugrid_measurement::I_in),
      String(edugrid_measurement::I_out));

  delay(CYCLE_TIME_TASK_LOOP_MS);
}