#include <Arduino.h>

//#include "FS.h" // LittleFS is declared
#include "LittleFS.h" // LittleFS is declared

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

// needed for OTA & MQTT
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <Adafruit_NeoPixel.h>

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40]  = "192.168.0.192";
char mqtt_port[6]     = "1883";

//Project name
char WifiApName[40] =     "Wifi_RGBNode";
char MQTTClientName[40] = "RGB_Node";
char WebHostOTA[40] =     "WEMOS_D1_RGB";


// MQTT
const char* pup_alive         = "/RGBNode/active";
const char* pup_LDR           = "/RGBNode/ldr";
const char* pup_trigger       = "/RGBNode/trigger";

const char* sub_RGBvalue      = "/RGBNode/rgbvalue";
const char* sub_freq          = "/RGBNode/freq";


// WIFI
WiFiClient espClient;
PubSubClient client(espClient);

// NeoPixel-Output
#define NEO_PIXEL_PIN            D2
#define BUILTIN_LED              D4

// Defines Timebase
#define INTERVAL0  500  //  1/2 sec
#define INTERVAL1  1000  //  1 sec
#define INTERVAL2  60000 //  60 sec = 1Min
#define INTERVAL3  1000  //   1 sec

// ADC-Filter
#define RATIO 0.90


// Define time table 
const int FrequencyTable[10] = {100,200,300,400,500,600,700,800,900,1000};

boolean RBG_LED_state = false;

char helpC[8] = "000000";

char helpCred[3];
char helpCgreen[3];
char helpCblue[3];

int red_p   = 0;
int green_p = 0;
int blue_p  = 0;

int frequency = 9;      // initial timebase: 1sec 

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// OTA setup function:
void OTA_setup (void)
{
  // Ergänzung OTA
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(WebHostOTA);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"admin");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)          Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)    Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)  Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)  Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)      Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address :   ");
  Serial.println(WiFi.localIP());
}

// MQTT callback function:
void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived @ PUB [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (strcmp(topic, sub_RGBvalue) == 0) {
    helpCred[0] = (char)payload[0];
    helpCred[1] = (char)payload[1];

    helpCgreen[0] = (char)payload[2];
    helpCgreen[1] = (char)payload[3];

    helpCblue[0] = (char)payload[4];
    helpCblue[1] = (char)payload[5];

    red_p = (int)strtol(helpCred, NULL, 16);
    green_p = (int)strtol(helpCgreen, NULL, 16);
    blue_p = (int)strtol(helpCblue, NULL, 16);

    Serial.println (red_p);
    Serial.println (green_p);
    Serial.println (blue_p);

    pixels.setPixelColor(0, pixels.Color(red_p, green_p, blue_p)); // Moderately bright green color.
    pixels.show(); // This sends the updated pixel color to the hardware.
  }


  if (strcmp(topic, sub_freq) == 0) {
    payload[length] = '\0';
    String s = String((char*)payload);
    frequency = s.toInt();
    Serial.println(frequency);
    Serial.println (FrequencyTable[frequency]);
  }
}

// RECONNECT MQTT Server
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTTClientName)) {
      
      client.subscribe(sub_RGBvalue); client.loop();
      client.subscribe(sub_freq); client.loop();

      Serial.println("connected ...");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      ArduinoOTA.handle();
      delay(1500);
      ArduinoOTA.handle();
      delay(1500);
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  // OUTPUT Definition 
  // HIGH to use LDR!
  pinMode(D5, OUTPUT);
  digitalWrite(D5, HIGH);

  // OTA reset 
  pinMode(D7, INPUT_PULLUP);
  boolean ota_reset = digitalRead(D7);

  // OUTPUT Definition !
  // initialize onboard LED as output
  pinMode(BUILTIN_LED, OUTPUT);  
  digitalWrite (BUILTIN_LED, LOW);

  pixels.begin(); // This initializes the NeoPixel library.
  pixels.clear();
  pixels.show(); // This sends the updated pixel color to the hardware.
  
  //clean FS, for testing
  //LittleFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (LittleFS.begin()) {
    Serial.println("mounted file system");
    if (LittleFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);

  //reset settings - for testing
  if (!ota_reset)
    wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

    wifiManager.setConfigPortalTimeout(180); // 3 Minuten!

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  //if (!wifiManager.autoConnect("WEMOS D1", "password")) {
  if (!wifiManager.autoConnect(WifiApName)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
  Serial.print("local ip    : ");
  Serial.println(WiFi.localIP());

  // OTA starts here!
  OTA_setup();

  // MQTT - Connection:
  client.setServer(mqtt_server, 1883);
  client.setCallback(MQTTcallback);

  Serial.print("mqtt server  : ");
  Serial.println(mqtt_server);

  client.publish(pup_trigger, "1");   // inital tigger to get first data 

  digitalWrite(BUILTIN_LED, HIGH);   // turn off LED with voltage LOW
}


void loop() {
  // MQTT connect
  if (!client.connected()) { reconnect(); }
  client.loop();

  // OTA handler !
  ArduinoOTA.handle();

  long now = millis();

  static long last_transmit   = 0;
  static long last_ldr_read   = 0;
  static long last_RGB_led_change = 0;

  static float sensorValueLDR = 0;
  static int sensorValueLDRmean     = 0;
  static int sensorValueLDRmean_old = 0;
  float pixelBrightness = 0;

  static boolean RGBledstate = true; 

  if ((last_ldr_read == 0) || (abs(now - last_ldr_read) > INTERVAL0))
  {
    last_ldr_read = now;
    // read the input on analog pin 0:
   int sensorValueLDR_raw = analogRead(A0);
        //Serial.print("LDR : ");Serial.println(sensorValueLDR_raw);
    sensorValueLDR = RATIO * sensorValueLDR + (1-RATIO) * sensorValueLDR_raw;
        // print out the value you read:        
        // Serial.print("LDR_mean: ");Serial.println(helper);
    sensorValueLDRmean = int (sensorValueLDR/4) - 1 ;
    pixelBrightness = sensorValueLDRmean / 1023 * 0.2;
        //Serial.print("Brightness : ");Serial.println(pixelBrightness);
    if (pixelBrightness > 1.00)
        pixelBrightness = 1.00;
   }

  if (abs(sensorValueLDRmean-sensorValueLDRmean_old) > 10)
  {
    //Serial.print("Publish LDR message: ");Serial.println(sensorValueLDRmean);
    client.publish(pup_LDR, String(sensorValueLDRmean).c_str());
    sensorValueLDRmean_old = sensorValueLDRmean;
  }

  if ((last_RGB_led_change == 0) || (abs(now - last_RGB_led_change) > FrequencyTable[frequency]))
  {
    last_RGB_led_change = now;
    if (RGBledstate)    {
      RGBledstate = false;
      pixels.setPixelColor(0, pixels.Color(red_p, green_p, blue_p));
      pixels.show();
    }
    else    {
      RGBledstate = true;
      pixels.setPixelColor(0,0,0,0);
      pixels.show();
   }
  }

  if ((last_transmit == 0) || (abs(now - last_transmit) > INTERVAL2))
  {
    last_transmit = now;
    sensorValueLDRmean_old = sensorValueLDRmean;
    Serial.print("Publish LDR message: ");Serial.println(sensorValueLDRmean);
    client.publish(pup_LDR, String(sensorValueLDRmean).c_str());

    Serial.print("Publish message: ");Serial.println("alive");
    //client.publish(pup_alive, String(sensorValueLDRmean).c_str());
    client.publish(pup_alive, "alive");
  }
}


  /*

 static long last_led_change = 0;
 static boolean ledstate = false;

  if (((last_led_change == 0) || (abs(now - last_led_change) > INTERVAL1)))
  {
    last_led_change = now;
    if (ledstate)    {
      ledstate = false;
      digitalWrite(BUILTIN_LED, HIGH);  // turn on LED with voltage HIGH
    }
    else    {
      ledstate = true;
      digitalWrite(BUILTIN_LED, LOW);   // turn off LED with voltage LOW
    }
  }
  */