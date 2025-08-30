#  Smart Bridge Monitoring System

An intelligent IoT-based bridge safety monitoring system using ESP32, designed to detect earthquakes, floods, and structural vibrations to ensure public safety through automated gate controls and real-time alerts.

![Bridge Monitoring System](https://img.shields.io/badge/Status-Active-brightgreen) ![ESP32](https://img.shields.io/badge/Hardware-ESP32-blue) ![Arduino](https://img.shields.io/badge/Framework-Arduino-teal) ![MIT License](https://img.shields.io/badge/License-MIT-yellow)

##  Project Overview

This system continuously monitors bridge conditions using vibration and water level sensors, automatically controlling entry/exit gates based on detected hazards. All data is logged to Supabase for remote monitoring and historical analysis.

### Key Features
- **Real-time Vibration Detection** using SW1801P analog sensor
- **Flood Detection** with water level monitoring
- **Automated Gate Control** with servo motors
- **Multi-level Alert System** (Visual, Audio, Remote)
- **Cloud Data Logging** via Supabase integration
- **MQTT Communication** for remote control
- **LCD Status Display** for local monitoring

##  System Architecture

```
[Sensors] → [ESP32] → [Local Actions] → [Cloud Storage]
    ↓         ↓           ↓              ↓
Vibration   Process    Gate Control   Supabase DB
Water Level  Data      LED/Buzzer     MQTT Broker
```

##  Hardware Components

| Component | Model | Purpose | Pin |
|-----------|-------|---------|-----|
| Microcontroller | ESP32 | Main processing unit | - |
| Vibration Sensor | SW1801P | Earthquake/vibration detection | Pin 34 |
| Water Sensor | Generic | Flood detection | Pin 25 |
| Servo Motors | SG90 (x2) | Gate control | Pins 32, 33 |
| LCD Display | 16x2 I2C | Status display | I2C |
| LED | Generic | Visual alert | Pin 14 |
| Buzzer | Active | Audio alert | Pin 27 |

##  System States

| State | Vibration Level | Actions | Gates | Alerts |
|-------|----------------|---------|-------|--------|
| **Normal** | 0-29 | Normal operation | Open | None |
| **Minor Vibration** | 30-99 | Monitor closely | Open | Slow LED blink |
| **Major Vibration** | 100-199 | Caution mode | Partial close | Fast LED + intermittent buzzer |
| **Earthquake** | 200+ | Emergency shutdown | Entry closed, Exit open | Continuous LED + rapid buzzer |
| **Flood** | Water > threshold | Emergency shutdown | Entry closed, Exit open | Continuous LED + buzzer |

## 🚀 Quick Start

### Prerequisites
- Arduino IDE with ESP32 board support
- Required libraries (see Installation section)
- WiFi network access
- Supabase account
- HiveMQ Cloud account (or MQTT broker)

### Installation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/yourusername/smart-bridge-monitoring](https://github.com/Mariam-coder7/Safety-.git
   cd smart-bridge-monitoring
   ```

2. **Install required Arduino libraries:**
   ```
   ESP32Servo
   LiquidCrystal_I2C
   WiFi
   PubSubClient
   WiFiClientSecure
   HTTPClient
   ArduinoJson
   ```

3. **Configure credentials:**
   ```cpp
   // WiFi Settings
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   
   // MQTT Settings
   const char* mqtt_server = "YOUR_MQTT_BROKER";
   const char* mqtt_user = "YOUR_MQTT_USER";
   const char* mqtt_pass = "YOUR_MQTT_PASS";
   
   // Supabase Settings
   const char* supabase_url = "https://fvlptjzaleesanbusaoi.supabase.co";
   const char* supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImZ2bHB0anphbGVlc2FuYnVzYW9pIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTY0NzM0MzQsImV4cCI6MjA3MjA0OTQzNH0.d3XDPnlOiiDnr3v2IHiy5QBpKKauKP5HNcRVWmjyKtw";
   ```

4. **Upload to ESP32:**
   - Connect ESP32 via USB
   - Select board and port in Arduino IDE
   - Upload the sketch

#### `sensor_readings`
```sql
CREATE TABLE sensor_readings (
    id SERIAL PRIMARY KEY,
    vibration_level INTEGER,
    max_vibration INTEGER,
    water_level INTEGER,
    system_state TEXT,
    alert_message TEXT,
    timestamp TIMESTAMPTZ DEFAULT NOW()
);
```

#### `gate_operations`
```sql
CREATE TABLE gate_operations (
    id SERIAL PRIMARY KEY,
    gate_number INTEGER,
    action TEXT,
    status TEXT,
    trigger_reason TEXT,
    timestamp TIMESTAMPTZ DEFAULT NOW()
);
```

#### `system_alerts`
```sql
CREATE TABLE system_alerts (
    id SERIAL PRIMARY KEY,
    alert_type TEXT,
    severity TEXT,
    message TEXT,
    timestamp TIMESTAMPTZ DEFAULT NOW()
);
```

##  MQTT Topics

| Topic | Purpose | Message Format |
|-------|---------|----------------|
| `bridge/sensor/vibration` | Vibration readings | Integer (0-1000+) |
| `bridge/sensor/water` | Water level readings | Integer (0-4095) |
| `bridge/alerts` | System alerts | String message |
| `bridge/gates/gate1/cmd` | Gate 1 commands | "OPEN" / "CLOSE" |
| `bridge/gates/gate1/status` | Gate 1 status | "open" / "closed" |
| `bridge/gates/gate2/cmd` | Gate 2 commands | "OPEN" / "CLOSE" |
| `bridge/gates/gate2/status` | Gate 2 status | "open" / "closed" |

##  Configuration

### Sensor Calibration
- System automatically calibrates vibration sensor on startup
- Requires 5-10 seconds of no movement during initialization
- Baseline value stored for accurate vibration detection

### Threshold Adjustment
```cpp
const int minorVibThreshold = 30;     // Adjust for sensitivity
const int majorVibThreshold = 100;    // Adjust for local conditions
const int earthquakeThreshold = 200;  // Critical level threshold
int waterThreshold = 500;             // Flood detection level
```

##  Data Flow

1. **Sensor Reading** (Every 50ms)
   - Vibration sensor: Analog reading processed against baseline
   - Water sensor: Analog reading compared to threshold

2. **State Processing** (Continuous)
   - Analyze sensor data
   - Determine appropriate system state
   - Execute state-specific actions

3. **Data Logging** (Every 30 seconds)
   - Send sensor readings to Supabase
   - Publish MQTT messages
   - Log gate operations and alerts

4. **Emergency Response** (Immediate)
   - Critical alerts sent instantly to Supabase
   - Gate operations logged with reason
   - MQTT emergency broadcasts

## 🎛️ Remote Control

### MQTT Commands
```bash
# Open/Close Gate 1
mosquitto_pub -h YOUR_BROKER -u USER -P PASS -t "bridge/gates/gate1/cmd" -m "OPEN"
mosquitto_pub -h YOUR_BROKER -u USER -P PASS -t "bridge/gates/gate1/cmd" -m "CLOSE"

# Open/Close Gate 2  
mosquitto_pub -h YOUR_BROKER -u USER -P PASS -t "bridge/gates/gate2/cmd" -m "OPEN"
mosquitto_pub -h YOUR_BROKER -u USER -P PASS -t "bridge/gates/gate2/cmd" -m "CLOSE"
```

**Note:** Manual commands are rejected during emergency states for safety.

## 🔍 Monitoring & Analytics

### Real-time Monitoring

- Local LCD display for on-site status

### Data Analysis Queries
```sql
-- Recent vibration patterns
SELECT vibration_level, max_vibration, timestamp 
FROM sensor_readings 
WHERE timestamp > NOW() - INTERVAL '1 hour'
ORDER BY timestamp DESC;

-- Emergency events
SELECT alert_type, severity, message, timestamp 
FROM system_alerts 
WHERE severity = 'CRITICAL'
ORDER BY timestamp DESC;

-- Gate operation history
SELECT gate_number, action, trigger_reason, timestamp 
FROM gate_operations 
ORDER BY timestamp DESC;
```

## 🛡 Safety Features

- **Fail-Safe Design:** Emergency states override manual commands
- **Redundant Monitoring:** Both current and peak vibration levels checked
- **Evacuation Protocol:** Exit gates open during emergencies
- **Data Persistence:** All events logged for analysis
- **Remote Monitoring:** Cloud-based oversight capability

## 🔧 Troubleshooting

### Common Issues

**WiFi Connection Failed**
- Check SSID and password
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- Verify signal strength

**MQTT Connection Issues**
- Verify broker credentials and URL
- Check network connectivity
- Ensure SSL/TLS settings match broker requirements

**Sensor Calibration Problems**
- Keep system still during startup calibration
- Check sensor wiring and connections
- Verify 3.3V power supply to sensors

**Supabase Upload Errors**
- Verify Supabase URL and API key
- Check table schemas match code
- Ensure RLS policies allow insertions

##  Future Enhancements

- [ ] Machine learning for predictive maintenance
- [ ] Mobile app for remote monitoring
- [ ] Integration with emergency services
- [ ] Multi-bridge network management
- [ ] Weather data integration
- [ ] Camera-based visual monitoring

##  Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request
: [your-email@example.com]

##  Acknowledgments
 
- ESP32 community for excellent documentation
- Supabase team for robust backend services
- Arduino ecosystem for simplified IoT development
- Open source sensor libraries and examples

---

**⚠️ Safety Notice:** This system is designed for monitoring and alerting purposes. Always follow local safety regulations and have professional structural engineers assess bridge safety concerns.

