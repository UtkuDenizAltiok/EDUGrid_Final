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
String edugrid_logging::log_line = "";
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

void edugrid_logging::appendLog(String field0, String field1, String field2, String field3)
{
  /* Log only, if activated */
  if (getLogState() == EDUGRID_LOGGING_ACTIVE)
  {
    log_message_counter += 1;
    all_messages += 1;
    log_line =
        (String)all_messages + EDUGRID_LOGGING_CSV_DELIMITER +
        field0 + EDUGRID_LOGGING_CSV_DELIMITER +
        field1 + EDUGRID_LOGGING_CSV_DELIMITER +
        field2 + EDUGRID_LOGGING_CSV_DELIMITER +
        field3 + "\n";

    /* add new log line to buffer String */
    log_message_buffer += log_line;

    /* Clear log_line */
    log_line = "";

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