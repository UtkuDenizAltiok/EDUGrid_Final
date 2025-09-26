/*************************************************************************
 * @file edugrid_filesystem.h
 * @date 2023/11/15
 *
 ************************************************************************/

#ifndef EDUGRID_FILE_SYSTEM_H_
#define EDUGRID_FILE_SYSTEM_H_

/*************************************************************************
 * Include
 ************************************************************************/
#include <Arduino.h>
#include <LittleFS.h>
#include <edugrid_states.h>

/*************************************************************************
 * Define
 ************************************************************************/

/* Paths according to the LittleFS filesystem */
#define CONFIG_FILEPATH_SSID                        ("/config/ssid.config")
#define CONFIG_FILEPATH_PW                          ("/config/password.config")
#define CONFIG_FILEPATH_LOGNAME                     ("/config/logname.config")
#define CONFIG_FILEPATH_TRIAC                       ("/config/triac.config")
#define CONFIG_FILEPATH_TRIAC_PIN_ZEROCROSSING      ("/config/pin_zerocrossing.config")
#define CONFIG_FILEPATH_TRIAC_PIN_CONTROL           ("/config/pin_triaccontrol.config")


/*************************************************************************
 * Class
 ************************************************************************/

/** 
 * Class with static members for all filesystem tasks
*/
class edugrid_filesystem
{
private:
public:
    static int init_filesystem();
    static int get_filesystem_state();
    static String getContent_str(String path);
    static int getContent_int(String path);
    static void loadConfig();
    static void writeContent_str(String path, String content, bool appending=false);
    static void setWiFiCredentials(String ssid, String pw);
    static String config_wlan_ssid;
    static String config_wlan_pw;
    static String config_log_name;
    static String config_triac_state;

protected:
    static File open_file;
    static String file_content;
    static int state_filesystem;
    static bool filesystem_mounted;
    static String json_config_str;
};

#endif /* EDUGRID_FILE_SYSTEM_H_ */