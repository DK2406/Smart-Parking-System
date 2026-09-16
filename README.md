# 🚗 Smart Parking Management System

An embedded **Smart Parking Management System** built using the **STM32F030C8T6** microcontroller, integrating **RFID authentication, IR-based vehicle detection, ultrasonic distance sensing, SPI communication, and UART-based event logging**.

The system provides controlled parking access, automatically assigns available parking slots to authorized users, tracks user entry/exit states, monitors parking occupancy, and generates structured parking-event logs for PC-based monitoring and analysis.

---

## 📌 Overview

Traditional parking systems often rely on manual monitoring to identify available spaces and manage access.

This project demonstrates an embedded approach to parking management where the **STM32F030C8T6** acts as the central controller.

### The system can:

* 🔐 Authenticate users using an **RC522 RFID reader**
* 🅿️ Automatically assign available parking slots
* 📡 Detect vehicles in **Slot 1 using an IR sensor**
* 📏 Detect vehicles in **Slot 2 using an HC-SR04 ultrasonic sensor**
* 👤 Maintain individual user parking states
* 🚫 Reject unauthorized RFID cards
* 🔄 Manage user entry and exit operations
* 📊 Transmit structured events through **USART1**
* 📁 Generate **CSV-compatible parking logs**
* ⚡ Perform ultrasonic measurements using timer-based microsecond timing

---

## ✨ Key Features

| Feature                  | Description                                     |
| ------------------------ | ----------------------------------------------- |
| 🔐 RFID Authentication   | Identifies registered parking users using RC522 |
| 🅿️ Automatic Allocation | Assigns the first available parking slot        |
| 📡 IR Detection          | Monitors occupancy of Parking Slot 1            |
| 📏 Ultrasonic Detection  | Measures Parking Slot 2 occupancy using HC-SR04 |
| 👤 User Tracking         | Maintains individual entry/exit states          |
| 🚫 Access Control        | Rejects unregistered RFID cards                 |
| 🔄 Entry/Exit Management | Same RFID card can be used to enter and exit    |
| 📊 UART Logging          | Sends structured system events through USART1   |
| 📁 CSV Logging           | Enables PC-side logging and Excel analysis      |
| ⚡ Timer Measurement      | TIM3 is used for ultrasonic echo timing         |
| 🔌 SPI Communication     | RC522 communicates through SPI1                 |

---

# 🧠 System Architecture

```text
                         ┌─────────────────────┐
                         │      RC522 RFID     │
                         │ User Authentication │
                         └──────────┬──────────┘
                                    │
                                   SPI
                                    │
                                    ▼
                         ┌─────────────────────┐
                         │                     │
                         │   STM32F030C8T6     │
                         │                     │
                         │ Parking Controller │
                         │                     │
                         └───────┬─────┬───────┘
                                 │     │
                      ┌──────────┘     └──────────┐
                      │                           │
                      ▼                           ▼
              ┌──────────────┐            ┌──────────────┐
              │  IR Sensor   │            │   HC-SR04    │
              │   Slot 1     │            │    Slot 2     │
              └──────────────┘            └──────────────┘
                      │                           │
                      ▼                           ▼
                Occupancy                    Distance
                 Detection                  Measurement

                              STM32
                                │
                              UART
                                │
                                ▼
                       ┌──────────────────┐
                       │   PC / Logger    │
                       │   CSV / Excel    │
                       └──────────────────┘
```

---

# 🔌 Hardware Components

| Component             |    Quantity | Purpose                      |
| --------------------- | ----------: | ---------------------------- |
| STM32F030C8T6         |           1 | Main microcontroller         |
| RC522 RFID Reader     |           1 | RFID authentication          |
| RFID Cards/Tags       |          2+ | User identification          |
| IR Sensor             |           1 | Slot 1 vehicle detection     |
| HC-SR04               |           1 | Slot 2 distance measurement  |
| USB-to-UART Interface |           1 | PC communication and logging |
| Breadboard / PCB      |           1 | Hardware integration         |
| Jumper Wires          | As required | Connections                  |
| Power Supply          |           1 | System power                 |

---

# 📍 Pin Configuration

## RC522 RFID — SPI1

| RC522 Pin | STM32F030C8T6 | Function    |
| --------- | ------------- | ----------- |
| SDA / SS  | PA4           | Chip Select |
| SCK       | SPI1 SCK      | SPI Clock   |
| MOSI      | SPI1 MOSI     | SPI Data    |
| MISO      | SPI1 MISO     | SPI Data    |
| RST       | PB0           | RFID Reset  |
| 3.3V      | 3.3V          | Power       |
| GND       | GND           | Ground      |

The RC522 communicates with the STM32 through **SPI1**, with **PA4** configured as the chip-select line and **PB0** used for reset.

---

## Parking Slot 1 — IR Sensor

| IR Pin | STM32 Pin | Function          |
| ------ | --------- | ----------------- |
| OUT    | PA0       | Vehicle detection |
| VCC    | Supply    | Power             |
| GND    | GND       | Ground            |

The firmware uses an active-low detection state:

```c
#define IR_DETECTED_STATE GPIO_PIN_RESET
```

Therefore:

```text
IR = LOW
   ↓
Vehicle Detected
   ↓
Slot 1 = OCCUPIED
```

```text
IR = HIGH
   ↓
No Vehicle Detected
   ↓
Slot 1 = FREE
```

---

## Parking Slot 2 — HC-SR04

| HC-SR04 Pin | STM32 Pin | Function       |
| ----------- | --------- | -------------- |
| TRIG        | PA1       | Trigger output |
| ECHO        | PB1       | Echo input     |
| VCC         | Supply    | Power          |
| GND         | GND       | Ground         |

The HC-SR04 is triggered using a **10 µs pulse**, while the echo duration is measured using **TIM3**.

### Occupancy Thresholds

| Distance | Slot Status             |
| -------- | ----------------------- |
| ≤ 13 cm  | 🟥 Occupied             |
| 13–18 cm | Previous state retained |
| ≥ 18 cm  | 🟩 Free                 |

Hysteresis is used between the occupied and free thresholds to reduce rapid state changes near the boundary.

> ⚠️ **Hardware Note:** Verify the HC-SR04 ECHO voltage before connecting it directly to the STM32F030C8T6. If the module outputs a 5 V signal, use an appropriate voltage divider or logic-level converter.

---

# 🔐 RFID Authentication

The current firmware contains two registered RFID users.

### Registered Users

| User | UID           |
| ---- | ------------- |
| DK   | `43 35 28 2A` |
| DEVA | `03 56 71 11` |

The UIDs are defined directly in the embedded firmware.

If a scanned UID does not match a registered user, the controller generates an:

```text
ACCESS DENIED
```

event.

> **Security Note:** UID-based authentication is suitable for this educational prototype but should not be considered a high-security access-control mechanism for production deployment.

---

# 🅿️ Parking Management Logic

The controller maintains the parking state of each registered user.

Conceptually:

```text
DK_inside
DK_slot

DEVA_inside
DEVA_slot
```

This allows the firmware to associate a user with the parking slot assigned during entry.

---

## 🚗 Entry Process

```text
             RFID Card
                 │
                 ▼
           Read RFID UID
                 │
                 ▼
       ┌────────────────────┐
       │ Authorized User?   │
       └─────────┬──────────┘
            NO   │   YES
             │   │
             ▼   ▼
      ACCESS DENIED
                 │
                 ▼
        Check User State
                 │
          Already Inside?
             /       \
           YES        NO
            │          │
            │          ▼
            │    Find Available Slot
            │          │
            │     ┌────┴────┐
            │     │         │
            │   None     Available
            │     │         │
            │     ▼         ▼
            │  PARKING    Assign Slot
            │    FULL         │
            │                 ▼
            │          User = INSIDE
            │
            ▼
         Exit Process
```

The firmware checks **Slot 1 first**, followed by **Slot 2**, when searching for an available slot.

---

# 🚪 Exit Process

When an already-registered user scans their RFID card again:

```text
RFID Scan
    │
    ▼
Identify User
    │
    ▼
User Already Inside?
    │
   YES
    │
    ▼
Retrieve Assigned Slot
    │
    ▼
Generate EXIT Event
    │
    ▼
Set User = OUTSIDE
    │
    ▼
Release Assigned Slot
```

The assigned slot is cleared after a successful exit operation.

---

# 📊 UART Event Logging

The system uses **USART1** for serial communication and structured parking-event logging.

Events follow the format:

```text
LOG,<tick_ms>,<event>,<user/uid>,<slot>,<status>
```

The timestamp is generated using:

```c
HAL_GetTick()
```

which provides the number of milliseconds elapsed since system startup.

### Example Output

```text
LOG,1250,SYSTEM,RFID,,OK
LOG,1360,STATUS,SLOT1,,FREE
LOG,1361,STATUS,SLOT2,,FREE
LOG,4520,CARD,, ,DETECTED
LOG,4521,ENTRY,DK,1,OK
LOG,4522,STATUS,SLOT1,,FULL
LOG,4523,STATUS,SLOT2,,FREE
```

These records can be captured by a PC-based application and stored as CSV data for further analysis using **Excel or other data-analysis tools**.

---

# 🔄 Complete System Workflow

```text
              ┌─────────────────┐
              │  System Startup │
              └────────┬────────┘
                       ▼
              Initialize STM32
                       │
                       ▼
                Initialize GPIO
                       │
                       ▼
                Initialize SPI1
                       │
                       ▼
               Initialize RC522
                       │
                       ▼
             Initialize USART1
                       │
                       ▼
                Initialize TIM3
                       │
                       ▼
              Initialize Sensors
                       │
                       ▼
             Monitor Slot Status
                       │
                       ▼
               Wait for RFID
                       │
              ┌────────┴────────┐
              │                 │
        RFID Detected       No RFID
              │                 │
              ▼                 │
          Read UID              │
              │                 │
              ▼                 │
       Verify RFID UID          │
          /       \             │
       Valid     Invalid        │
         │          │           │
         ▼          ▼           │
   Process User  Access Denied  │
         │                      │
         ▼                      │
   Entry / Exit                 │
         │                      │
         ▼                      │
   Update Slot State            │
         │                      │
         ▼                      │
     Generate Log               │
         │                      │
         └──────────┬───────────┘
                    ▼
              Continue Loop
```

---

# 💻 Firmware Architecture

The firmware is developed in **C using the STM32 HAL library**.

The implementation is organized into functional layers.

### RFID Driver

```c
RFID_Init()
RFID_WriteRegister()
RFID_ReadRegister()
RFID_Request()
RFID_Anticoll()
RFID_Halt()
```

Responsible for RC522 initialization, register access, card detection, and UID reading.

### Ultrasonic Driver

```c
Ultrasonic_GetDistance()
UpdateUltrasonicSlot()
```

Responsible for triggering the HC-SR04 and measuring echo duration.

### Parking Management

```c
Slot1_Occupied()
Slot2_Occupied()
SlotReserved()
FindAvailableSlot()
PrintParkingStatus()
ProcessCard()
```

Responsible for occupancy evaluation, slot allocation, user tracking, and entry/exit processing.

### UART Logging

```c
UART_Print()
UART_LogCSV()
```

Responsible for system messages and structured event transmission.

---

# ⚙️ STM32 Peripherals Used

| Peripheral | Purpose                           |
| ---------- | --------------------------------- |
| GPIO       | Sensor and control signals        |
| SPI1       | RC522 RFID communication          |
| USART1     | UART monitoring and event logging |
| TIM3       | Ultrasonic microsecond timing     |

---

# 🛠️ Software Requirements

### Development Environment

* **STM32CubeIDE**
* **STM32F0 MCU Package**
* **STM32 HAL**
* **C**
* UART terminal application
* Optional Python logging application
* Microsoft Excel / compatible spreadsheet software

---

# 🚀 Getting Started

## 1. Clone the Repository

```bash
git clone <your-repository-url>
cd Smart-Parking-System
```

## 2. Open the Project

Open the project in:

```text
STM32CubeIDE
```

## 3. Connect the Hardware

Connect the modules according to the pin configuration:

```text
RC522       → SPI1 + PA4 + PB0
IR Sensor   → PA0
HC-SR04     → PA1 + PB1
UART        → USART1
```

Ensure that all modules have a **common ground**.

## 4. Build the Firmware

In STM32CubeIDE:

```text
Project → Build Project
```

## 5. Flash the Microcontroller

Program the **STM32F030C8T6** using a compatible programmer/debugger.

## 6. Monitor UART Output

Connect the UART interface to the PC and open a serial terminal.

The system will display initialization messages, RFID events, parking status, and structured logs.

---

# 🧪 Example System Operation

## Scenario 1 — Authorized Entry

```text
DK scans RFID
      ↓
UID verified
      ↓
Slot 1 available
      ↓
Slot 1 assigned to DK
      ↓
ENTRY → DK → SLOT 1 → OK
```

---

## Scenario 2 — Parking Full

```text
Authorized user scans RFID
          ↓
No available slot
          ↓
PARKING FULL
```

---

## Scenario 3 — Authorized Exit

```text
DK scans RFID
      ↓
DK already inside
      ↓
Retrieve assigned slot
      ↓
EXIT → DK → SLOT 1 → OK
      ↓
Release Slot 1
```

---

## Scenario 4 — Unauthorized Card

```text
Unknown RFID
      ↓
UID not registered
      ↓
ACCESS DENIED
```

---

# 📁 Repository Structure

```text
Smart-Parking-System/
│
├── Core/
│   ├── Inc/
│   └── Src/
│
├── Drivers/
│
├── Circuit/
│   └── smart_parking_circuit.png
│
├── Documentation/
│   ├── Block_Diagram.png
│   └── System_Flowchart.png
│
├── Logger/
│   └── parking_logger.py
│
├── Screenshots/
│   ├── RFID_Test.jpg
│   ├── Slot_Detection.jpg
│   └── UART_Logging.jpg
│
├── README.md
└── LICENSE
```

---

# 📈 Future Improvements

The prototype can be extended with:

* 🌐 IoT-based remote parking monitoring
* 📱 Web/mobile parking dashboard
* 🅿️ Support for multiple parking slots
* 📺 OLED/LCD parking-status display
* 🚦 Automated entry/exit gate
* 🔔 Buzzer and LED indicators
* 🗄️ Database integration
* 📊 Real-time parking analytics
* 👥 Dynamic RFID user registration
* 🔒 Enhanced RFID authentication
* 📷 Camera-based vehicle verification
* 📡 Wireless communication between parking nodes

---

# 🎓 Learning Outcomes

This project provided hands-on experience in:

* STM32F0 microcontroller programming
* Embedded C development
* STM32 HAL peripheral configuration
* GPIO interfacing
* SPI communication
* RC522 RFID interfacing
* IR sensor interfacing
* HC-SR04 ultrasonic sensing
* Timer-based microsecond measurement
* UART communication
* State-based parking management
* Event-driven embedded logic
* Structured serial data logging
* Hardware-software integration

---

# 📌 Project Specifications

| Parameter                | Details             |
| ------------------------ | ------------------- |
| **Microcontroller**      | STM32F030C8T6       |
| **Programming Language** | C                   |
| **Framework**            | STM32 HAL           |
| **RFID Reader**          | RC522               |
| **Slot 1 Sensor**        | IR Sensor           |
| **Slot 2 Sensor**        | HC-SR04             |
| **RFID Interface**       | SPI1                |
| **PC Communication**     | USART1              |
| **Timer**                | TIM3                |
| **Data Format**          | CSV-compatible logs |
| **Development Tool**     | STM32CubeIDE        |

---

# 🚀 Project Highlights

```text
                 SMART PARKING SYSTEM

          ┌─────────────────────────────┐
          │      RFID Authentication    │
          └──────────────┬──────────────┘
                         +
          ┌──────────────┴──────────────┐
          │     IR Vehicle Detection    │
          └──────────────┬──────────────┘
                         +
          ┌──────────────┴──────────────┐
          │ Ultrasonic Distance Sensing │
          └──────────────┬──────────────┘
                         +
          ┌──────────────┴──────────────┐
          │   STM32 Embedded Processing  │
          └──────────────┬──────────────┘
                         +
          ┌──────────────┴──────────────┐
          │      UART Event Logging      │
          └──────────────┬──────────────┘
                         │
                         ▼
             PARKING MANAGEMENT SYSTEM
```

---

## 📜 License

This project is intended for **educational, experimental, and embedded-systems development purposes**.

Feel free to modify and extend the project according to your requirements.

---

## 👨‍💻 Author

**Dinesh Kumar M**

**Embedded Systems & IoT Developer**

Interested in embedded systems, IoT, hardware-software integration, and practical automation solutions.

---

⭐ **If you find this project useful, consider starring the repository.**
