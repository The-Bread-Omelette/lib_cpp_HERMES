# Getting Started with Hermes

This guide will walk you through using the Hermes library in your SMT application.

---

## Table of Contents

1. [Installation](#installation)
2. [First Application](#first-application)
3. [Understanding Hermes Roles](#understanding-hermes-roles)
4. [Common Patterns](#common-patterns)
5. [Troubleshooting](#troubleshooting)

---

## Installation

### Step 1: Install Dependencies

**Windows (MinGW):**
```batch
# Download Boost 1.78.0
# https://archives.boost.io/release/1.78.0/source/boost_1_78_0.zip
# Extract and copy boost/ folder to: References\boost\

# Download Pugixml
# https://github.com/zeux/pugixml/releases
# Copy src/*.hpp and src/*.cpp to: References\pugixml\

# Verify MinGW installed
where g++
where mingw32-make
```

**Linux (Ubuntu/Debian):**
```bash
# Install from package manager
sudo apt-get update
sudo apt-get install build-essential libboost-all-dev libpugixml-dev

# Verify installation
g++ --version
```

### Step 2: Build Hermes Library

**Windows:**
```batch
cd hermes
setup_windows.bat
```

**Linux:**
```bash
cd hermes
chmod +x setup.sh
./setup.sh
```

### Step 3: Verify Installation

You should see:
```
[OK] Boost found
[OK] Pugixml found
[BUILD] Compiling Hermes library...
[OK] Hermes.dll created
[TEST] Running tests...
[SUCCESS] All tests passed
```

---

## First Application

### Hello Hermes - Minimal Connection Test

Create `hello_hermes.cpp`:

```cpp
#include <Hermes.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::cout << "=== Hermes Connection Test ===" << std::endl;
    
    // Create an upstream connection object
    Hermes::Upstream upstream;
    
    // Configure connection parameters
    Hermes::UpstreamSettings settings;
    settings._laneId = 1;                    // Lane/track ID
    settings._hostAddress = "192.168.1.10";  // Downstream machine IP
    settings._port = 50101;                  // Hermes default port
    
    std::cout << "Connecting to " << settings._hostAddress 
              << ":" << settings._port << std::endl;
    
    // Attempt connection
    upstream.Connect(1, settings);
    
    // Wait for connection to establish
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "Connection established!" << std::endl;
    
    // Send machine ready notification
    Hermes::NotificationData notification;
    notification._notificationCode = Hermes::ENotificationCode::eMACHINE_READY;
    notification._severity = Hermes::ESeverity::eINFO;
    notification._description = "Machine initialized";
    
    upstream.Signal(1, notification);
    std::cout << "Sent MACHINE_READY notification" << std::endl;
    
    // Disconnect gracefully
    upstream.Disconnect(1);
    std::cout << "Disconnected" << std::endl;
    
    return 0;
}
```

### Compile and Run

**Windows:**
```batch
g++ -std=c++17 -I src/include -L . -o hello_hermes.exe hello_hermes.cpp ^
    -lHermes -lws2_32 -lmswsock -static-libgcc -static-libstdc++

hello_hermes.exe
```

**Linux:**
```bash
g++ -std=c++17 -I src/include -L . -o hello_hermes hello_hermes.cpp \
    -lhermes -lboost_system -lboost_thread -lpugixml -lpthread \
    -Wl,-rpath,'$ORIGIN'

./hello_hermes
```

---

## Understanding Hermes Roles

Hermes defines three main communication roles:

### 1. Upstream (Sending Machine)

A machine that sends boards **downstream** to the next machine.

```cpp
Hermes::Upstream upstream;

// Connect to next machine
UpstreamSettings settings;
settings._hostAddress = "192.168.1.20";  // Next machine IP
settings._port = 50101;

upstream.Connect(1, settings);

// Signal board available
BoardAvailableData board;
board._boardId = "PCB-001";
board._topBarcode = "BARCODE-TOP";
board._lengthInMM = 250.0;
board._widthInMM = 200.0;

upstream.Signal(1, board);
```

**Use Case:** Loader, printer, placement machine sending to next station

### 2. Downstream (Receiving Machine)

A machine that receives boards from the **upstream** machine.

```cpp
Hermes::Downstream downstream;

// Listen for incoming boards
DownstreamSettings settings;
settings._port = 50101;  // Listen on this port

downstream.Enable(1, settings);

// Register callback for board arrival
downstream.RegisterCallback(1, 
    [](const BoardAvailableData& board) {
        std::cout << "Received board: " << board._boardId << std::endl;
    }
);
```

**Use Case:** Placement machine, reflow oven, AOI receiving from previous station

### 3. Bidirectional (Middle Machine)

Most machines in a line are both upstream AND downstream.

```cpp
// Receive from previous machine
Hermes::Downstream downstream;
downstream.Enable(1, downstreamSettings);

// Send to next machine
Hermes::Upstream upstream;
upstream.Connect(1, upstreamSettings);

// Process board and pass it along
downstream.RegisterCallback(1, 
    [&upstream](const BoardAvailableData& board) {
        // Process board here...
        
        // Send to next machine
        upstream.Signal(1, board);
    }
);
```

**Use Case:** Pick-and-place, inspection, coating machines in middle of line

---

## Common Patterns

### Pattern 1: Board Flow (Complete Transaction)

```cpp
#include <Hermes.h>
#include <iostream>

class HermesMachine {
private:
    Hermes::Upstream upstream_;
    Hermes::Downstream downstream_;
    
public:
    void Initialize() {
        // Setup downstream (receive from previous)
        DownstreamSettings downSettings;
        downSettings._port = 50101;
        downstream_.Enable(1, downSettings);
        
        // Setup upstream (send to next)
        UpstreamSettings upSettings;
        upSettings._hostAddress = "192.168.1.20";
        upSettings._port = 50101;
        upstream_.Connect(1, upSettings);
        
        // Register board arrival handler
        downstream_.RegisterCallback(1, 
            std::bind(&HermesMachine::OnBoardArrived, this, 
                     std::placeholders::_1)
        );
    }
    
    void OnBoardArrived(const BoardAvailableData& board) {
        std::cout << "Board arrived: " << board._boardId << std::endl;
        
        // Signal ready to receive
        MachineReadyData ready;
        ready._failedBoard = Hermes::EBoardQuality::eGOOD;
        downstream_.Signal(1, ready);
        
        // Simulate processing
        ProcessBoard(board);
        
        // Send to next machine
        upstream_.Signal(1, board);
        
        // Signal transport finished
        TransportFinishedData finished;
        finished._transferState = Hermes::ETransferState::eCOMPLETE;
        upstream_.Signal(1, finished);
    }
    
    void ProcessBoard(const BoardAvailableData& board) {
        // Your machine logic here
        std::cout << "Processing board..." << std::endl;
    }
};

int main() {
    HermesMachine machine;
    machine.Initialize();
    
    // Keep running
    std::this_thread::sleep_for(std::chrono::hours(24));
    return 0;
}
```

### Pattern 2: Error Handling and Notifications

```cpp
void SendErrorNotification(Hermes::Upstream& upstream, 
                          const std::string& error) {
    NotificationData notification;
    notification._notificationCode = 
        Hermes::ENotificationCode::eMACHINE_ERROR;
    notification._severity = Hermes::ESeverity::eERROR;
    notification._description = error;
    
    upstream.Signal(1, notification);
}

void HandleBoardDefect(Hermes::Upstream& upstream, 
                      const BoardAvailableData& board) {
    // Mark board as failed
    board._failedBoard = Hermes::EBoardQuality::eFAILED;
    
    // Send defect notification
    NotificationData notification;
    notification._notificationCode = 
        Hermes::ENotificationCode::eBOARD_DEFECT;
    notification._severity = Hermes::ESeverity::eWARNING;
    notification._description = "Board quality check failed";
    
    upstream.Signal(1, notification);
    upstream.Signal(1, board);
}
```

### Pattern 3: Work Order Management

```cpp
void RequestWorkOrder(Hermes::ConfigurationService& config) {
    GetWorkOrderDataRequest request;
    request._workOrderId = "WO-12345";
    
    config.Signal(1, request);
}

void SetWorkOrder(Hermes::ConfigurationService& config, 
                 const std::string& workOrderId) {
    SetWorkOrderDataRequest request;
    request._workOrderId = workOrderId;
    
    WorkOrderData workOrder;
    workOrder._workOrderIdentifier = workOrderId;
    workOrder._boardIdCreatedFrom = "TEMPLATE-001";
    workOrder._productTypeId = "PRODUCT-ABC";
    
    request._workOrder = workOrder;
    config.Signal(1, request);
}
```

### Pattern 4: Callbacks and Events

```cpp
class MyMachine {
public:
    void SetupCallbacks(Hermes::Downstream& downstream) {
        // Board available from upstream
        downstream.RegisterBoardAvailableCallback(1, 
            [this](const BoardAvailableData& data) {
                OnBoardAvailable(data);
            }
        );
        
        // Board forecast (advance notice)
        downstream.RegisterBoardForecastCallback(1,
            [this](const BoardForecastData& data) {
                OnBoardForecast(data);
            }
        );
        
        // Connection state changes
        downstream.RegisterConnectedCallback(1,
            [this]() {
                std::cout << "Upstream machine connected" << std::endl;
            }
        );
        
        downstream.RegisterDisconnectedCallback(1,
            [this]() {
                std::cout << "Upstream machine disconnected" << std::endl;
            }
        );
    }
    
private:
    void OnBoardAvailable(const BoardAvailableData& board) {
        std::cout << "Board: " << board._boardId 
                  << " (" << board._lengthInMM << "x" 
                  << board._widthInMM << "mm)" << std::endl;
    }
    
    void OnBoardForecast(const BoardForecastData& forecast) {
        std::cout << "Forecast: " << forecast._boardId 
                  << " arriving in " << forecast._timeUntilAvailable 
                  << "s" << std::endl;
    }
};
```

---

## Troubleshooting

### Connection Issues

**Problem:** `Connect()` fails or times out

**Solutions:**
```cpp
// 1. Verify network connectivity
ping 192.168.1.20

// 2. Check port is not blocked
telnet 192.168.1.20 50101

// 3. Verify other machine is listening
// (downstream must Enable() before upstream Connect())

// 4. Check firewall settings
# Windows: Allow Hermes.dll through Windows Firewall
# Linux: sudo ufw allow 50101/tcp
```

### Compilation Errors

**Problem:** `undefined reference to boost::asio::io_service`

**Solution:** Wrong Boost version! Use 1.66-1.78:
```bash
# Download correct version
wget https://archives.boost.io/release/1.78.0/source/boost_1_78_0.zip
```

**Problem:** `cannot find -lHermes`

**Solution:** Ensure `Hermes.dll` is in current directory or library path:
```batch
# Windows
dir Hermes.dll

# Linux
ls -l libhermes.so*
export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
```

### Runtime Errors

**Problem:** Application crashes with `DLL not found`

**Solution:** Copy `Hermes.dll` to executable directory:
```batch
copy Hermes.dll Release\
```

**Problem:** No data received from upstream

**Solution:** Verify callback registration happens BEFORE `Enable()`:
```cpp
// WRONG order
downstream.Enable(1, settings);
downstream.RegisterCallback(...);  // Too late!

// CORRECT order
downstream.RegisterCallback(...);
downstream.Enable(1, settings);
```

---

## Next Steps

1. **Read API Reference:** [API_REFERENCE.md](API_REFERENCE.md)
2. **Study Examples:** [EXAMPLES.md](EXAMPLES.md)
3. **Review Test Code:** `test/BoostTestHermes/*.cpp`
4. **IPC Standard:** [Download IPC-HERMES-9852](https://www.the-hermes-standard.info/)

---

## Quick Reference Card

```cpp
// UPSTREAM (Sending boards downstream)
Hermes::Upstream up;
up.Connect(laneId, settings);
up.Signal(laneId, boardData);

// DOWNSTREAM (Receiving boards from upstream)
Hermes::Downstream down;
down.Enable(laneId, settings);
down.RegisterCallback(laneId, callback);

// CONFIGURATION SERVICE
Hermes::ConfigurationService config;
config.Enable(laneId, settings);
config.Signal(laneId, request);

// COMMON DATA STRUCTURES
BoardAvailableData board;
NotificationData notification;
MachineReadyData ready;
TransportFinishedData finished;
```

---

**Need help?** Check [examples/](../examples/) or open an issue on GitHub.