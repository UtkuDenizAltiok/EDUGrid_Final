/*************************************************************************
 * @file edugrid_webserver.cpp
 * @date 2023/11/13
 ************************************************************************/

/*************************************************************************
 * Includes
 ************************************************************************/
#include <LittleFS.h>
#include <edugrid_webserver.h>
#include "edugrid_mpp_algorithm.h"
#include "edugrid_measurement.h"

/*************************************************************************
 * Statics
 ************************************************************************/
String edugrid_webserver::_id = "";
String edugrid_webserver::_state = "";
String edugrid_webserver::_state2 = "";
String edugrid_webserver::JSON_str = "";
String edugrid_webserver::wifi_name = "";
String edugrid_webserver::wifi_password = "";
StaticJsonDocument<1024> edugrid_webserver::doc;

/* Web servers */
WebSocketsServer webSocket(81);   // matches script.js ws://<host>:81/
AsyncWebServer   server(80);

/* Query parameter keys */
static const char *PARAM_INPUT_1 = "ID";
static const char *PARAM_INPUT_2 = "STATE";
static const char *PARAM_INPUT_3 = "STATE2";

/*************************************************************************
 * Helpers
 ************************************************************************/
String edugrid_webserver::processor(const String &var)
{
  if (var == "BUTTONPLACEHOLDER") {
    return (String)EDUGRID_VERSION;
  }
  if (var == "FILELIST") {
    return listFiles(true);
  }
  if (var == "FREESPIFFS") {
    return humanReadableSize((LittleFS.totalBytes() - LittleFS.usedBytes()));
  }
  if (var == "USEDSPIFFS") {
    return humanReadableSize(LittleFS.usedBytes());
  }
  if (var == "TOTALSPIFFS") {
    return humanReadableSize(LittleFS.totalBytes());
  }
  return String();
}

/*************************************************************************
 * WiFi + HTTP + WS init
 ************************************************************************/
void edugrid_webserver::initWiFi(void)
{
  /* Access-Point mode (SSID/pw from filesystem config) */
  WiFi.softAP(edugrid_filesystem::config_wlan_ssid.c_str(),
              edugrid_filesystem::config_wlan_pw.c_str());

  /* AP network config */
  IPAddress local_ip(192, 168, 1, 1);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);

  /* WebSocket server (port 81) */
  webSocket.begin();

  /* Pre-reserve JSON string buffer */
  JSON_str.reserve(512);

  /* HTTP routes */
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, WEBSERVER_HOME_PATH, String(), false, processor);
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, WEBSERVER_STYLE_PATH, "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, WEBSERVER_JS_PATH, "application/javascript");
  });

  /* Serve Chart.js locally for offline use */
  server.on("/chart.umd.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/www/chart.umd.js", "application/javascript");
  });

  server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, WEBSERVER_ADMIN_PATH, String(), false, processor);
  });

  server.on("/file", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, WEBSERVER_FILE_PATH, String(), false, processor);
  });

    /* --- IV SWEEP API --- */
  server.on("/ivsweep/start", HTTP_GET, [](AsyncWebServerRequest *request){
    edugrid_mpp_algorithm::request_iv_sweep();
    request->send(200, "application/json", "{\"status\":\"started\"}");
  });

  // === MODIFIED: Replaced String concatenation with ArduinoJson for performance ===
  server.on("/ivsweep/data", HTTP_GET, [](AsyncWebServerRequest *request){
    const uint16_t n = edugrid_mpp_algorithm::iv_point_count();
    
    // Create a JSON document. Adjust size if you have more than ~100 points.
    DynamicJsonDocument doc(4096);

    // Create the arrays in the JSON object
    JsonArray v_data = doc.createNestedArray("v");
    JsonArray i_data = doc.createNestedArray("i");
    JsonArray p_data = doc.createNestedArray("p");

    // Populate the arrays with data from the algorithm module
    for (uint16_t i = 0; i < n; ++i) {
      float v, cur;
      edugrid_mpp_algorithm::iv_get_point(i, v, cur);
      v_data.add(round(v * 1000.0) / 1000.0);       // 3 decimal places
      i_data.add(round(cur * 1000.0) / 1000.0);     // 3 decimal places
      p_data.add(round(v * cur * 1000.0) / 1000.0); // 3 decimal places
    }
    
    // Add the status flags
    doc["in_progress"] = edugrid_mpp_algorithm::iv_sweep_in_progress();
    doc["done"] = edugrid_mpp_algorithm::iv_sweep_done();
    
    // Serialize the JSON to a String and send it
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  /* File actions (download/delete) */
  server.on("/filehandle", HTTP_GET, [](AsyncWebServerRequest *request){
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

    if (request->hasParam("name") && request->hasParam("action")) {
      const char* fileName   = request->getParam("name")->value().c_str();
      const char* fileAction = request->getParam("action")->value().c_str();

      if (!LittleFS.exists(fileName)) {
        Serial.println(logmessage + " ERROR: file does not exist");
        request->send(400, "text/plain", "ERROR: file does not exist");
      } else {
        if (strcmp(fileAction, "download") == 0) {
          request->send(LittleFS, fileName, "application/octet-stream");
        } else if (strcmp(fileAction, "delete") == 0) {
          LittleFS.remove(fileName);
          request->send(200, "text/plain", "Deleted File: " + String(fileName));
        } else {
          request->send(400, "text/plain", "ERROR: invalid action param supplied");
        }
      }
    } else {
      request->send(400, "text/plain", "ERROR: name and action params required");
    }
  });

  server.on("/listfiles", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", listFiles(true));
  });

  /* File upload into LittleFS */
  server.on("/upload", HTTP_POST,
            [](AsyncWebServerRequest *request){ request->send(200); },
            handleUpload);

  /* Control endpoint */
  server.on("/updatevalues", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam(PARAM_INPUT_1)) {
      _id    = request->getParam(PARAM_INPUT_1)->value();
      _state = request->getParam(PARAM_INPUT_2)->value();
      Serial.print("[UI] ID="); Serial.print(_id);
      Serial.print(" STATE=");  Serial.println(_state);

      if (_id.equals(WEBSERVER_ID_MPP_SWITCH)) {
        edugrid_mpp_algorithm::toggle_mode_state();
      } else if (_id.equals(WEBSERVER_ID_PWM_INCREMENT)) {
        edugrid_pwm_control::pwmIncrementDecrement(5);
      } else if (_id.equals(WEBSERVER_ID_PWM_DECREMENT)) {
        edugrid_pwm_control::pwmIncrementDecrement(-5);
      } else if (_id.equals(WEBSERVER_ID_PWM_SLIDER)) {
          edugrid_pwm_control::requestManualTarget(
            (uint8_t)_state.toInt());
      } else if (_id.equals(WEBSERVER_ID_MODE_LABEL)) {
        // Client must send the desired state: "AUTO" or "MANUAL".
        // We do not toggle here. We never enter IV_SWEEP from MODE clicks.
        String s = _state;
        s.trim();
        s.toUpperCase();

        if (s == "AUTO" || s == "1") {
          edugrid_mpp_algorithm::set_mode_state(AUTO);
        } else if (s == "MANUAL" || s == "MANUALLY" || s == "0") {
          edugrid_mpp_algorithm::set_mode_state(MANUALLY);
        } else {
          // Unknown request -> do nothing (keeps current mode)
        }
      } else if (_id.equals(WEBSERVER_ID_LOGGING_LABEL)) {
        edugrid_logging::toggleLogging();
      } else if (_id.equals(WEBSERVER_ID_PWM_FREQ_LABEL)) {
        // No-op label
      } else if (_id.equals(WEBSERVER_ID_REBOOT_REQUEST)) {
        ESP.restart();
      }
    }
    request->send(200, "text/plain", "OK");
  });

  // === Zero-offset calibration endpoint ===
  // Hit: GET /calibrate_zero
  server.on("/calibrate_zero", HTTP_GET, [](AsyncWebServerRequest *req){
    // Tip: For best results, run this when PV is disconnected and no load is attached.
    edugrid_measurement::calibrateZeroOffsets(400);  // ~0.8s total
    req->send(200, "text/plain", "OK");
  });

#ifdef OTA_UPDATES_ENABLE
  ElegantOTA.begin(&server);
#endif

  // Live numbers for the UI (simple polling API)
  // MODIFIED: Replaced String concatenation with ArduinoJson for performance and reliability.
  server.on("/api/now", HTTP_GET, [](AsyncWebServerRequest* req){
    // Create a small, temporary JSON document on the stack.
    // This is much more efficient than using the global `doc`.
    StaticJsonDocument<256> json_doc;

    // Populate the JSON object with sensor data, rounding to 3 decimal places.
    json_doc["vin"]  = edugrid_measurement::V_in;
    json_doc["iin"]  = edugrid_measurement::I_in;
    json_doc["vout"] = edugrid_measurement::V_out;
    json_doc["iout"] = edugrid_measurement::I_out;
    json_doc["pin"]  = edugrid_measurement::P_in;
    json_doc["pout"] = edugrid_measurement::P_out;
    // For efficiency, round to one decimal place for the UI.
    json_doc["eff"]  = round(edugrid_measurement::eff * 1000.0f) / 10.0f;

    // Serialize the JSON object into a String to be sent.
    String out;
    serializeJson(json_doc, out);
    req->send(200, "application/json", out);
  });

  server.begin();

  Serial.print("|WiFi| EduGrid Webserver started at ");
  Serial.println(WiFi.softAPIP());
  Serial.print("       Name: ");     Serial.println(edugrid_filesystem::config_wlan_ssid);
  Serial.print("       Password: "); Serial.println(edugrid_filesystem::config_wlan_pw);
}

/*************************************************************************
 * WS loop: publish JSON periodically (called from Task/coreTwo)
 ************************************************************************/
void edugrid_webserver::webSocketLoop(void)
{
  webSocket.loop();

  static uint32_t lastPush = 0;
  const uint32_t PUSH_EVERY_MS = 100; // 10 Hz is plenty

  uint32_t now = millis();
  if (now - lastPush < PUSH_EVERY_MS) return;
  lastPush = now;

  /* PWM / converter */
  doc["pwm"]      = (String)edugrid_pwm_control::getPWM() + " %";
  doc["pwm_raw"]  = edugrid_pwm_control::getPWM();
  doc["pwm_min"]  = edugrid_pwm_control::getPwmLowerLimit();
  doc["pwm_max"]  = edugrid_pwm_control::getPwmUpperLimit();
  doc["freq"]     = (String)edugrid_pwm_control::getFrequency_kHz() + " kHz";
  switch (edugrid_mpp_algorithm::get_mode_state()) { // MANUALLY/AUTO/IV_SWEEP
  case MANUALLY: doc["mode"] = "MANUAL";   break;
  case AUTO:     doc["mode"] = "AUTO";     break;
  case IV_SWEEP: doc["mode"] = "IV_SWEEP"; break;
  default:       doc["mode"] = "UNKNOWN";  break;
}

  /* Measurements (numeric; format in JS) */
  doc["vin"]  = edugrid_measurement::V_in;
  doc["iin"]  = edugrid_measurement::I_in;
  doc["vout"] = edugrid_measurement::V_out;
  doc["iout"] = edugrid_measurement::I_out;
  doc["pin"]  = edugrid_measurement::P_in;
  doc["pout"] = edugrid_measurement::P_out;
  doc["eff"]  = edugrid_measurement::eff; // 0..1; multiply by 100 in JS

  /* Misc */
  doc["logging"]    = edugrid_logging::getLogState_str();

  JSON_str = "";
  serializeJson(doc, JSON_str);
  webSocket.broadcastTXT(JSON_str);
  doc.clear();
}

/*************************************************************************
 * File upload handler
 ************************************************************************/
void edugrid_webserver::handleUpload(AsyncWebServerRequest *request, String filename,
                                     size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index) {
    request->_tempFile = LittleFS.open("/" + filename, "w");
  }
  if (len) {
    request->_tempFile.write(data, len);
  }
  if (final) {
    request->_tempFile.close();
    request->redirect("/file");
  }
}

/*************************************************************************
 * List LittleFS files (www, log, config)
 ************************************************************************/
String edugrid_webserver::listFiles(bool ishtml)
{
  String returnText;

  auto listDir = [&](const char* path) {
    File root = LittleFS.open(path);
    if (!root) return;
    File f = root.openNextFile();
    while (f) {
      returnText += "<tr align='left'><td>" + String(path) + String(f.name()) + "</td><td>"
                    + edugrid_webserver::humanReadableSize(f.size()) + "</td>";
      returnText += "<td><button onclick=\"downloadDeleteButton('"
                    + String(path) + String(f.name())
                    + "', 'download')\">Download</button>";
      returnText += "<td><button onclick=\"downloadDeleteButton('"
                    + String(path) + String(f.name())
                    + "', 'delete')\">Delete</button></tr>";
      f = root.openNextFile();
    }
    root.close();
  };

  returnText += "<table><tr><th align='left'>Name</th><th align='left'>Size</th><th></th><th></th></tr>";
  listDir("/log/");
  listDir("/www/");
  listDir("/config/");
  listDir("/");
  returnText += "</table>";
  return returnText;
}

/*************************************************************************
 * Human readable size helper
 ************************************************************************/
String edugrid_webserver::humanReadableSize(const size_t bytes)
{
  if (bytes < 1024) return String(bytes) + " B";
  if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " KB";
  if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0) + " MB";
  return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}
