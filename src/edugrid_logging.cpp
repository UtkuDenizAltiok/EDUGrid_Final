/*************************************************************************
 * @file edugrid_logging.cpp
 * @date 2024/01/09
 *
 ************************************************************************/

/*************************************************************************
 * Include
 ************************************************************************/
#include <edugrid_logging.h>

/*************************************************************************
 * Define
 ************************************************************************/

/*************************************************************************
 * Variable Definition
 ************************************************************************/
bool edugrid_logging::log_active = false;
bool edugrid_logging::safe_request = false;
String edugrid_logging::log_message_buffer = "";
uint8_t edugrid_logging::log_message_counter = 0;
unsigned long edugrid_logging::log_start_time = 0;
unsigned long edugrid_logging::all_messages = 0;

/*************************************************************************
 * Function Definition
 ************************************************************************/

bool edugrid_logging::getLogState()
{
  return log_active;
}

String edugrid_logging::getLogState_str()
{
  if (getLogState() == EDUGRID_LOGGING_ACTIVE)
  {
    return "ON";
  }
  else
  {
    return "OFF";
  }
}

void edugrid_logging::activateLogging()
{
  log_active = true;
  log_start_time = millis();
  log_message_buffer = "";
  log_message_buffer.reserve(EDUGRID_LOGGING_MAX_MESSAGES_IN_BUFFER * 48);
  /* Clear log file content */
  edugrid_filesystem::writeContent_str(edugrid_filesystem::config_log_name, "");
  Serial.print("| OK | Logging  ");
  Serial.println(getLogState_str());
  Serial.print("| OK | Logging start time: ");
  Serial.println(log_start_time);
}

void edugrid_logging::deactivateLogging()
{
  log_active = false;
  safe_request = true;
  log_start_time = 0;
  Serial.print("| OK | Logging ");
  Serial.println(getLogState_str());
  Serial.print("| OK | Logging end time: ");
  Serial.println(millis());

  /* Testing */
  // Serial.println(edugrid_filesystem::getContent_str(edugrid_filesystem::config_log_name));
}

void edugrid_logging::toggleLogging()
{
  if (getLogState() == EDUGRID_LOGGING_ACTIVE)
  {
    deactivateLogging();
  }
  else
  {
    activateLogging();
  }
}

void edugrid_logging::appendLog(float vin, float vout, float iin, float iout)
{
  /* Log only, if activated */
  if (getLogState() == EDUGRID_LOGGING_ACTIVE)
  {
    log_message_counter += 1;
    all_messages += 1;
    String line;
    line.reserve(64);
    line += all_messages;
    line += EDUGRID_LOGGING_CSV_DELIMITER;
    line += String(vin, 3);
    line += EDUGRID_LOGGING_CSV_DELIMITER;
    line += String(vout, 3);
    line += EDUGRID_LOGGING_CSV_DELIMITER;
    line += String(iin, 3);
    line += EDUGRID_LOGGING_CSV_DELIMITER;
    line += String(iout, 3);
    line += '\n';

    /* add new log line to buffer String */
    log_message_buffer += line;

    /* Check for logging timeout*/
    if ((millis() - log_start_time) >= EDUGRID_LOGGING_MAX_TIME_MS)
    {
      deactivateLogging();
    }
  }

  /* Limit in buffer reached --> write to flash
   * Only reached, if logging is ON
   */
  if (log_message_counter >= EDUGRID_LOGGING_MAX_MESSAGES_IN_BUFFER)
  {
    edugrid_filesystem::writeContent_str(edugrid_filesystem::config_log_name, log_message_buffer, true);
    log_message_buffer = "";
    log_message_counter = 0;
    Serial.println("| OK | Logging block safed to flash");
  }

  if (safe_request)
  {
    safe_request = false;
    /* Always append last buffer iteration --> avoiding loss of logging data! */
    edugrid_filesystem::writeContent_str(edugrid_filesystem::config_log_name, log_message_buffer, true);
    /* Reset everything */
    log_message_buffer = "";
    log_message_counter = 0;
    all_messages = 0;
    Serial.println("| OK | Logging finished");
  }
}