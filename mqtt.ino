// ESP8266 Smartina
//
// Written by Michele <o-zone@zerozone.it> Pinassi
// Released under GPLv3 - No any warranty
//

bool mqttConnect() {
  uint8_t timeout=10;
  if(strlen(config.broker_host) > 0) {
    DEBUG("[MQTT] Attempting connection to "+String(config.broker_host)+":"+String(config.broker_port));
    while((!client.connected())&&(timeout > 0)) {
      // Attempt to connect
      if (client.connect(config.client_id)) {
        // Once connected, publish an announcement...
        DEBUG("[MQTT] Connected as "+String(config.client_id));
      } else {
        timeout--;
        delay(500);
      }
    }
    if(!client.connected()) {
      DEBUG("[MQTT] Connection failed");    
      return false;
    }
    // subscribe MQTT topics
    client.subscribe("smartina/set_mode");
    return true;
  } else {
    return false;
  }
}

void mqttReceiver(char* _topic, byte* payload, unsigned int length) {
  String topic = String(_topic);
  payload[length] = '\0';
  DEBUG("[MQTT] Received "+topic);
  if(topic.equals("smartina/set_mode")) {
 
  }
}

bool mqttPublish(char *topic, char *payload) {
  if(mqttConnect()) {
    DEBUG("[MQTT] Publish "+String(topic)+":"+String(payload));
    if(client.publish(topic,payload)) {
      DEBUG("[MQTT] Published");
      return true;
    } else {
      DEBUG("[MQTT] Publish FAILED");
      return false;
    }
  }
  return false;
}

void mqttCallback() {
  char topic[32];
  DEBUG("[DEBUG] MQTT callback");
  // First check for connection
  if(WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }
  // MQTT not connected? Connect or return false;
  if (!client.connected()) {
    if(!mqttConnect()) {
      // MQTT connection failed or disabled
      return;
    }
  }
}
