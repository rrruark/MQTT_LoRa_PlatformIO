/*
  Robert Ruark
  2023-03-25
  Heltec Wifi Lora32 V3 repeater / MQTT web interface.
  The LoRa syncword is 0x42 for testing.
  Radio configuration over MQTT will eventually be done with a JSON message, but for now the output power and spreading factor are defined with individual MQTT topics.
*/

// include the library
#include <RadioLib.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//WiFi & MQTT config parameters
//You can define these locally if you comment out the next four lines and uncomment the three lines after that.
#include "WiFiSecrets.h"
extern const char* ssid;
extern const char* password; 
extern const char* mqtt_server;
//const char* password    = "YOUR_WIFI_PASSWORD";
//const char* ssid        = "YOUR_SSID";
//const char* mqtt_server = "YOUR_MQTT_SERVER";

long lastMsg = 0;

// flag to indicate that a packet was received
volatile bool receivedFlag = false;
volatile bool operation_done = true;


#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif

//Radio Config Parameters
float lora_freq = 433.900;
float lora_BW   = 10.4;
int   lora_SF   = 7;
int   lora_PWR  = 10;
byte  sync_word  = 0x42;

WiFiClient espClient;
PubSubClient client(espClient);

//Radio Declaration
SX1262 radio = new Module(8, 14, 12, 13);

//Function prototypes
void config_radio();
void setFlag();
void setup_wifi();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);

void setup() 
{
  Serial.begin(9600);

  //LED Pin
  pinMode(35, OUTPUT);
  
  // initialize SX1262 with default settings
  Serial.print(F("[SX1262] Initializing ... "));
  void config_radio();

  //Set the function that will be called when a new packet is received
  radio.setDio1Action(setFlag);

  // start listening for LoRa packets
  Serial.print(F("[SX1262] Starting to listen ... "));
  int state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) 
  {
    Serial.println(F("success!"));
  } 
  else 
  {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }

  //WiFi and MQTT setup
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);  
}

//Tells the main loop that a packet has been received.
void setFlag(void) 
{
  receivedFlag = true;
}

//All this does is listen for
void loop() 
{
  if (!client.connected()) 
  {
    reconnect();
  }

  client.loop();
  // check if the flag is set
  if(receivedFlag && operation_done) 
  {
    // reset flag
    receivedFlag = false;

    // you can read received data as an Arduino String
    String str;
    int state = radio.readData(str);

    if (state == RADIOLIB_ERR_NONE) {
      // packet was successfully received
      Serial.println(F("[SX1262] Received packet!"));

      // print data of the packet
      Serial.print(F("[SX1262] Data:\t\t"));
      Serial.println(str);

      // print RSSI (Received Signal Strength Indicator)
      Serial.print(F("[SX1262] RSSI:\t\t"));
      Serial.print(radio.getRSSI());
      Serial.println(F(" dBm"));

      // print SNR (Signal-to-Noise Ratio)
      Serial.print(F("[SX1262] SNR:\t\t"));
      Serial.print(radio.getSNR());
      Serial.println(F(" dB"));

      // Publish the JSON message to the MQTT topic
      StaticJsonDocument<256> doc;
      doc["msg"] = str;
      char jsonBuffer[256];
      serializeJson(doc, jsonBuffer);
      StaticJsonDocument<32> rssi;
      doc["rssi"] = radio.getRSSI();
      serializeJson(doc, jsonBuffer);
      client.publish("home/mgs", jsonBuffer);

      // Transmit the received message
      digitalWrite(35, HIGH);
      radio.transmit(str);
      digitalWrite(35, LOW);
      receivedFlag = false;

    } 
    else if (state == RADIOLIB_ERR_CRC_MISMATCH) 
    {
      // packet was received, but is malformed
      Serial.println(F("CRC error!"));

    } 
    else 
    {
      // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(state);

    }

    // put module back to listen mode
    radio.startReceive();
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  operation_done=false;
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("]: ");
  String topic_local(topic); 

  if(topic_local=="home/mgs2")
  {
    // Print the payload as a string
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
    digitalWrite(35, HIGH);
    radio.transmit(payload, length);
    digitalWrite(35, LOW);
    //delay(100);
    operation_done=true;
    radio.startReceive();
    receivedFlag = false;
  }
  if(topic_local=="home/PWR")
  {
    String tempStr = "";
    for (int i = 0; i < length; i++) {
      tempStr.concat((char)payload[i]);
    }
    int temp_PWR = tempStr.toInt();
    if(temp_PWR>-1 && temp_PWR<21)
    {
      lora_PWR = temp_PWR;
      Serial.println();
      Serial.print("Transmit power has been set to ");
      Serial.println(lora_PWR);
      config_radio();      
    }
    else
    {
      Serial.println();
      Serial.print("Invalid power: ");
      Serial.println(temp_PWR);      
    }
  }
  if(topic_local=="home/SF")
  {
    String tempStr = "";
    for (int i = 0; i < length; i++) {
      tempStr.concat((char)payload[i]);
    }
    int temp_SF = tempStr.toInt();
    if(temp_SF>5 && temp_SF<13)
    {
      lora_SF = temp_SF;
      Serial.println();
      Serial.print("Transmit power has been set to ");
      Serial.println(lora_PWR);
      config_radio();      
    }
    else
    {
      Serial.println();
      Serial.print("Invalid SF: ");
      Serial.println(temp_SF);      
    }
  }
  else
  {
    Serial.println();
  }
}

void config_radio()
{
  int state = radio.begin(lora_freq,lora_BW,lora_SF,5,sync_word,lora_PWR);
  if (state == RADIOLIB_ERR_NONE) 
  {
    Serial.println(F("success!"));
  } 
  else 
  {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }  
  delay(100);
}

void setup_wifi() 
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int wifi_timeout =0;
  while (WiFi.status() != WL_CONNECTED || wifi_timeout>5) 
  {
    delay(500);
    Serial.print(".");
    wifi_timeout++;
    if(wifi_timeout>5)
    {
      Serial.println("Wifi Timeout");
    }
    else
    {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.println("done with wifi shit");
    }
  }
}

void reconnect() 
{
  Serial.print("Attempting MQTT connection...");
  if (client.connect("ESP32Client")) 
  {
    Serial.println("connected");

    // Subscribe to the MQTT topic
    client.subscribe("home/mgs2");
    client.subscribe("home/SF");
    client.subscribe("home/PWR");
  } 
  else 
  {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    //Serial.println(" try again in 5 seconds");
    //delay(5000);
  }
}