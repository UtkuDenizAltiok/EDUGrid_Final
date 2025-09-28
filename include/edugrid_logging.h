/*************************************************************************
 * @file edugrid_logging.h
 * @date 2024-01-09
 *
 ************************************************************************/

#ifndef EDUGRID_LOGGING_H_
#define EDUGRID_LOGGING_H_

/*************************************************************************
 * Include
 ************************************************************************/
#include <Arduino.h>
#include <edugrid_states.h>
#include <edugrid_filesystem.h>

/*************************************************************************
 * Define
 ************************************************************************/
#define EDUGRID_LOGGING_ACTIVE (true)
#define EDUGRID_LOGGING_CSV_DELIMITER (";")
#define EDUGRID_LOGGING_MAX_MESSAGES_IN_BUFFER (100) // write every 100 messages to flash
#define EDUGRID_LOGGING_MAX_TIME_MS (15 * 60 * 1000) // 15 min = 900 s = 900,000 ms
/* Log size for 15 min = 18.57 KB
 */

/*************************************************************************
 * Class
 ************************************************************************/
class edugrid_logging
{
public:
    // Log control entry points used by the web UI and by setup().
    static bool getLogState();
    static String getLogState_str();
    static void activateLogging();
    static void deactivateLogging();
    static void toggleLogging();

    // Append a single CSV row (Vin, Vout, Iin, Iout).  The helper takes care of
    // buffering and flashing the data in blocks so the main loop only needs to
    // call it once per second.
    static void appendLog(float vin, float vout, float iin, float iout);

private:
    static bool log_active;
    static bool safe_request;
    static String log_message_buffer;
    static uint8_t log_message_counter;
    static unsigned long all_messages;
    static unsigned long log_start_time;

protected:
};

#endif /* EDUGRID_LOGGING_H_ */