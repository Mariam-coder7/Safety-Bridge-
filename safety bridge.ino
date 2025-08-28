#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#define LEDpin 14
#define buzzerPin 27
#define watersensorPin 25  // potentiometer acting as water sensor
#define vibrationsensorPin 26  // potentiometer acting as vibration sensor
#define servo1Pin 32  // gate 1
#define servo2Pin 33  // gate 2
LiquidCrystal_I2C LCD(0x27, 16, 2);  // LCD object
Servo servo1;  // servo object for gate 1
Servo servo2;  // servo object for gate 2

// --- Wi-Fi credentials ---
const char* ssid = "TEData_928DDB";
const char* password = "h9f09533";

// --- HiveMQ broker settings ---
const char* mqtt_server = "6cc755719171410a86e3baff39605d2b.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883;
const char* mqtt_user = "nadamo_0";
const char* mqtt_pass = "sherLocked10";

const char* topic_vibration = "bridge/sensor/vibration";
const char* topic_water = "bridge/sensor/water";

const char* topic_alerts = "bridge/alerts";    // to send alert messages to the app 

const char* topic_gate1_status = "bridge/gates/gate1/status";   //to show gate opened or closed
const char* topic_gate1_cmd    = "bridge/gates/gate1/cmd";        // to give commands to the gates from flutter app

const char* topic_gate2_status = "bridge/gates/gate2/status";
const char* topic_gate2_cmd    = "bridge/gates/gate2/cmd";


// --- Wi-Fi and MQTT clients ---
WiFiClientSecure secureClient;
PubSubClient client(secureClient);
String gateStatus; // Global variable for gate status

// Function declarations
void flood_warning();
void earthquake_warning();
void manual_override();
void reconnect();      // --- MQTT Reconnect ---
void callback(char* topic, byte* payload, unsigned int length); // --- Handle incoming messages ---



void setup() {
  Serial.begin(115200);

  pinMode(LEDpin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(watersensorPin, INPUT);
  pinMode(vibrationsensorPin, INPUT);
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  LCD.init();                      // Initialize LCD
  LCD.backlight();
  LCD.clear();                     // Clears display

  // -- wifi code --
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // MQTT
  secureClient.setInsecure();  // Skip cert check for now
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  int waterlevel = analogRead(watersensorPin);  
  // client.publish(topic_water, String(waterLevel).c_str());
  int waterthreshold = 500;
  int vibration = analogRead(vibrationsensorPin); 
  // client.publish(topic_earthquake, String(quakeValue).c_str());
  int vibrationthreshold = 1000;

  if (waterlevel > waterthreshold) {
    flood_warning();       
    // client.publish(topic_alerts, "Flood Detected! Evacuate!");
  }
  else {
    servo1.write(90);
    servo2.write(90);
    digitalWrite(LEDpin, LOW);
    digitalWrite(buzzerPin, LOW);
    LCD.clear();
    // client.publish(topic_alerts, "Bridge Safe");
  }
  if (vibration > vibrationthreshold) {
    earthquake_warning();      
    // client.publish(topic_alerts, "Earthquake Detected! Evacuate!");
  }
  else {
    servo1.write(90);
    servo2.write(90);
    digitalWrite(LEDpin, LOW);
    digitalWrite(buzzerPin, LOW);
    LCD.clear();
    // client.publish(topic_alerts, "Bridge Safe");
  }

  delay(10); // this speeds up the simulation
}


// --- MQTT Reconnect ---
void reconnect() {
    while (!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        if (client.connect("ESP32Client", mqtt_user, mqtt_pass)) {
            Serial.println("MQTT connected");
            client.subscribe(topic_gate1_cmd);  // Subscribe for gate 1 commands
            client.subscribe(topic_gate2_cmd);  // Subscribe for gate 2 commands
        } else {
            Serial.print("Failed. State=");
            Serial.print(client.state());
            Serial.println(" Retrying in 5 seconds...");
            delay(5000);
        }
    }
}


// --- Handle incoming messages ---
void callback(char* topic, byte* payload, unsigned int length) {
    payload[length] = '\0'; // null terminate
    String message = String((char*)payload);

    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    Serial.println(message);

    if (String(topic) == topic_gate1_cmd) {
        if (message == "OPEN") {
            gateStatus = "open";
            servo1.write(0); // Open gate 1
            Serial.println("Gate 1 opening...");
        } else if (message == "CLOSE") {
            gateStatus = "closed";
            servo1.write(90); // Close gate 1
            Serial.println("Gate 1 closing...");
        }
        // Publish updated status
        client.publish(topic_gate1_status, gateStatus.c_str());
    }

    if (String(topic) == topic_gate2_cmd) {
        if (message == "OPEN") {
            gateStatus = "open";
            servo2.write(0); // Open gate 2
            Serial.println("Gate 2 opening...");
        } else if (message == "CLOSE") {
            gateStatus = "closed";
            servo2.write(90); // Close gate 2
            Serial.println("Gate 2 closing...");
        }
        // Publish updated status
        client.publish(topic_gate2_status, gateStatus.c_str());
    }
}


void flood_warning() {
  servo1.write(180);
  servo2.write(180);
  digitalWrite(LEDpin, HIGH);  // turns warning lights on
  digitalWrite(buzzerPin, HIGH);  // turns warning buzzer on
  LCD.clear();  // clears display
  LCD.setCursor(0, 0);
  LCD.print("Flood");
  LCD.setCursor(0, 1);
  LCD.print("Warning!");
  delay(500);
}


void earthquake_warning() {
  servo1.write(180);
  servo2.write(180);
  digitalWrite(LEDpin, HIGH);  // turns warning lights on
  digitalWrite(buzzerPin, HIGH);  // turns warning buzzer on
  LCD.clear();  // clears display
  LCD.setCursor(0, 0);
  LCD.print("Earthquake");
  LCD.setCursor(0, 1);
  LCD.print("Warning!");
  delay(500); 
}


void manual_override() {
  servo1.write(90);
  servo2.write(90);
  digitalWrite(LEDpin, LOW);
  digitalWrite(buzzerPin, LOW);
  LCD.clear();
  LCD.setCursor(0, 0);
  LCD.print("Manual");
  LCD.setCursor(0, 1);
  LCD.print("Override!");
  delay(500);
}