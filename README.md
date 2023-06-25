# ir_Panasonic
IR remote control program for Panasonic Air-conditioner using IRremoteESP8266

The target of this library is Panasonic A/C that uses Panasonic A75C3777 remote controller. 

![A75C3777](https://diysmartmatter.com/images/20221201181010.png "A75C3777")

You could buid a smart IR remote controller using ESP32:

![ESP32](https://diysmartmatter.com/wp-content/uploads/2023/06/esp32recorder0.jpg)

![Diagram](https://diysmartmatter.com/wp-content/uploads/2023/06/circuit_new.jpg)

that can be used by Apple HomeKit as a Heater/Cooler accessory. 

![HomeKit](https://diysmartmatter.com/images/20221123195125.png)

# Background

The IRremoteESP8266 library has a very excellent class for Panasonic air-conditioners, although some of data bits in the IR data slightly differ from my Panasonic products using an A75C3777 remote controller. This program fixes the IR data. Note that only essential methods required by HomeKit Heater Cooler Accessory are implemented. 

# Prerequisite

![Configuration](https://diysmartmatter.com/wp-content/uploads/2023/04/remo_E.jpg)


- Homebridge
- MQTT (Mosquitto)
- Arduino IDE
- ESP32 with IR LED(s)
- ESP32 Arduino (Board manager)
- Arduino libraries:
- IRremoteESP8266 
- ArduinoOTA
- EspMQTTClient
- DHT20


# How to use

- Create an Arduino sketch using example.ino. (WiFi info and address should be changed to yours)

# Recording IR-remote signals

Besides smart IR remote functions, this program support to record IR signal sequences from any IR remote controller. Sending time-out seconds to RecordIR topics, for example (of mosquitto_pub command):

% mosquitto_pub -t mqttthing/irESP32/set/RecordIR -m 5

will scan an IR signal for 5 seconds. If an IR sequence is captured, it will be returned via MQTT message. For example (of mosquitto_sub command):

% mosquitto_sub -t mqttthing/irESP32/# -v
mqttthing/irESP32/debug {"sequence":[6894,4516,328,1340,335,507,334,1340,326,512,334,1340,327,515,334,1340,327,515,333,511,326,1345,334,511,326,517,333,1340,327,1342,357,1309,334,1345,335]}

The numbers in the message are durations in micro-seconds of on or off in IR signals. (In the above example, IR is off for 6894, on for 4516, off for 328, on for 1340 micro sedonds and so on.)
