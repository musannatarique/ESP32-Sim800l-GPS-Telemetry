# GPS Tracker: 

An optimized, real-time vehicular asset tracking solution leveraging hybrid connectivity (WiFi/GPRS via SIM800L) and high-precision telemetry positioning using the Neo-6M GPS module. The system handles hardware-aware power analytics and reports data stream payloads across secure TLS connections to HiveMQ Cloud.

## 🛠 Hardware Architecture

* **Processor Core**: TTGO T-Call (AM-036 Module with ESP32 and SIM800L GPRS Engine)
* **GNSS Unit**: Neo-6M GPS Module 
* **Power Profiling**: TP4056-based Lithium Ion Charging Module paired with a 1000µF high-capacity filtering capacitor to stabilize GPRS burst drops. 
* **10000mAh Battery**: To Provide Power 

### Pinout Mapping Configuration
| Peripheral | Pin (ESP32) | Hardware Function |
| :--- | :--- | :--- |
| **SIM800L RX** | `IO27` | UART AT Command Interface TX |
| **SIM800L TX** | `IO26` | UART AT Command Interface RX |
| **SIM800L RST** | `IO5` | Hardware Power Reset |
| **SIM800L PWKEY**| `IO4` | Power-on Activation Toggle |
| **Neo-6M RX** | `IO2` | Hardware Serial GNSS TX |
| **Neo-6M TX** | `IO25` | Hardware Serial GNSS RX |
| **Power ADC** | `IO34` | Precision Analog Voltage Divider |
| **Capacitor** | `JST Pin` | For Stabilize Power Supply |

---

## 🚀 Quick Start & Installation

### 1. Hardware Assembly
Upload the `schematic_diagram.png`  to view complete loop paths. Make sure the 1000µF capacitor sits physically close to the SIM800L power input pins to eliminate voltage dips when transmission flags trip.

### 2. Firmware Compilation
This codebase relies on PlatformIO IDE. 

```bash
# Navigate to the firmware path
cd firmware

# Compile code to verify dependencies
pio run

# Upload compiled binary directly over target USB-UART bridge
pio run --target upload
