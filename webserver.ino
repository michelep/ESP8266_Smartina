// ESP8266 Smartina
//
// Written by Michele <o-zone@zerozone.it> Pinassi
// Released under GPLv3 - No any warranty
//

String templateProcessor(const String& var)
{
  //
  // System values
  //
  if(var == "hostname") {
    return String(config.hostname);
  }
  if(var == "fw_name") {
    return String(FW_NAME);
  }
  if(var=="fw_version") {
    return String(FW_VERSION);
  }
  if(var=="uptime") {
    return String(millis()/1000);
  }
  if(var=="timedate") {
    return String(timeClient.getFormattedTime());
  }
  //
  // Config values
  //
  if(var=="wifi_ssid") {
    return String(config.wifi_ssid);
  }
  if(var=="wifi_password") {
    return String(config.wifi_password);
  }
  if(var=="wifi_rssi") {
    return String(WiFi.RSSI());
  }
  if(var=="broker_host") {
    return String(config.broker_host);
  }
  if(var=="broker_port") {
    return String(config.broker_port);
  }  
  if(var=="broker_username") {
    return String(config.broker_username);
  }
  if(var=="broker_password") {
    return String(config.broker_password);
  }  
  if(var=="broker_tout") {
    return String(config.broker_tout);
  }  
  if(var=="client_id") {
    return String(config.client_id);
  }
  return String();
}

// ************************************
// initWebServer
//
// initialize web server
// ************************************
void initWebServer() {
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setTemplateProcessor(templateProcessor);

  server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request){
    String message;
    if(request->hasParam("wifi_ssid", true)) {
        strcpy(config.wifi_ssid,request->getParam("wifi_ssid", true)->value().c_str());
    }
    if(request->hasParam("wifi_password", true)) {
        strcpy(config.wifi_password,request->getParam("wifi_password", true)->value().c_str());
    }
    if(request->hasParam("broker_host", true)) {
        strcpy(config.broker_host,request->getParam("broker_host", true)->value().c_str());
    }
    if(request->hasParam("broker_port", true)) {
        config.broker_port = atoi(request->getParam("broker_port", true)->value().c_str());
    }
    if(request->hasParam("broker_username", true)) {
        strcpy(config.broker_username,request->getParam("broker_username", true)->value().c_str());
    }
    if(request->hasParam("broker_password", true)) {
        strcpy(config.broker_password,request->getParam("broker_password", true)->value().c_str());
    }
    if(request->hasParam("broker_tout", true)) {
        config.broker_tout = atoi(request->getParam("broker_tout", true)->value().c_str());
    }
    if(request->hasParam("client_id", true)) {
        strcpy(config.client_id, request->getParam("client_id", true)->value().c_str());
    }    
    //
    saveConfigFile();
    request->redirect("/?result=ok");
  });

  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    ESP.restart();
  });
  
  server.on("/ajax", HTTP_POST, [] (AsyncWebServerRequest *request) {
    String action,value,response="";
    char outputJson[256];

    if (request->hasParam("action", true)) {
      action = request->getParam("action", true)->value();
//      DEBUG("[DEBUG] ACTION: "+String(action));
      // ENVironment
      if(action.equals("env")) {
        serializeJson(env,outputJson);
        response = String(outputJson);
      }
    }
    request->send(200, "text/plain", response);
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    Serial.printf("NOT_FOUND: ");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());
    request->send(404);
  });

  server.begin();
}
