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
    static bool getLogState();
    static String getLogState_str();
    static void activateLogging();
    static void deactivateLogging();
    static void toggleLogging();
    static void appendLog(String field0, String field1, String field2, String field3);

private:
    static bool log_active;
    static bool safe_request;
    static String log_line;
    static String log_message_buffer;
    static uint8_t log_message_counter;
    static unsigned long all_messages;
    static unsigned long log_start_time;

protected:
};

#endif /* EDUGRID_LOGGING_H_ */