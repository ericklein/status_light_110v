/*
  Project Name:   status light 110v
  Description:    110v light relay triggered by MQTT feed
*/

// hardware and internet configuration parameters
#include "config.h"
// private credentials for network, MQTT
#include "secrets.h"

#if defined (ESP8266)
  #include <ESP8266WiFi.h>
#elif defined (ESP32)
  #include <WiFi.h>
#endif

// MQTT setup
// MQTT uses WiFiClient class to create TCP connections
WiFiClient client;

#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
Adafruit_MQTT_Client aq_mqtt(&client, MQTT_BROKER, MQTT_PORT, DEVICE_ID, MQTT_USER, MQTT_PASS);
extern Adafruit_MQTT_Subscribe statusLightSub;
extern bool mqttConnect();
extern bool mqttDeviceWiFiUpdate(uint8_t rssi);
extern uint8_t mqttStatusLightMessage();

// Global variables
unsigned long previousMQTTPingTime = 0;
uint8_t rssi = 0; // 0 value used to indicate no WiFi connection

void setup() 
{
  // handle Serial first so debugMessage() works
  #ifdef DEBUG
    Serial.begin(115200);
    while (!Serial) ;
    debugMessage("Status Light started",1);
    debugMessage(String("Client ID: ") + DEVICE_ID,1);
  #endif

  pinMode(hardwareRelayPin, OUTPUT);

  // Setup network connection specified in config.h
  if (!networkConnect())
  {
    // alert user to the WiFi connectivity problem
    debugMessage("unable to connect to WiFi",1);
    for (uint8_t loop = 1; loop < 4; loop++)
      lightFlash(3);
    ESP.restart();
  }
  else
  {
    // validate the status light
    for (uint8_t loop = 1; loop < 4; loop++)
      lightFlash(1);
  }
  
  aq_mqtt.subscribe(&statusLightSub);
}

void loop()
{
  // ensure we have a connection to MQTT
  if (!mqttConnect())
  {
    for (uint8_t loop = 1; loop < 4; loop++)
      lightFlash(2);
  }
  else
  {
    // keep the MQTT broker connection active for subscription via ping
    if((millis() - previousMQTTPingTime) > (networkMQTTKeepAliveInterval * 1000))
    {
      previousMQTTPingTime = millis();   
      if(aq_mqtt.ping())
        debugMessage("MQTT broker pinged to maintain connection",1);
      else
      {
        debugMessage("unable to ping MQTT broker; disconnected",1);
        aq_mqtt.disconnect();
      }
    }
    // check to see if there is a status change for the light
    uint8_t status = mqttStatusLightMessage();

    if (status != 2) // 2 is no message received
    {
      debugMessage(String("MQTT message; change light to ") + (status == 1 ? "On" : "Off"),1);
      digitalWrite(hardwareRelayPin, status);
    }
  }
}

void debugMessage(String messageText, uint8_t messageLevel)
// wraps Serial.println as #define conditional
{
#ifdef DEBUG
  if (messageLevel <= DEBUG) {
    Serial.println(messageText);
    Serial.flush();  // Make sure the message gets output before other functions
  }
#endif
}

void lightFlash(uint8_t interval) // interval in seconds
// flash the status light
{
  digitalWrite(hardwareRelayPin, HIGH);
  delay(interval * 1000);
  digitalWrite(hardwareRelayPin, LOW);
  delay(interval * 1000);
}

bool networkConnect() 
// Connect to WiFi network specified in secrets.h
{
  // reconnect to WiFi only if needed
  if (WiFi.status() == WL_CONNECTED) 
  {
    debugMessage("Already connected to WiFi",2);
    return true;
  }
  // set hostname has to come before WiFi.begin
  WiFi.hostname(DEVICE_ID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  for (uint8_t loop = 1; loop <= networkConnectAttemptLimit; loop++)
  // Attempts WiFi connection, and if unsuccessful, re-attempts after networkConnectAttemptInterval second delay for networkConnectAttemptLimit times
  {
    if (WiFi.status() == WL_CONNECTED) 
    {
      rssi = abs(WiFi.RSSI());
      debugMessage(String("WiFi IP address lease from ") + WIFI_SSID + " is " + WiFi.localIP().toString(), 1);
      debugMessage(String("WiFi RSSI is: ") + rssi + " dBm", 1);
      return true;
    }
    debugMessage(String("Connection attempt ") + loop + " of " + networkConnectAttemptLimit + " to " + WIFI_SSID + " failed", 1);
    // use of delay() OK as this is initialization code
    delay(networkConnectAttemptInterval * 1000);  // converted into milliseconds
  }
  return false;
}