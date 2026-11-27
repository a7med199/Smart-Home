# Nexus Home — Smart Home Simulator
### Qt5/Qt6 C++ OOP Desktop Application

---

## Project Structure

```
SmartHome/
├── CMakeLists.txt          ← CMake build file
├── SmartHome.pro           ← qmake build file
├── README.md
├── include/
│   ├── Device.h            ← Device base class
│   ├── Room.h              ← Room class (owns devices)
│   ├── SmartHome.h         ← Core controller / model
│   ├── Models.h            ← Scene, Automation, LogEntry structs
│   ├── Widgets.h           ← HeaderBar, SideBar widgets
│   ├── Pages.h             ← All 7 page widget declarations
│   └── MainWindow.h        ← Main application window
└── src/
    ├── main.cpp            ← Entry point
    ├── Device.cpp
    ├── Room.cpp
    ├── SmartHome.cpp
    ├── Widgets.cpp
    ├── Pages.cpp
    └── MainWindow.cpp
```

---

## OOP Architecture

| Class | Role |
|---|---|
| `Device` | Base class for all smart devices. Holds type, on/off state, value, and power draw. |
| `Room` | QObject that owns a collection of `Device` instances. Emits signals on env changes. |
| `SmartHome` | Central controller. Owns all rooms, scenes, automations, log. Drives simulation timer. |
| `MainWindow` | Top-level QMainWindow. Wires UI to model via Qt signals/slots. |
| `HeaderBar` | Custom header widget showing live stats and clock. |
| `SideBar` | Navigation widget with checkable QPushButton group. |
| `RoomsPage` | Grid view of room cards + device cards with sliders. |
| `ClimatePage` | Per-room thermostat controls + outdoor conditions. |
| `EnergyPage` | Live load, daily usage, solar, cost + bar chart. |
| `SecurityPage` | Alarm, locks, cameras, motion sensors + alerts. |
| `ScenesPage` | 6 clickable scene preset cards. |
| `AutomationPage` | Toggleable automation rule list. |
| `LogPage` | Timestamped activity log (most recent first). |

---

## Features

- **6 Rooms** — Living Room, Kitchen, Master Bed, Bathroom, Home Office, Garden
- **24 Smart Devices** — Lights, Thermostats, Locks, Cameras, Fans, TVs, Speakers, Sensors, Smart Plugs, Blinds
- **Live simulation** — Temperature & humidity drift on a 3-second timer
- **Real-time power metering** — Per-device watt calculation, totalled per room and globally
- **6 Scene presets** — Morning, Cinema, Dinner, Away, Night, Party (batch device control)
- **6 Automation rules** — Toggle on/off with visual feedback
- **Security panel** — Arm/disarm alarm, lock status, camera & sensor counts
- **Energy dashboard** — Load, daily kWh, solar generation, cost estimate, per-room bars
- **Climate control** — Per-room target temperature with +/− buttons
- **Activity log** — Up to 100 timestamped entries
- **Persist state** — Save/load home state to/from JSON (File menu)
- **Simulate events** — Inject motion alerts or smoke alarms (Simulate menu)
- **Dark sci-fi theme** — Deep navy + cyan accent stylesheet throughout

---

## Build Instructions

### Prerequisites
- Qt 5.15+ or Qt 6.x
- CMake 3.16+ **or** qmake
- C++17 capable compiler (MSVC 2019+, GCC 9+, Clang 10+)

---

### Option A — CMake (recommended)

```bash
# 1. Clone / extract the project
cd SmartHome

# 2. Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build build --config Release

# 4. Run
./build/SmartHome          # Linux / macOS
build\Release\SmartHome.exe  # Windows
```

---

### Option B — qmake

```bash
cd SmartHome

# Qt Creator: File → Open → SmartHome.pro
# Or from terminal:
qmake SmartHome.pro
make -j$(nproc)         # Linux / macOS
nmake                   # Windows (MSVC)
```

---

### Option C — Qt Creator (easiest)

1. Open **Qt Creator**
2. **File → Open File or Project…** → select `CMakeLists.txt` or `SmartHome.pro`
3. Configure the kit (Qt 5.15 or Qt 6)
4. Press **▶ Run**

---

## Notes

- The `QFlowLayout` include in `Pages.h` is referenced but the grid-based fallback is used
  in all pages for maximum Qt 5/6 compatibility.
- All device/room state is held in-memory; use **File → Save State** to persist to JSON.
- The simulation timer fires every 3 seconds and slightly drifts temperature & humidity.
