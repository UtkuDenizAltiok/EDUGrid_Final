/*************************************************************************
 * @file edugrid_webserver.h
 * @date 2023/11/13
 ************************************************************************/
#ifndef EDUGRID_WEBSERVER_H_
#define EDUGRID_WEBSERVER_H_

/*************************************************************************
 * Includes
 ************************************************************************/
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>

#include <edugrid_states.h>
#include <edugrid_pwm_control.h>
#include <edugrid_filesystem.h>
#include <edugrid_mpp_algorithm.h>
#include <edugrid_logging.h>

/*************************************************************************
 * Defines
 ************************************************************************/
#define DNS_DOMAIN "edugrid"   // optional mDNS domain, if you add mDNS

/* HTML IDs (must match your index.html) */
#define WEBSERVER_ID_REBOOT_REQUEST   ("id_reboot_request")
#define WEBSERVER_ID_MPP_SWITCH       ("1")
#define WEBSERVER_ID_PWM_INCREMENT    ("2")
#define WEBSERVER_ID_PWM_DECREMENT    ("3")
#define WEBSERVER_ID_PWM_SLIDER       ("4")
#define WEBSERVER_ID_MODE_LABEL       ("mode_label")
#define WEBSERVER_ID_LOGGING_LABEL    ("logging_label")
#define WEBSERVER_ID_PWM_FREQ_LABEL   ("freq_label")

/* Filesystem paths */
#define WEBSERVER_HOME_PATH   ("/www/index.html")
#define WEBSERVER_STYLE_PATH  ("/www/style.css")
#define WEBSERVER_JS_PATH     ("/www/script.js")
#define WEBSERVER_FILE_PATH   ("/www/file.html")
#define WEBSERVER_ADMIN_PATH  ("/www/admin.html")

/*************************************************************************
 * Class
 ************************************************************************/
class edugrid_webserver
{
public:
    static void initWiFi(void);
    static void webSocketLoop(void);
    static String humanReadableSize(const size_t bytes);

private:
    static String processor(const String &var);
    static void handleUpload(AsyncWebServerRequest* request, String filename,
                         size_t index, uint8_t* data, size_t len, bool final);
    static String listFiles(bool ishtml);
    static String _id;
    static String _state;
};

#endif /* EDUGRID_WEBSERVER_H_ */
