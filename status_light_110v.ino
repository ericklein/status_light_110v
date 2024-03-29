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
// Adafruit_MQTT_Subscribe statusLightSub = Adafruit_MQTT_Subscribe(&aq_mqtt, MQTT_SUB_TOPIC);
extern Adafruit_MQTT_Subscribe statusLightSub;
extern void mqttConnect();
extern bool mqttDeviceWiFiUpdate(uint8_t rssi);
extern bool mqttStatusLightCheck();

// Global variables
unsigned long previousMQTTPingTime = 0;
uint8_t rssi = 0;

void setup() 
{
  #ifdef DEBUG
    Serial.begin(115200);
    while (!Serial) ;
    debugMessage("Status Light started",1);
    debugMessage(String("Client ID: ") + DEVICE_ID,1);
  #endif

  // activate and validate the status light
  pinMode(hardwareRelayPin, OUTPUT);
  for (uint8_t loop = 1; loop <4;loop++)
  {
    lightFlash(1);
  }

  // Setup network connection specified in config.h
  if (!networkConnect())
  {
    // alert user to the the connectivity problem
    debugMessage("unable to connect to WiFi",1);
    while (1)
      lightFlash(1);
  }
  aq_mqtt.subscribe(&statusLightSub);
}

void loop()
{
  mqttConnect();
  if (mqttStatusLightCheck())
    {
      debugMessage("turning status light on",1);
      digitalWrite(hardwareRelayPin, HIGH);
    }
    else
    {
      debugMessage("turning status light off",1);
      digitalWrite(hardwareRelayPin, LOW);
    }
  // Since we are subscribing only, we are required to ping the MQTT broker regularly
  if((millis() - previousMQTTPingTime) > (networkMQTTKeepAliveInterval * 1000))
  {
    previousMQTTPingTime = millis();   
    if(!aq_mqtt.ping())
    {
      debugMessage("unable to ping MQTT broker; disconnected",1);
      aq_mqtt.disconnect();
    }
    debugMessage("MQTT broker pinged to maintain connection",1);
  }
}

void debugMessage(String messageText, uint8_t messageLevel)
// wraps Serial.println as #define conditional
{
#ifdef DEBUG
  if (messageLevel <= DEBUG) {
    Serial.println(messageText);
    Serial.flush();  // Make sure the message gets output (before any sleeping...)
  }
#endif
}

void lightFlash(int interval) // interval in seconds
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

// void networkDisconnect()
// // Disconnect from WiFi network
// {
//   WiFi.disconnect();
//   WiFi.mode(WIFI_OFF);
//   debugMessage("power off: WiFi",1);
// }