/*************************************************************************
 * @file edugrid_filesystem.cpp
 * @date 2023/11/15
 *
 ************************************************************************/

/*************************************************************************
 * Include
 ************************************************************************/
#include <edugrid_filesystem.h>
#include <FS.h>
#include <LittleFS.h>

/*************************************************************************
 * Define
 ************************************************************************/

/*************************************************************************
 * Variable Definition
 ************************************************************************/
File edugrid_filesystem::open_file;
String edugrid_filesystem::file_content = "";
int edugrid_filesystem::state_filesystem = 99;
bool edugrid_filesystem::filesystem_mounted = false;
String edugrid_filesystem::config_wlan_ssid = "";
String edugrid_filesystem::config_wlan_pw = "";
String edugrid_filesystem::json_config_str = "";
String edugrid_filesystem::config_log_name = "";

/*************************************************************************
 * Function Definition
 ************************************************************************/

/** Start the littlefs filesystem on the esp32 --> call once
 */
int edugrid_filesystem::init_filesystem()
{
    // Mount LittleFS once during boot.  All subsequent file operations rely on
    // the `filesystem_mounted` flag so we never attempt to access storage that
    // failed to initialise.
    if (!LittleFS.begin())
    {
        state_filesystem = STATE_FILESYSTEM_ERROR;
        filesystem_mounted = false;
        Serial.println("|FAIL| Failed to mount filesystem");
    }
    else
    {
        filesystem_mounted = true;
        Serial.println("| OK | Filesystem mounted");
        state_filesystem = STATE_FILESYSTEM_OK;
    }

    // Serial.println(listFiles(true));
    return get_filesystem_state();
}

/** Returns state of filesystem
 * @return state true/false
 */
int edugrid_filesystem::get_filesystem_state()
{
    return state_filesystem;
}

/** Get the content of a file
 * --> Start path always with "/"
 * @param path Path to the file as Arduino String
 * @return Content as a String
 */
String edugrid_filesystem::getContent_str(String path)
{
    if (filesystem_mounted)
    {
        open_file = LittleFS.open(path, "r");
        if (!open_file)
        {
            Serial.print("|FAIL| File ");
            Serial.print(path);
            Serial.println(" failed to open");
        }

        // Read the entire file into a String so callers can parse the contents
        // without dealing with FS streams.
        file_content = open_file.readString();
        open_file.close();
        Serial.print("| OK | File ");
        Serial.print(path);
        Serial.println(" opened");

        return file_content;
    }
    else
    {
        return "";
    }
}

/** Get the content of a file
 * --> Start path always with "/"
 * @param path Path to the file as Arduino String
 * @return Content as a uint8_t (Content must be cast-able!)
 */
int edugrid_filesystem::getContent_int(String path)
{
    return getContent_str(path).toInt();
}

void edugrid_filesystem::loadConfig()
{
    // Configuration files are plain text; each helper returns an empty string
    // if the file does not exist so the Wi-Fi AP falls back to compiled
    // defaults.
    config_wlan_ssid = getContent_str(CONFIG_FILEPATH_SSID);
    config_wlan_pw = getContent_str(CONFIG_FILEPATH_PW);
    config_log_name = getContent_str(CONFIG_FILEPATH_LOGNAME);
}

/** Write an Arduino String to esp32 flash storage
 * @param path Absolut path in LittleFs (has to start with "/" !)
 * @param content Text to write 
 * @param appending activate appending mode
 */
void edugrid_filesystem::writeContent_str(String path, String content, bool appending)
{

    if (appending)
    {
        // Append mode is used for the CSV log.  We open the file in FILE_APPEND
        // so the new block is added to the end without overwriting previous
        // data.
        /* Appending */
        File file = LittleFS.open(path, FILE_APPEND);
        if (!file)
        {
            Serial.println("|FAIL| Failed to open file for appending");
            return;
        }
        if (file.print(content))
        {
            /*
            Serial.print("| OK | File ");
            Serial.print(path);
            Serial.print(" --> ");
            Serial.print(content);
            Serial.println(" appended");
            */
        }
        else
        {
            Serial.print("|FAIL| File ");
            Serial.print(path);
            Serial.println(" failed to append");
        }
        file.close();
    }
    else
    {
        /* Writting */
        File file = LittleFS.open(path, FILE_WRITE);
        if (!file)
        {
            Serial.println("|FAIL| Failed to open file for writing");
            return;
        }
        if (file.print(content))
        {
            Serial.print("| OK | File ");
            Serial.print(path);
            Serial.print(" --> ");
            Serial.print(content);
            Serial.println(" written");
        }
        else
        {
            Serial.print("|FAIL| File ");
            Serial.print(path);
            Serial.println(" failed to write");
        }
        file.close();
    }
}

/** Write wifi settings to esp32 Flash
 * @param ssid Name for WiFi AP
 * @param pw Password for WiFi AP
 */
void edugrid_filesystem::setWiFiCredentials(String ssid, String pw)
{
    // Update the cached values so the running access point reflects the new
    // credentials immediately and persist them so the next reboot uses them
    // automatically.
    config_wlan_ssid = ssid;
    config_wlan_pw = pw;
    writeContent_str(CONFIG_FILEPATH_SSID, config_wlan_ssid);
    writeContent_str(CONFIG_FILEPATH_PW, config_wlan_pw);
}

/** Get the content of a file
 * @param path Path to the file
 * @return Content as a char array
 */
// String edugrid_filesystem::getContent_char(String path)
// {
//     file_content = getContent_str(path);

//     return ;
// }