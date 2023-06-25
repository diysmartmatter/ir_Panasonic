/* IRremote for Panasonic Aircon with IR recorder*/

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <cstring> //for std::memcpy method

//IR Remote
#include <IRremoteESP8266.h>
#include <ir_Panasonic.h>
const uint16_t kIrLed = 4;  //GPIO for IR LED. Recommended: 4.
IRPanasonicAc pana=IRPanasonicAc(kIrLed); //instance of Panasonic AC

//IR Recorder
#define RECLED 2 //while recording, turns on the blue LED on the board
#define RECMODULE 15 //signal pin for PL-IRM0101-3

//DHT sensor
#include "DHT20.h"
#define GPIO_SDA 21 //I2C for DHT20
#define GPIO_SCL 22 //I2C for DHT20
DHT20 dht; //instance of DHT20

//MQTT
#include <EspMQTTClient.h>
EspMQTTClient *client; //instance of MQTT

//WiFi & MQTT parameters
const char SSID[] = "XXXXXXXX"; //WiFi SSID
const char PASS[] = "xxxxxxxx"; //WiFi password
char CLIENTID[] = "IRremote_82705624"; //something random
const char  MQTTADD[] = "192.168.xxx.xxx"; //Broker IP address
const short MQTTPORT = 1883; //Broker port
const char  MQTTUSER[] = "";//Can be omitted if not needed
const char  MQTTPASS[] = "";//Can be omitted if not needed
const char  SUBTOPIC[] = "mqttthing/irESP32/set/#"; //to subscribe commands
const char  PUBTOPIC[] = "mqttthing/irESP32/get"; //to publish temperature
const char  DEBUG[] = "mqttthing/irESP32/debug"; //topic for debug

//base value for the IR Remote state (Panasonic AC)
uint8_t coolState[kPanasonicAcStateLength] = { //Cool 28, auto
0x02,0x20,0xE0,0x04,0x00,0x00,0x00,0x06,
0x02,0x20,0xE0,0x04,0x00,0x31,0x38,0x80,0xAF,0x00,0x00,0x06,0x60,0x00,0x00,0x80,0x00,0x16,0x9A
};
uint8_t heatState[kPanasonicAcStateLength] = { //Heat 20, auto
0x02,0x20,0xE0,0x04,0x00,0x00,0x00,0x06,
0x02,0x20,0xE0,0x04,0x00,0x41,0x28,0x80,0xAF,0x00,0x00,0x06,0x60,0x00,0x00,0x80,0x00,0x06,0x8A
};

void setup() {
  dht.begin(GPIO_SDA, GPIO_SCL); //DHT20
  pinMode(RECLED,OUTPUT);
  digitalWrite(RECLED,LOW);
  pinMode(RECMODULE,INPUT);
  pana.setRaw(coolState);
  pana.begin();   //IR Remo
  client = new EspMQTTClient(SSID,PASS,MQTTADD,MQTTUSER,MQTTPASS,CLIENTID,MQTTPORT); 
  client->setMaxPacketSize(3000);
  delay(1000);
}

void debugRaw(){
  const uint8_t *raw=pana.getRaw();
  char buf[kPanasonicAcStateLength * 2 + 1];
  int i;
  for(i=0;i<kPanasonicAcStateLength;i++){
    sprintf(&buf[i*2],"%02X",raw[i]);
  }
  buf[kPanasonicAcStateLength * 2]=0;
  client->publish(DEBUG,buf);
}

//MQTT: subtopic call back
void onMessageReceived(const String& topic, const String& message) { 
  String command = topic.substring(topic.lastIndexOf("/") + 1);

  if (command.equals("Active")) {
    if(message.equalsIgnoreCase("true")) pana.on();
    if(message.equalsIgnoreCase("false")) pana.off();
    pana.send();
    debugRaw();
  }else if(command.equals("TargetHeaterCoolerState")){
    if(message.equalsIgnoreCase("COOL")) {
      switchMode(kPanasonicAcCool);
    }
    if(message.equalsIgnoreCase("HEAT")) {
      switchMode(kPanasonicAcHeat);
    }
  }else if(command.equals("CoolingThresholdTemperature")){
    switchMode(kPanasonicAcCool);//just in case
    pana.setTemp(message.toInt());
    pana.send();
    debugRaw();
  }else if(command.equals("HeatingThresholdTemperature")){
    switchMode(kPanasonicAcHeat);//just in case
    pana.setTemp(message.toInt());
    pana.send();
    debugRaw();
  }else if(command.equals("SwingMode")){
    if(message.equalsIgnoreCase("DISABLED")) pana.setSwingVertical(kPanasonicAcSwingVLowest);
    if(message.equalsIgnoreCase("ENABLED")) pana.setSwingVertical(kPanasonicAcSwingVAuto);
    pana.send();
    debugRaw();
  }else if(command.equals("RotationSpeed")){
    int speed=message.toInt();
    if(speed < 5) pana.setFan(kPanasonicAcFanAuto);
    else if(speed < 20) pana.setFan(kPanasonicAcFanMin);
    else if(speed < 40) pana.setFan(kPanasonicAcFanLow);
    else if(speed < 60) pana.setFan(kPanasonicAcFanMed);
    else if(speed < 80) pana.setFan(kPanasonicAcFanHigh);
    else pana.setFan(kPanasonicAcFanMax);
  }else if(command.equals("CeilingOnOff")){
    if(message.equalsIgnoreCase("true")) irsend.sendRaw(kCeilingOnOff, 35, 38);
  }else if(command.equals("RecordIR")){
    int timeouts=message.toInt(); //read time out value (sec.)
    if(timeouts > 10) timeouts=10; //too long, max 10 sec.
    client->publish(DEBUG,"Recording starts.");
    digitalWrite(RECLED,HIGH);
    String result;
    result = recordIR(1000000L * timeouts);
    client->publish(DEBUG,result );
    digitalWrite(RECLED,LOW);
    client->publish(DEBUG,"Recording ends.");
  }
}

//MQTT: connection callback
void onConnectionEstablished() {
  ArduinoOTA.setHostname("Pana_IR");
  ArduinoOTA.setPasswordHash("xxxxxxxxxxxxxxxxxxxxxxxxxx");
  ArduinoOTA.begin();
  client->subscribe(SUBTOPIC, onMessageReceived); //set callback function
  client->publish(DEBUG,"irESP32 started.");
}

String recordIR(unsigned long timeoutmicro){
  //returns like this: {"sequence": [ 123, 456, 789 ]}
  String result="{\"sequence\":[";
  unsigned long zerostarttime, onestarttime;
  bool isTimeout=false;
  //wait for the first zero for timeoutmicro, otherwise set isTimeout to true
  onestarttime=micros();
  while(digitalRead(RECMODULE) == 1){
    if((micros()-onestarttime) > timeoutmicro){
      isTimeout=true;
      break;
    }
  }
  if(isTimeout) { //timeout
    result.concat(String("]}"));
    return result;  //returns zero result: {"sequence": []}
  }
  
  while(true) { //IR signal started.
    zerostarttime=micros(); //IR zero signal started.
    while(digitalRead(RECMODULE) == 0); //wait until zero ends
    result.concat(String(micros()-zerostarttime));
    onestarttime=micros();
    while(digitalRead(RECMODULE) == 1){
      if((micros()-onestarttime)>1000000L) {
        isTimeout=true;
        break;
      }
    }
    if(!isTimeout) { //Sequence continues...
      result.concat(String(","));
      result.concat(String(micros()-onestarttime));
      result.concat(String(","));
    }else{ //Sequence ends, by timeout
      result.concat(String("]}"));
      return result;
    }
  }
}


//Backup and switch the operating mode.
void switchMode(const uint8_t targetmode){
  uint8_t currentmode;
  
  currentmode = pana.getMode();
  if(targetmode == currentmode) return; //no switch, do nothing

  const uint8_t *raw=pana.getRaw();
  
  if((currentmode == kPanasonicAcHeat) && (targetmode == kPanasonicAcCool)) {
    std::memcpy(heatState, raw, kPanasonicAcStateLength);
    pana.setRaw(coolState);
  }
  if((currentmode == kPanasonicAcCool) && (targetmode == kPanasonicAcHeat)) {
    std::memcpy(coolState, raw, kPanasonicAcStateLength);
    pana.setRaw(heatState);
  }
}

//IR Remo and MQTT: read DHT20 and publish results
//check every 30 sec.
//if temp or humi change publish soon, otherwise publish every 5 min.
void publishDHT() { 
  char buff[64];
  static int count30=0; //counts up in every 30 sec.
  float humi, temp;
  static int oldhumi=0, oldtemp=0; //previous value
  int newhumi, newtemp; //new value
  if(millis() - dht.lastRead() < 30000) return;//do nothing < 30 sec.
  count30++; //count up 30 s counter
  if(DHT20_OK != dht.read()){
    client->publish(DEBUG,"DHT20 Read Error.");
    return; //sensor error, do nothing.
  }
  //read the current temp and humi
  humi=dht.getHumidity();
  newhumi=round(humi);//int version
  temp=dht.getTemperature();
  newtemp=round(temp * 10);//int version (x10)
  //if measurement changes or 300 seconds passed
  if((oldtemp != newtemp) || (oldhumi != newhumi) || (count30 >= 10)){
    oldtemp=newtemp;
    oldhumi=newhumi;    
    sprintf(buff, "{\"temperature\":%.1f,\"humidity\":%.0f}", temp, humi);
    client->publish(PUBTOPIC,buff);
    count30=0; //reset counter
  }
}

void loop() {
  ArduinoOTA.handle();
  client->loop(); 
  publishDHT(); //publish temp and humi if needed
}
