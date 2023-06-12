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
float lora_freq = 445.900;
float lora_BW   = 62.5;
int   lora_SF   = 7;
int   lora_PWR  = 10;
byte  sync_word  = 0x42;

WiFiClient espClient;
PubSubClient client(espClient);

//Radio Declaration
SX1262 radio = new Module(8, 14, 12, 13);
PagerClient pager(&radio);

//Function prototypes
void config_radio();
void config_pager(float freq, int baud);
void pager_tx(String message, int id, int message_type);
void setFlag();
void setup_wifi();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
void initialize_topics();

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
      client.publish("radio/received_lora", jsonBuffer);

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

//Callback function called when subscribed MQTT topic is updated.
void callback(char* topic, byte* payload, unsigned int length) 
{
  operation_done=false;
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.println("]: ");
  String topic_local(topic);

  char jsonMessage[length + 1];
  memcpy(jsonMessage, payload, length);
  jsonMessage[length] = '\0';
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, jsonMessage);

  // Check if parsing was successful
  if (error) {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  if(topic_local=="radio/lora_message")
  {
    String message = doc["message"].as<String>();
    Serial.println(message);
    digitalWrite(35, HIGH);
    radio.transmit(message);
    digitalWrite(35, LOW);
    //delay(100);
    operation_done=true;
    radio.startReceive();
    receivedFlag = false;
  }

  if(topic_local=="radio/lora_config")
  {
    float temp_freq = doc["lora_freq"];
    lora_BW         = doc["lora_BW"];
    int temp_SF     = doc["lora_SF"];
    int temp_PWR    = doc["lora_PWR"];

    if(temp_PWR>-1 && temp_PWR<21)         lora_PWR = temp_PWR;
    if(temp_SF>5 && temp_SF<13)            lora_SF = temp_SF;
    if(temp_freq>420.0 && temp_freq<450.0) lora_freq = temp_freq;

    Serial.print("lora_freq: ");
    Serial.println(lora_freq);
    
    Serial.print("lora_BW: ");
    Serial.println(lora_BW);

    Serial.print("lora_SF: ");
    Serial.println(lora_SF);

    Serial.print("lora_PWR: ");
    Serial.println(lora_PWR);
    Serial.println();

    config_radio(); 
    radio.startReceive();
    receivedFlag = false;
  }

  if(topic_local=="radio/pager_message")
  {
    float frequency = doc["frequency"];
    int id = doc["id"];
    String message = doc["message"].as<String>();
    int message_type = doc["message_type"];

    Serial.print("Frequency: ");
    Serial.println(frequency);
    
    Serial.print("ID: ");
    Serial.println(id);

    Serial.print("message_type: ");
    Serial.println(message_type);

    Serial.print("Message: ");
    Serial.println(message);
    Serial.println();

    config_pager(frequency, 1200);
    pager_tx(message,id,message_type);
    //delay(100);
    operation_done=true;
    config_radio(); 
    radio.startReceive();
    receivedFlag = false;
  }

  else
  {
    Serial.println();
  }
}

void config_radio()
{
  radio.reset();
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

void config_pager(float freq, int baud)
{
  radio.reset();
  int state = radio.beginFSK();
  if (state == RADIOLIB_ERR_NONE) 
  {
    Serial.println(F("success!"));
  } 
  else 
  {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }
  Serial.print(F("[Pager] Initializing ... "));
  state = pager.begin(freq, baud);
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

void pager_tx(String message, int id, int message_type)
{
  digitalWrite(35, HIGH);
  int state;
  if(message_type==0)
  {
    state = pager.transmit(message, id);
  }
  else if(message_type==1)
  {
    state = pager.transmit(message, id, RADIOLIB_PAGER_ASCII);
  }
  digitalWrite(35, LOW);
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }
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
    client.subscribe("radio/lora_message");
    client.subscribe("radio/pager_message");
    client.subscribe("radio/lora_config");
    
  } 
  else 
  {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    //Serial.println(" try again in 5 seconds");
    //delay(5000);
  }
  delay(500);
  initialize_topics();
}

void initialize_topics()
{
  StaticJsonDocument<200> topics;
  char jsonBuffer[256];
  topics["lora_freq"] = lora_freq;
  topics["lora_BW"] = lora_BW;
  topics["lora_SF"] = lora_SF;
  topics["lora_PWR"] = lora_PWR;

  serializeJson(topics, jsonBuffer);

  client.publish("radio/lora_config", jsonBuffer);
}