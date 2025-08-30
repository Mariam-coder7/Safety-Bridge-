#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#define LEDpin 14
#define buzzerPin 27
#define water_powerPin  17 // ESP32 pin GPIO17 (TX2) connected to sensor's VCC pin
#define water_signalPin 36 // ESP32 pin GPIO36 (ADC0) (VP) connected to sensor's signal pin
#define water_min 0
#define water_max 521
#define vibrationsensorPin 34      // SW1801P vibration sensor (analog)
#define servo1Pin 32               // gate 1
#define servo2Pin 33               // gate 2

LiquidCrystal_I2C LCD(0x27, 16, 2);  // LCD object
Servo servo1;  // servo object for gate 1
Servo servo2;  // servo object for gate 2

int waterValue = 0; // variable to store the sensor value
int waterLevel = 0; // variable to store the water level
int waterThreshold = 1000; // variable to store the water threshold for us to consider it a flood


// --- Wi-Fi credentials ---
const char* ssid = "V~";
const char* password = "iloveyou";

// --- HiveMQ broker settings ---
const char* mqtt_server = "594cdc23084646ca970a765372b3bd99.s1.eu.hivemq.cloud"; 
const int mqtt_port = 8883;
const char* mqtt_user = "team32";
const char* mqtt_pass = "L0cked_up";

// MQTT Topics
const char* topic_vibration = "bridge/sensor/vibration";
const char* topic_water = "bridge/sensor/water";
const char* topic_alerts = "bridge/alerts";
const char* topic_gate1_status = "bridge/gates/gate1/status";
const char* topic_gate1_cmd = "bridge/gates/gate1/cmd";
const char* topic_gate2_status = "bridge/gates/gate2/status";
const char* topic_gate2_cmd = "bridge/gates/gate2/cmd";

// --- Wi-Fi and MQTT clients ---
WiFiClientSecure secureClient;
PubSubClient client(secureClient);
String gateStatus;

// --- Supabase Configuration ---
const char* supabase_url = "https://fvlptjzaleesanbusaoi.supabase.co";  // Replace with your URL
const char* supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImZ2bHB0anphbGVlc2FuYnVzYW9pIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTY0NzM0MzQsImV4cCI6MjA3MjA0OTQzNH0.d3XDPnlOiiDnr3v2IHiy5QBpKKauKP5HNcRVWmjyKtw";      // Replace with your API key

#include <HTTPClient.h>
#include <ArduinoJson.h>

// =============================================================================
// SW1801P VIBRATION SENSOR VARIABLES (Enhanced Integration)
// =============================================================================
int vibrationValue = 0;       // current analog reading
int baselineValue = 0;        // baseline/rest value
int vibrationLevel = 0;       // processed vibration intensity
int maxVibration = 0;         // peak vibration in current window

// Enhanced thresholds for better detection
const int minorVibThreshold = 30;   // minor vibration threshold
const int majorVibThreshold = 100;  // major vibration threshold  
const int earthquakeThreshold = 200; // earthquake threshold

// Timing variables for vibration analysis
unsigned long lastVibReading = 0;
unsigned long vibWindowStart = 0;
const int vibReadingInterval = 50;    // read every 50ms
const int vibWindowDuration = 3000;   // 3 second analysis window

// Calibration variables
bool vibrationCalibrated = false;
int calibrationCount = 0;
long calibrationSum = 0;
const int calibrationSamples = 100;   // More samples for better accuracy

// System states
enum SystemState {
  CALIBRATING,
  NORMAL,
  MINOR_VIBRATION,
  MAJOR_VIBRATION, 
  EARTHQUAKE,
  FLOOD,
  MANUAL_OVERRIDE
};
SystemState currentState = CALIBRATING;

// Water sensor threshold
int waterThreshold = 500;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================
void flood_warning();
void earthquake_warning();
void minor_vibration_warning();
void major_vibration_warning();
void manual_override();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
void updateVibrationSensor();
void publishSensorData();
void updateLCD();

void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(LEDpin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(water_powerPin, OUTPUT);   // configure D7 pin as an OUTPUT
  digitalWrite(water_powerPin, LOW); // turn the sensor OFF
  pinMode(vibrationsensorPin, INPUT);
  
  // Initialize servos and LCD
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  LCD.init();
  LCD.backlight();
  LCD.clear();
  
  // Set initial servo positions (gates closed)
  servo1.write(90);
  servo2.write(90);
  
  // Show calibration message
  LCD.setCursor(0, 0);
  LCD.print("Calibrating...");
  LCD.setCursor(0, 1);
  LCD.print("Keep sensor still");

  // Initialize timing
  vibWindowStart = millis();
  
  // -- Wi-Fi connection --
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // MQTT setup
  secureClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  Serial.println("System initialized. Starting vibration calibration...");
}

void loop() {
  // Handle MQTT connection
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Update vibration sensor (includes calibration)
  updateVibrationSensor();
  
  // Only proceed with normal monitoring after calibration
  if (!vibrationCalibrated) {
    return; // Stay in calibration mode
  }

  // water sensor loop code
  digitalWrite(water_powerPin, HIGH);  // turn the sensor ON
  delay(10);                      // wait 10 milliseconds
  waterLevel = analogRead(water_signalPin); // read the analog value from sensor
  digitalWrite(water_powerPin, LOW);   // turn the sensor OFF

  Serial.print("Water level: ");
  Serial.println(waterLevel);

  delay(1000);
  
  // Determine system state based on sensors
  SystemState newState = NORMAL;
  
  // Check for flood first (highest priority)
  if (waterLevel > waterThreshold) {
    newState = FLOOD;
  }
  // Check vibration levels
  else if (vibrationLevel >= earthquakeThreshold || maxVibration >= earthquakeThreshold) {
    newState = EARTHQUAKE;
  }
  else if (vibrationLevel >= majorVibThreshold || maxVibration >= majorVibThreshold) {
    newState = MAJOR_VIBRATION;
  }
  else if (vibrationLevel >= minorVibThreshold || maxVibration >= minorVibThreshold) {
    newState = MINOR_VIBRATION;
  }

  // State change handling
  if (newState != currentState) {
    currentState = newState;
    Serial.print("State changed to: ");
    Serial.println(currentState);
  }

  // Execute state actions
  switch (currentState) {
    case NORMAL:
      // Normal operation - gates open, no alarms
      servo1.write(90);
      servo2.write(90);
      digitalWrite(LEDpin, LOW);
      digitalWrite(buzzerPin, LOW);
      break;
      
    case MINOR_VIBRATION:
      minor_vibration_warning();
      break;
      
    case MAJOR_VIBRATION:
      major_vibration_warning();
      break;
      
    case EARTHQUAKE:
      earthquake_warning();
      break;
      
    case FLOOD:
      flood_warning();
      break;
      
    case MANUAL_OVERRIDE:
      manual_override();
      break;
  }

  // Update LCD display
  updateLCD();
  
  // Publish sensor data to MQTT (every 1 second)
  static unsigned long lastPublish = 0;
  if (millis() - lastPublish >= 1000) {
    publishSensorData();
    lastPublish = millis();
  }

  delay(10);
}
// supabase function
void sendSensorDataToSupabase() {
    if (WiFi.status() == WL_CONNECTED && vibrationCalibrated) {
        HTTPClient http;
        http.begin(String(supabase_url) + "/rest/v1/sensor_readings");
        http.addHeader("Content-Type", "application/json");
        http.addHeader("apikey", supabase_key);
        http.addHeader("Authorization", "Bearer " + String(supabase_key));
        
        // Create JSON payload
        DynamicJsonDocument doc(1024);
        doc["vibration_level"] = vibrationLevel;
        doc["max_vibration"] = maxVibration;
        doc["water_level"] = analogRead(watersensorPin);
        doc["system_state"] = getStateString(currentState);
        doc["alert_message"] = getCurrentAlertMessage();
        
        String jsonString;
        serializeJson(doc, jsonString);
        
        int httpResponseCode = http.POST(jsonString);
        
        if (httpResponseCode > 0) {
            Serial.print("Supabase response: ");
            Serial.println(httpResponseCode);
        } else {
            Serial.print("Supabase error: ");
            Serial.println(httpResponseCode);
        }
        
        http.end();
    }
}

void sendGateOperationToSupabase(int gateNumber, String action, String status, String reason) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(String(supabase_url) + "/rest/v1/gate_operations");
        http.addHeader("Content-Type", "application/json");
        http.addHeader("apikey", supabase_key);
        http.addHeader("Authorization", "Bearer " + String(supabase_key));
        
        DynamicJsonDocument doc(512);
        doc["gate_number"] = gateNumber;
        doc["action"] = action;
        doc["status"] = status;
        doc["trigger_reason"] = reason;
        
        String jsonString;
        serializeJson(doc, jsonString);
        
        http.POST(jsonString);
        http.end();
    }
}

void sendAlertToSupabase(String alertType, String severity, String message) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(String(supabase_url) + "/rest/v1/system_alerts");
        http.addHeader("Content-Type", "application/json");
        http.addHeader("apikey", supabase_key);
        http.addHeader("Authorization", "Bearer " + String(supabase_key));
        
        DynamicJsonDocument doc(512);
        doc["alert_type"] = alertType;
        doc["severity"] = severity;
        doc["message"] = message;
        
        String jsonString;
        serializeJson(doc, jsonString);
        
        http.POST(jsonString);
        http.end();
    }
}

String getStateString(SystemState state) {
    switch (state) {
        case NORMAL: return "NORMAL";
        case MINOR_VIBRATION: return "MINOR_VIBRATION";
        case MAJOR_VIBRATION: return "MAJOR_VIBRATION";
        case EARTHQUAKE: return "EARTHQUAKE";
        case FLOOD: return "FLOOD";
        case MANUAL_OVERRIDE: return "MANUAL_OVERRIDE";
        case CALIBRATING: return "CALIBRATING";
        default: return "UNKNOWN";
    }
}

String getCurrentAlertMessage() {
    switch (currentState) {
        case NORMAL: return "Bridge Safe";
        case MINOR_VIBRATION: return "Minor Vibration Detected";
        case MAJOR_VIBRATION: return "Major Vibration - Caution!";
        case EARTHQUAKE: return "EARTHQUAKE DETECTED! EVACUATE!";
        case FLOOD: return "FLOOD DETECTED! EVACUATE!";
        case MANUAL_OVERRIDE: return "Manual Override Active";
        case CALIBRATING: return "System Calibrating";
        default: return "Unknown State";
    }
}

// =============================================================================
// VIBRATION SENSOR FUNCTIONS
// =============================================================================
void updateVibrationSensor() {
  unsigned long currentTime = millis();
  
  // Read sensor at specified interval
  if (currentTime - lastVibReading >= vibReadingInterval) {
    vibrationValue = analogRead(vibrationsensorPin);
    
    // Calibration phase
    if (!vibrationCalibrated) {
      calibrationSum += vibrationValue;
      calibrationCount++;
      
      if (calibrationCount >= calibrationSamples) {
        baselineValue = calibrationSum / calibrationSamples;
        vibrationCalibrated = true;
        LCD.clear();
        Serial.print("Vibration calibration complete. Baseline: ");
        Serial.println(baselineValue);
      }
      
      // Show calibration progress
      LCD.setCursor(0, 1);
      LCD.print("Progress: ");
      LCD.print((calibrationCount * 100) / calibrationSamples);
      LCD.print("%   ");
      
      lastVibReading = currentTime;
      return;
    }
    
    // Calculate vibration intensity
    vibrationLevel = abs(vibrationValue - baselineValue);
    
    // Track maximum vibration in current window
    if (vibrationLevel > maxVibration) {
      maxVibration = vibrationLevel;
    }
    
    lastVibReading = currentTime;
  }
  
  // Reset analysis window
  if (currentTime - vibWindowStart >= vibWindowDuration) {
    Serial.print("Vibration window: Max=");
    Serial.print(maxVibration);
    Serial.print(", Current=");
    Serial.println(vibrationLevel);
    
    maxVibration = 0;  // Reset for next window
    vibWindowStart = currentTime;
  }
}

void publishSensorData() {
  if (client.connected() && vibrationCalibrated) {
    // Publish vibration data
    client.publish(topic_vibration, String(vibrationLevel).c_str());
    
    // Publish water level
    int waterLevel = analogRead(watersensorPin);
    client.publish(topic_water, String(waterLevel).c_str());
    
    // Publish alerts based on current state
    String alertMessage = "";
    switch (currentState) {
      case NORMAL:
        alertMessage = "Bridge Safe";
        break;
      case MINOR_VIBRATION:
        alertMessage = "Minor Vibration Detected";
        break;
      case MAJOR_VIBRATION:
        alertMessage = "Major Vibration - Caution!";
        break;
      case EARTHQUAKE:
        alertMessage = "EARTHQUAKE DETECTED! EVACUATE!";
        break;
      case FLOOD:
        alertMessage = "FLOOD DETECTED! EVACUATE!";
        break;
      case MANUAL_OVERRIDE:
        alertMessage = "Manual Override Active";
        break;
    }
    client.publish(topic_alerts, alertMessage.c_str());
      // Also send to Supabase every 3 seconds to avoid too many requests
        static unsigned long lastSupabaseUpdate = 0;
        if (millis() - lastSupabaseUpdate >= 3000) {
            sendSensorDataToSupabase();
            lastSupabaseUpdate = millis();
        }





  }
}

void updateLCD() {
  if (!vibrationCalibrated) return; // Don't update during calibration
  
  LCD.clear();
  LCD.setCursor(0, 0);
  
  switch (currentState) {
    case NORMAL:
      LCD.print("Bridge Safe");
      break;
      
    case MINOR_VIBRATION:
      LCD.print("Minor Vibration");
      LCD.setCursor(0, 1);
      LCD.print("Level: ");
      LCD.print(vibrationLevel);
      break;
      
    case MAJOR_VIBRATION:
      LCD.print("Major Vibration");
      LCD.setCursor(0, 1);
      LCD.print("CAUTION!");
      break;
      
    case EARTHQUAKE:
      LCD.print("!! EARTHQUAKE !!");
      LCD.setCursor(0, 1);
      LCD.print("EVACUATE NOW!");
      delay(500);
      break;
      
    case FLOOD:
      LCD.print("!! FLOOD !!");
      LCD.setCursor(0, 1);
      LCD.print("EVACUATE NOW!");
      delay(500);
      break;
      
    case MANUAL_OVERRIDE:
      LCD.print("Manual Override");
      LCD.setCursor(0, 1);
      LCD.print("Active");
      delay(500);
      break;
  }
}

// =============================================================================
// WARNING FUNCTIONS (Enhanced)
// =============================================================================
void minor_vibration_warning() {
  // Yellow alert - keep gates open, minor warning
  servo1.write(90);
  servo2.write(90);
  
  // Slow LED blink
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  if (millis() - lastBlink > 1000) {
    ledState = !ledState;
    digitalWrite(LEDpin, ledState);
    lastBlink = millis();
  }
  
  digitalWrite(buzzerPin, LOW); // No buzzer for minor vibrations
}

void major_vibration_warning() {
  // Orange alert - close gates, stronger warning
  servo1.write(45);  // Partially close gates
  servo2.write(45);
  
  // Fast LED blink
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  if (millis() - lastBlink > 500) {
    ledState = !ledState;
    digitalWrite(LEDpin, ledState);
    lastBlink = millis();
  }
  
  // Intermittent buzzer
  static unsigned long lastBuzz = 0;
  static bool buzzState = false;
  if (millis() - lastBuzz > 1000) {
    buzzState = !buzzState;
    digitalWrite(buzzerPin, buzzState);
    lastBuzz = millis();
  }
}

void earthquake_warning() {
  // Red alert - fully close gates, maximum warning
  servo1.write(0);   //  close gates
  servo2.write(90);  // open Exit gate
  digitalWrite(LEDpin, HIGH);
  
  // Pulsing buzzer for maximum attention
  static unsigned long lastPulse = 0;
  static bool pulseState = false;
  if (millis() - lastPulse > 200) {
    pulseState = !pulseState;
    digitalWrite(buzzerPin, pulseState);
    lastPulse = millis();
  }

     //send to supabase 
   static bool alertSent = false;
    if (!alertSent) {
        sendAlertToSupabase("EARTHQUAKE", "CRITICAL", "Earthquake detected - bridge evacuation required");
        sendGateOperationToSupabase(1, "CLOSE", "closed", "earthquake_detection");
        sendGateOperationToSupabase(2, "OPEN", "open", "earthquake_evacuation");
        alertSent = true;
    }
}

void flood_warning() {
  // Flood alert - fully close gates
  servo1.write(0);
  servo2.write(90);  // exit open
  digitalWrite(LEDpin, HIGH);
  digitalWrite(buzzerPin, HIGH);

    //send to supabase
   static bool alertSent = false;
    if (!alertSent) {
        sendAlertToSupabase("FLOOD", "CRITICAL", "Flood detected - bridge closure required");
        sendGateOperationToSupabase(1, "CLOSE", "closed", "flood_detection");
        sendGateOperationToSupabase(2, "OPEN", "open", "flood_evacuation");
        alertSent = true;
    }
}

void manual_override() {
  // Manual control - gates controlled by MQTT
  digitalWrite(LEDpin, LOW);
  digitalWrite(buzzerPin, LOW);
}

// =============================================================================
// MQTT FUNCTIONS
// =============================================================================
void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_pass)) {
      Serial.println("MQTT connected");
      client.subscribe(topic_gate1_cmd);
      client.subscribe(topic_gate2_cmd);
    } else {
      Serial.print("Failed. State=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = String((char*)payload);

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(message);

  // Handle gate commands (only in normal state or manual override)
  if (currentState == NORMAL || currentState == MANUAL_OVERRIDE) {
    if (String(topic) == topic_gate1_cmd) {
      if (message == "OPEN") {
        gateStatus = "open";
        servo1.write(90);
        currentState = MANUAL_OVERRIDE;
      } else if (message == "CLOSE") {
        gateStatus = "closed";
        servo1.write(0);
        currentState = MANUAL_OVERRIDE;
      }
      client.publish(topic_gate1_status, gateStatus.c_str());
    }

    if (String(topic) == topic_gate2_cmd) {
      if (message == "OPEN") {
        gateStatus = "open";
        servo2.write(90);
        currentState = MANUAL_OVERRIDE;
      } else if (message == "CLOSE") {
        gateStatus = "closed";
        servo2.write(0);
        currentState = MANUAL_OVERRIDE;
      }
      client.publish(topic_gate2_status, gateStatus.c_str());
    }
  } else {
    // Reject manual commands during emergency
    Serial.println("Manual commands rejected - emergency state active");
  }
}
