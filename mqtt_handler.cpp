/*
  Project:      status light 110v
  Description:  MQTT functions for status light 110v
*/

#include "Arduino.h"

// hardware and internet configuration parameters
#include "config.h"
// Overall data and metadata naming scheme
#include "data.h"
// private credentials for network, MQTT, weather provider
#include "secrets.h"

// required external functions and data structures
extern void debugMessage(String messageText, uint8_t messageLevel);

// Status variables shared across various functions

// MQTT setup
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
extern Adafruit_MQTT_Client aq_mqtt;
Adafruit_MQTT_Subscribe statusLightSub = Adafruit_MQTT_Subscribe(&aq_mqtt, MQTT_SUB_TOPIC);

bool mqttConnect()
// Connects and reconnects to MQTT broker, call as needed to maintain connection
{
  // exit if already connected
  if (aq_mqtt.connected())
  {
    debugMessage(String("Already connected to MQTT broker ") + MQTT_BROKER,2);
    return true;
  }

  // Q: does this need to be signed?
  int8_t mqttErr;

  for(uint8_t loop = 1; loop <= networkConnectAttemptLimit; loop++)
  {
    if ((mqttErr = aq_mqtt.connect()) == 0)
    {
      debugMessage(String("Connected to MQTT broker ") + MQTT_BROKER,1);
      return true;
    }

    // report problem
    aq_mqtt.disconnect();
    debugMessage(String("MQTT connection attempt ") + loop + " of " + networkConnectAttemptLimit + " failed with error msg: " + aq_mqtt.connectErrorString(mqttErr),1);
    delay(networkConnectAttemptInterval*1000);
  }
  // MQTT connection did not happen after multiple attempts
  return false;
} 

  String generateTopic(char *key)
  // Utility function to streamline dynamically generating MQTT topics using site and device 
  // parameters defined in config.h and our standard naming scheme using values set in data.h
  {
    String topic;
    topic = String(DEVICE_SITE) + "/" + String(DEVICE_LOCATION) + "/" + String(DEVICE_ROOM) +
            "/" + String(DEVICE) + "/" + String(key);
    debugMessage(String("Generated MQTT topic: ") + topic,2);
    return(topic);
  }

  bool mqttDeviceWiFiUpdate(uint8_t rssi)
  {
    bool result = false;
    if (rssi!=0)
    {
      String topic;
      topic = generateTopic(VALUE_KEY_RSSI);  // Generate topic using config.h and data.h parameters
      // add ,MQTT_QOS_1); if problematic, remove QOS parameter
      Adafruit_MQTT_Publish rssiLevelPub = Adafruit_MQTT_Publish(&aq_mqtt, topic.c_str());
      
      mqttConnect();

      if (rssiLevelPub.publish(rssi))
      {
        debugMessage("MQTT publish: WiFi RSSI succeeded",2);
        result = true;
      }
      else
      {
        debugMessage("MQTT publish: WiFi RSSI failed",2);
      }
    }
    return(result);
  }

uint8_t mqttStatusLightMessage()
{
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = aq_mqtt.readSubscription(5000))) 
  {
    if (subscription == &statusLightSub)
    {
      debugMessage(String("Received '") + ((char *)statusLightSub.lastread) + "' from " + MQTT_SUB_TOPIC,1); 
      return (atol((char *)statusLightSub.lastread) == 1) ? 1 : 0;
    }
  }
  // no message
  return 2;
}