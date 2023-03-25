# MQTT_LoRa_PlatformIO
ESP32 SX1262 LoRa to MQTT Bridge

This code is in a working state and is configured for the Heltec WiFi Lora 32 V2 Board. The only thing you eed to change to get started are the WiFi and MQTT configuration parameters.

It will transmit (over LoA using an SX1262) messages posted to a MQTT topic using radio configuration parameters defined in other MQTT topics.
It will also post messages received from the SX1262 to a different MQTT topic and will re-transmit received messages allowing it to act as a repeater.

The LoRa messages are not currently compatible with The Things Network, Helium, and other commonly used LoRaWAN Internet of Shit services.

My longer term ambition is to add support for <a href="https://github.com/lora-aprs/LoRa_APRS_Tracker">LoRa APRS</a> and <a href="https://github.com/aprs434/aprs434.github.io">aprs434</a>. LoRa APRS has chosen a bandwidth and spreading factor that results in much longer transmit times than traditional AFSK1200 APRS and it does not pack messages efficiently. aprs434 proposes a more efficient packing and a higher bitrate.
