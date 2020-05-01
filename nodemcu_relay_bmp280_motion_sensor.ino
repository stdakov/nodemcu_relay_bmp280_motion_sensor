#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>

#define mqtt_server "192.168.1.10"
#define mqtt_port 1883

String in_topic = String("light/in");
String out_topic = String("light/out");

String temperature_topic = String("sensor/temperature");

String deviceName = String("mcu_relay_v12");

Adafruit_BMP280 bmp; // I2C

ESP8266WebServer server(80);
PubSubClient client;
WiFiClient espClient;

int motionSensorPin = 14; //D5(gpio14)
int relayPin = 13; // D7(gpio13)
int buttonPin = 12; //D6(gpio12)
boolean montionIsEnabled = true;
void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  pinMode(relayPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  if (digitalRead(buttonPin) == HIGH) {
    Serial.println("Motion sensor is disable, push the button when start the box");
    montionIsEnabled = false;
  } else {
    pinMode(motionSensorPin, INPUT);
    Serial.println("Motion sensor is enabled");
  }

  digitalWrite(relayPin, HIGH);

  if (!bmp.begin(0x76)) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  }

  temperature_topic = String(deviceName + "/" + temperature_topic);
  in_topic = String(deviceName + "/" + in_topic);
  out_topic = String(deviceName + "/" + out_topic);

  WiFiManager wifiManager;
  wifiManager.autoConnect(deviceName.c_str());

  // Start the server
  server.begin();
  Serial.println("Server started");

  client.setClient(espClient);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Print the IP address
  Serial.print(WiFi.localIP());
  Serial.println("/app?swich=1");
  Serial.print(WiFi.localIP());
  Serial.println("/app?swich=0");

  Serial.println(in_topic);
  Serial.println(out_topic);

  server.on("/app", handleSpecificArg); //Associate the handler
}

void handleSpecificArg() {

  String swich = "";

  if (server.arg("swich") != "") {   //Parameter not found
    swich = server.arg("swich");
    if (swich == "1") {
      relayOn();
    } else if (swich == "0") {
      relayOff();
    }
    Serial.println(server.arg("swich"));
  }

  server.send(200, "text/plain", swich);          //Returns the HTTP response
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(String("ESP8266Client_" + deviceName).c_str())) {
      Serial.println("connected");
      client.subscribe(in_topic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    char receivedChar = (char)payload[i];
    Serial.print(receivedChar);
    if (receivedChar == '0') {
      relayOff();
    }
    if (receivedChar == '1') {
      relayOn();
    }
  }
}

int currentButtonState = 0;
int lastButtonState = 0;

int buttonState = 0;
int buttonON = 0;
unsigned long previousMillisMotion = 0;
const long interval = 20000;

unsigned long previousMillistTemp = 0;
const long intervalTemp = 10000;

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentMillis = millis();

  if (montionIsEnabled && digitalRead(motionSensorPin) && previousMillisMotion == 0) {
    previousMillisMotion = currentMillis;
    Serial.println("Motion detected!");
    relayOn();
  }

  if (previousMillisMotion > 0 && currentMillis - previousMillisMotion >= interval && !digitalRead(motionSensorPin)) {
    Serial.println("Motion ended!");
    relayOff();
  }

  currentButtonState = digitalRead(buttonPin);
  if (currentButtonState != lastButtonState) {
    if (currentButtonState == HIGH && digitalRead(relayPin) == LOW) {
      relayOff();
    } else if (currentButtonState == HIGH && digitalRead(relayPin) == HIGH) {
      relayOn();
    }
  }
  
  lastButtonState = currentButtonState;

  if (currentMillis - previousMillistTemp >= intervalTemp) {
    previousMillistTemp = currentMillis;
    sentTemp();
  }
  
  server.handleClient();    //Handling of incoming requests
  delay(50);
}

void sentTemp() {
  Serial.print(F("Temperature = "));
  Serial.print(bmp.readTemperature());
  Serial.println(" *C");
  client.publish(temperature_topic.c_str(), String(bmp.readTemperature()).c_str(), true);
}

void relayOff() {
  Serial.println("off");
  previousMillisMotion = 0;
  digitalWrite(relayPin, HIGH);
  client.publish(out_topic.c_str(), String(0).c_str(), true);
}

void relayOn() {
  Serial.println("on");
  digitalWrite(relayPin, LOW);
  client.publish(out_topic.c_str(), String(1).c_str(), true);
}
