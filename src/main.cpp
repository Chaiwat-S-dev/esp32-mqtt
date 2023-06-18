#include "secrets.h"
#include <WiFiClientSecure.h>
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define LED_BUILTIN 2
#define LED_CONTROL 25
#define ANALOG_VAL_MAX 255
#define ANALOG_VAL_MIN 0
#define MAC_LENGTH 20
#define ORG "SWD"
#define BUCKET "power_plant"
#define INTERVAL_TIME 5000
#define WIFI_TIMEOUT 30000
// #define WEATHER
#define ELECTRICITY
#define SECURE_MQTT_CONNECT
#define MAX_BATTERY 3.3
#define MIN_BATTERY 0.1
#define TOPIC_CONTROL "control/"

static double battery = 0.0, count = 0.1;
static uint8_t status_led = 0;

const char* ssid = "Your wifi ssid";
const char* password =  "password";

#ifdef WEATHER
  const char* measurment = "weather";
#else
  const char* measurment = "electricity";
#endif

#ifdef SECURE_MQTT_CONNECT
  const char* mqttServer = "a31dvipl38h049-ats.iot.ap-southeast-1.amazonaws.com";
  const int mqttPort = 8883;
#else
  const char* mqttServer = "192.168.11.29";
  const int mqttPort = 1883;
#endif

const char* mqttUser = "";
const char* mqttPassword = "";

#ifdef SECURE_MQTT_CONNECT
  WiFiClientSecure espClient = WiFiClientSecure();
#else
  WiFiClient espClient;
#endif

PubSubClient client(espClient); 

// static const char *mac;
char mac[MAC_LENGTH];
String topic = "swd/" + String(ORG) + '/' + String(BUCKET) + '/' + String(measurment);
String macAddress;

unsigned long interval = 0;

inline String GetMacAddress(WiFiClass *wifi) {
  String macAddress = wifi->macAddress();
  macAddress.replace(":", "");
  return macAddress;
}

inline double round2(double value) {
   return (int)(value * 100 + 0.5) / 100.0;
}

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    String buffer;
    
    for (int i = 0; i < length; i++)
      buffer = buffer + (char) payload[i];  // convert *byte to string
    
    StaticJsonDocument<200> doc;
    deserializeJson(doc, buffer);
    
    Serial.println(buffer);
    if (doc["control_led"] == 1) {
      status_led = 1;
      dacWrite(LED_CONTROL, ANALOG_VAL_MAX);
    }   // LED on
    if (doc["control_led"] == 0) {
      status_led = 0;
      dacWrite(LED_CONTROL, ANALOG_VAL_MIN);
    } // LED off
    Serial.println("-----------------------");
}

void setup() {
 
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_CONTROL, OUTPUT);
  interval = millis();
  
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
    if ((millis()-interval) < WIFI_TIMEOUT) {
      digitalWrite(LED_BUILTIN, LOW);
      Serial.println("Connecting to WiFi..");
      delay(500);
    }
    else {
      Serial.printf("Failed with state: %d \n", client.state());
      Serial.println("ESP is rebooting in 2 second.");
      delay(2000);
      ESP.restart();
    }
  }
  
  Serial.println("Connected to the WiFi network");
#ifdef SECURE_MQTT_CONNECT
  espClient.setCACert(AWS_CERT_CA);
  espClient.setCertificate(AWS_CERT_CRT);
  espClient.setPrivateKey(AWS_CERT_PRIVATE);
  Serial.printf("%s\n", AWS_CERT_CA);
  Serial.printf("%s\n", AWS_CERT_CRT);
  Serial.printf("%s\n", AWS_CERT_PRIVATE);
#endif
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
  macAddress = GetMacAddress(&WiFi);
  Serial.println("MAC: " + macAddress);
  
  while (!client.connected()) {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Connecting to MQTT...");
 
    if (client.connect(macAddress.c_str(), mqttUser, mqttPassword )) {
      digitalWrite(LED_BUILTIN, LOW);
      Serial.println("connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  String topicControl = TOPIC_CONTROL + macAddress;
  client.subscribe(topicControl.c_str());
  // Serial.printf("wifi mode is %d\n", WiFi.getMode());
  // macAddress = WiFi.macAddress();
  // macAddress.replace(":", "");
  // topic += '/' + macAddress;
  // strncpy(mac, macAddress.c_str(), MAC_LENGTH);
  // Serial.printf("MAC: %s\n", mac);
  Serial.printf("topic: %s\n", topic.c_str());
}
 
void loop() {
  if (client.connected()) {
    digitalWrite(LED_BUILTIN, HIGH);
    StaticJsonDocument<200> doc;
#ifdef WEATHER
    doc["mac_address"] = macAddress;
    doc["temp"] = random(0, 100);
    doc["humid"] = random(0, 100);
#else
    doc["mac_address"] = macAddress;
    doc["voltage"] = random(0, 220);
    doc["current"] = random(0, 100);
    doc["frequency"] = random(0, 50);
    doc["status_led"] = status_led;
#endif
    doc["signal"] = WiFi.RSSI();
    doc["battery"] = battery += count;
    if (battery < MIN_BATTERY || battery >= MAX_BATTERY)
      count *= -1;

  
  String output;
  serializeJson(doc, output);
  Serial.printf("result serialize: %s\n", output.c_str());
  client.publish(topic.c_str(), output.c_str());
  client.loop();
  digitalWrite(LED_BUILTIN, LOW);
  uint8_t ran = random(5);
  Serial.printf("Random waiting time is %d sec\n", ran);
  delay(1000);
  }
  else {
    client.disconnect();
    while (!client.connected()) {
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.print("Connecting to MQTT...");
      if (client.connect(macAddress.c_str(), mqttUser, mqttPassword )) {
        Serial.println("connected");
      }
      else {
        digitalWrite(LED_BUILTIN, LOW);
        Serial.printf("Failed with state: %d \n", client.state());
        Serial.println("ESP is rebooting in 2 second.");
        delay(2000);
        ESP.restart();
      }
    }
  }
}