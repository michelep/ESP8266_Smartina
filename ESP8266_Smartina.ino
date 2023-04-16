// ESP8266 Smartina
//
// Written by Michele <o-zone@zerozone.it> Pinassi
// Released under GPLv3 - No any warranty
//
// LOLIN WeMos D1 mini
// Flash size: 4MB with 2MB OTA
//
//
// 0.1.0 - 16.04.2023 First release
// 
#define __DEBUG__

// Firmware data
const char BUILD[] = __DATE__ " " __TIME__;
#define FW_NAME         "smartina"
#define FW_VERSION      "0.1.0"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ArduinoJson
// https://arduinojson.org/
#include <ArduinoJson.h>

// MQTT PubSub client
// https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// NTP ClientLib 
// https://github.com/taranais/NTPClient
#include <NTPClient.h>
#include <WiFiUdp.h>

#define NTPSERVER "time.ien.it"
#define NTPTZ 3600

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,NTPSERVER,NTPTZ);

// Web server
AsyncWebServer server(80);

// FastLED
// https://github.com/FastLED
#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
FASTLED_USING_NAMESPACE

#define LED_PIN             D5
#define NUM_LEDS            12
#define MAX_POWER_MILLIAMPS 500
#define LED_TYPE            WS2812B
#define COLOR_ORDER         GRB     

CRGB leds[NUM_LEDS];


const int TOUCH_PIN = D1;

// Config
struct _Config {
  // WiFi config
  char wifi_ssid[16];
  char wifi_password[16];
  //
  char hostname[32];
  // MQTT config
  char broker_host[32];
  unsigned int broker_port;
  char broker_username[16];
  char broker_password[16];
  unsigned int broker_tout;
  char client_id[18];
};

#define CONFIG_FILE "/config.json"

File configFile;
_Config config; // Global config object

// Format SPIFFS if mount failed
#define FORMAT_SPIFFS_IF_FAILED 1

// JSON
#include <ArduinoJson.h>
DynamicJsonDocument env(256);

unsigned long last;
bool isOnTouch=false, isTouchReleased=false;
unsigned long touchTimer=0;

// LED mode

enum {
  LED_OFF, // Default
  LED_SINGLE,
  LED_PACIFICA,
};

unsigned int ledMode=LED_OFF, ledBrightness=250;

// ************************************
// DEBUG(message)
//
// ************************************
void DEBUG(String message, bool cr=true) {
#ifdef __DEBUG__
  if(cr) {
    String debug = "["+String(millis()/10)+"]"+message;
    Serial.println(debug);    
  } else {
    Serial.print(message);        
  }
#endif
}

// ************************************
// connectToWifi()
//
// connect to configured WiFi network
// ************************************
bool connectToWifi() {
  uint8_t timeout=0;

  if(strlen(config.wifi_ssid) > 0) {
    DEBUG("Connecting to "+String(config.wifi_ssid),false);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifi_ssid, config.wifi_password);

    while((WiFi.status() != WL_CONNECTED)&&(timeout < 10)) {
      delay(500);
      DEBUG(".",false);
      timeout++;
    }
    if(WiFi.status() == WL_CONNECTED) {
      DEBUG("OK! My IP is "+WiFi.localIP().toString());

      if (MDNS.begin(config.hostname)) {
        DEBUG("MDNS responder started");
        // Add service to MDNS-SD
        MDNS.addService("http", "tcp", 80);
      }

      // NTP    
      timeClient.begin();
      
      return true;  
    } else {
      DEBUG("FAILED!");
      return false;
    }
  } else {
    DEBUG(F("Wifi configuration missing"));
    return false; 
  }
}

// ************************************
// onTouch ISR routine
// triggered ONCHANGE, so first trigger is on touch, the second on release.
//
// ************************************
ICACHE_RAM_ATTR void onTouch() {
  DEBUG(F("Touch ISR"));
  if(!isOnTouch) {
    isOnTouch = true;
    isTouchReleased = false;
    touchTimer = 0;
  } else {
    isOnTouch = false;  
    isTouchReleased = true;
  }
}

// ************************************
// SETUP
//
// ************************************
void setup() {
  Serial.begin(115200);
  delay(100);
    
  // print firmware and build data
  Serial.println();
  Serial.println();
  Serial.print(FW_NAME);  
  Serial.print(" ");
  Serial.print(FW_VERSION);
  Serial.print(" ");  
  Serial.println(BUILD);

  // Initialize LEDs
  FastLED.addLeds<LED_TYPE,LED_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);
  FastLED.setBrightness(100);

  leds[0] = CRGB::Red;
  FastLED.show();
  delay(500);
  leds[0] = CRGB::Black;
  FastLED.show();

  // Initialize SPIFFS
  if(!SPIFFS.begin()){
    DEBUG(F("SPIFFS Mount Failed. Try formatting..."));
    if(SPIFFS.format()) {
      DEBUG(F("SPIFFS initialized successfully"));
    } else {
      DEBUG(F("SPIFFS error"));
      ESP.restart();
    }
  } else {
    DEBUG(F("SPIFFS done"));
  }

  // Load config file from SPIFFS
  loadConfigFile();

  // Connect to WiFi network
  connectToWifi();

  // Initialize web server on port 80
  initWebServer();

  env["status"] = "Booting...";

  // Setup OTA
  ArduinoOTA.onStart([]() { 
    Serial.println("[OTA] Update Start");
  });
  ArduinoOTA.onEnd([]() { 
    Serial.println("[OTA] Update End"); 
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "[OTA] Progress: %u%%\n", (progress/(total/100)));
    Serial.println(p);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if(error == OTA_AUTH_ERROR) Serial.println("[OTA] Auth Failed");
    else if(error == OTA_BEGIN_ERROR) Serial.println("[OTA] Begin Failed");
    else if(error == OTA_CONNECT_ERROR) Serial.println("[OTA] Connect Failed");
    else if(error == OTA_RECEIVE_ERROR) Serial.println("[OTA] Recieve Failed");
    else if(error == OTA_END_ERROR) Serial.println("[OTA] End Failed");
  });
  ArduinoOTA.setHostname(config.hostname);
  ArduinoOTA.begin();

  // Setup MQTT connection
  client.setServer(config.broker_host, config.broker_port);
  client.setCallback(mqttReceiver); 

  // Touch interrupt pin
  pinMode(TOUCH_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), onTouch, CHANGE);

  // Go!
  env["status"] = "Run";
  DEBUG("Run!");
}

unsigned int ledBrightSense=0;

void handleTouch() {
  if(isOnTouch) {
    touchTimer++;    

    if(touchTimer > 100) {
      // Long touch: handle brightness
      if(ledBrightSense == 0) {
        ledBrightness--;
        if(ledBrightness < 10) {
          ledBrightSense = 1;
        }
      } else {
        ledBrightness++;
        if(ledBrightness > 250) {
          ledBrightSense = 0;          
        }
      }
    }
  }

  if(isTouchReleased) {
    Serial.printf("Touch duration %d ticks\n",touchTimer);
    if(touchTimer < 20) { // Fast touch = mode switch
      switch(ledMode) {
        case LED_OFF:
          ledMode=LED_SINGLE;
          break;
        case LED_SINGLE:
          ledMode=LED_PACIFICA;
          break;
        default:
        case LED_PACIFICA:
          ledMode=LED_OFF;
          break;
      }
    }
    isTouchReleased=false;
  }  
}

void loop() {
  // handle OTA
  ArduinoOTA.handle();
  // handle MQTT
  client.loop();
  // handle touch
  handleTouch();
  // other functions inside main loop...
  if((millis() - last) > 5100) {   
    // NTP Sync
    timeClient.update();

    last = millis();
  }

  switch(ledMode) {
    case LED_SINGLE:
      for(uint8_t c=0;c<NUM_LEDS;c++) {
          leds[c] = CRGB::White;
      }
      FastLED.show();
      break;
    case LED_PACIFICA:
      // To complete this loop, handle FastLEDs
      EVERY_N_MILLISECONDS(20) {
        pacifica_loop();
        FastLED.show();
      }
      break;
    case LED_OFF:
    default:
      for(uint8_t c=0;c<NUM_LEDS;c++) {
          leds[c] = CRGB::Black;
      }
      FastLED.show();
      break;    
  }
  FastLED.setBrightness(ledBrightness);

  delay(10);
}

// ****************************************
// FASTLED Pacifica theme
//
// ****************************************

CRGBPalette16 pacifica_palette_1 = 
    { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117, 
      0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50 };
CRGBPalette16 pacifica_palette_2 = 
    { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117, 
      0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F };
CRGBPalette16 pacifica_palette_3 = 
    { 0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33, 
      0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF };


void pacifica_loop()
{
  // Increment the four "color index start" counters, one for each wave layer.
  // Each is incremented at a different speed, and the speeds vary over time.
  static uint16_t sCIStart1, sCIStart2, sCIStart3, sCIStart4;
  static uint32_t sLastms = 0;
  uint32_t ms = GET_MILLIS();
  uint32_t deltams = ms - sLastms;
  sLastms = ms;
  uint16_t speedfactor1 = beatsin16(3, 179, 269);
  uint16_t speedfactor2 = beatsin16(4, 179, 269);
  uint32_t deltams1 = (deltams * speedfactor1) / 256;
  uint32_t deltams2 = (deltams * speedfactor2) / 256;
  uint32_t deltams21 = (deltams1 + deltams2) / 2;
  sCIStart1 += (deltams1 * beatsin88(1011,10,13));
  sCIStart2 -= (deltams21 * beatsin88(777,8,11));
  sCIStart3 -= (deltams1 * beatsin88(501,5,7));
  sCIStart4 -= (deltams2 * beatsin88(257,4,6));

  // Clear out the LED array to a dim background blue-green
  fill_solid( leds, NUM_LEDS, CRGB( 2, 6, 10));

  // Render each of four layers, with different scales and speeds, that vary over time
  pacifica_one_layer( pacifica_palette_1, sCIStart1, beatsin16( 3, 11 * 256, 14 * 256), beatsin8( 10, 70, 130), 0-beat16( 301) );
  pacifica_one_layer( pacifica_palette_2, sCIStart2, beatsin16( 4,  6 * 256,  9 * 256), beatsin8( 17, 40,  80), beat16( 401) );
  pacifica_one_layer( pacifica_palette_3, sCIStart3, 6 * 256, beatsin8( 9, 10,38), 0-beat16(503));
  pacifica_one_layer( pacifica_palette_3, sCIStart4, 5 * 256, beatsin8( 8, 10,28), beat16(601));

  // Add brighter 'whitecaps' where the waves lines up more
  pacifica_add_whitecaps();

  // Deepen the blues and greens a bit
  pacifica_deepen_colors();
}

// Add one layer of waves into the led array
void pacifica_one_layer( CRGBPalette16& p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff)
{
  uint16_t ci = cistart;
  uint16_t waveangle = ioff;
  uint16_t wavescale_half = (wavescale / 2) + 20;
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    waveangle += 250;
    uint16_t s16 = sin16( waveangle ) + 32768;
    uint16_t cs = scale16( s16 , wavescale_half ) + wavescale_half;
    ci += cs;
    uint16_t sindex16 = sin16( ci) + 32768;
    uint8_t sindex8 = scale16( sindex16, 240);
    CRGB c = ColorFromPalette( p, sindex8, bri, LINEARBLEND);
    leds[i] += c;
  }
}

// Add extra 'white' to areas where the four layers of light have lined up brightly
void pacifica_add_whitecaps()
{
  uint8_t basethreshold = beatsin8( 9, 55, 65);
  uint8_t wave = beat8( 7 );
  
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t threshold = scale8( sin8( wave), 20) + basethreshold;
    wave += 7;
    uint8_t l = leds[i].getAverageLight();
    if( l > threshold) {
      uint8_t overage = l - threshold;
      uint8_t overage2 = qadd8( overage, overage);
      leds[i] += CRGB( overage, overage2, qadd8( overage2, overage2));
    }
  }
}

// Deepen the blues and greens
void pacifica_deepen_colors()
{
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i].blue = scale8( leds[i].blue,  145); 
    leds[i].green= scale8( leds[i].green, 200); 
    leds[i] |= CRGB( 2, 5, 7);
  }
}
