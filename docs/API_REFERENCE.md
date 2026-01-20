# Hermes API Reference

Complete API documentation for the IPC-HERMES-9852 library.

---

## Table of Contents

- [Core Classes](#core-classes)
  - [Upstream](#upstream)
  - [Downstream](#downstream)
  - [ConfigurationService](#configurationservice)
- [Data Structures](#data-structures)
- [Enumerations](#enumerations)
- [Callbacks](#callbacks)

---

## Core Classes

### Upstream

The `Upstream` class manages connections to downstream machines (sending boards forward in the line).

#### Methods

##### `void Connect(unsigned laneId, const UpstreamSettings& settings)`

Establishes connection to a downstream machine.

**Parameters:**
- `laneId` - Unique identifier for this lane/track (1-based)
- `settings` - Connection configuration

**Example:**
```cpp
Hermes::Upstream upstream;

Hermes::UpstreamSettings settings;
settings._laneId = 1;
settings._hostAddress = "192.168.1.20";
settings._port = 50101;
settings._checkAliveResponseTimeout = 30.0;
settings._reconnectWaitTime = 10.0;

upstream.Connect(1, settings);
```

##### `void Disconnect(unsigned laneId)`

Closes connection to downstream machine.

**Parameters:**
- `laneId` - Lane identifier to disconnect

**Example:**
```cpp
upstream.Disconnect(1);
```

##### `void Signal(unsigned laneId, const T& data)`

Sends a message to the downstream machine. Type `T` can be:
- `BoardAvailableData` - Notify board is ready to transfer
- `BoardForecastData` - Advance notice of upcoming board
- `NotificationData` - Status, warning, or error message
- `MachineReadyData` - Ready to receive next board
- `TransportFinishedData` - Board transfer complete
- `CheckAliveData` - Heartbeat/keepalive

**Example:**
```cpp
// Signal board available
Hermes::BoardAvailableData board;
board._boardId = "PCB-12345";
board._topBarcode = "BARCODE-001";
board._lengthInMM = 250.0;
board._widthInMM = 200.0;
board._thickness = 1.6;
board._conveyorSpeed = 200.0;
board._topClearanceHeight = 15.0;
board._bottomClearanceHeight = 15.0;
board._weight = 0.5;
board._workOrderId = "WO-001";
board._failedBoard = Hermes::EBoardQuality::eGOOD;

upstream.Signal(1, board);

// Signal notification
Hermes::NotificationData notification;
notification._notificationCode = Hermes::ENotificationCode::eMACHINE_READY;
notification._severity = Hermes::ESeverity::eINFO;
notification._description = "Machine initialized successfully";

upstream.Signal(1, notification);
```

#### Settings Structure

```cpp
struct UpstreamSettings {
    unsigned _laneId;                          // Lane identifier
    std::string _machineId;                    // This machine's ID
    std::string _hostAddress;                  // Downstream IP address
    uint16_t _port;                            // Downstream port (default: 50101)
    double _checkAliveResponseTimeout;         // Timeout in seconds (default: 30.0)
    double _reconnectWaitTime;                 // Reconnect delay (default: 10.0)
};
```

---

### Downstream

The `Downstream` class listens for connections from upstream machines (receiving boards).

#### Methods

##### `void Enable(unsigned laneId, const DownstreamSettings& settings)`

Starts listening for upstream machine connections.

**Parameters:**
- `laneId` - Unique identifier for this lane/track
- `settings` - Listener configuration

**Example:**
```cpp
Hermes::Downstream downstream;

Hermes::DownstreamSettings settings;
settings._laneId = 1;
settings._machineId = "MACHINE-02";
settings._port = 50101;
settings._checkAliveResponseTimeout = 30.0;

downstream.Enable(1, settings);
```

##### `void Disable(unsigned laneId)`

Stops listening and disconnects any connected upstream machines.

**Parameters:**
- `laneId` - Lane identifier to disable

**Example:**
```cpp
downstream.Disable(1);
```

##### `void Signal(unsigned laneId, const T& data)`

Sends a message to the connected upstream machine. Type `T` can be:
- `MachineReadyData` - Ready to accept next board
- `RevokeMachineReadyData` - Cancel ready state
- `StartTransportData` - Begin board transfer
- `StopTransportData` - Stop board movement
- `NotificationData` - Status, warning, or error
- `CheckAliveData` - Heartbeat response

**Example:**
```cpp
// Signal ready to receive
Hermes::MachineReadyData ready;
ready._boardForecast = Hermes::EBoardForecast::eAVAILABLE;
ready._failedBoard = Hermes::EBoardQuality::eGOOD;

downstream.Signal(1, ready);
```

##### `void RegisterCallback(unsigned laneId, std::function<void(const T&)> callback)`

Registers a callback to be invoked when specific message types are received.

**Callback Types:**
- `BoardAvailableData` - Upstream has board ready
- `BoardForecastData` - Upcoming board notification
- `TransportFinishedData` - Board transfer completed
- `NotificationData` - Upstream status/error
- `CheckAliveData` - Heartbeat received

**Example:**
```cpp
// Register board arrival handler
downstream.RegisterCallback(1, 
    [](const Hermes::BoardAvailableData& board) {
        std::cout << "Board arrived: " << board._boardId << std::endl;
        std::cout << "  Size: " << board._lengthInMM << "x" 
                  << board._widthInMM << "mm" << std::endl;
        std::cout << "  Barcode: " << board._topBarcode << std::endl;
    }
);

// Register notification handler
downstream.RegisterCallback(1,
    [](const Hermes::NotificationData& notification) {
        std::cout << "Notification: " << notification._description << std::endl;
    }
);
```

##### `void RegisterConnectionCallbacks(unsigned laneId, std::function<void()> onConnect, std::function<void()> onDisconnect)`

Registers callbacks for connection state changes.

**Example:**
```cpp
downstream.RegisterConnectionCallbacks(1,
    []() {
        std::cout << "Upstream machine connected" << std::endl;
    },
    []() {
        std::cout << "Upstream machine disconnected" << std::endl;
    }
);
```

#### Settings Structure

```cpp
struct DownstreamSettings {
    unsigned _laneId;                          // Lane identifier
    std::string _machineId;                    // This machine's ID
    uint16_t _port;                            // Listen port (default: 50101)
    std::string _optionalClientAddress;        // Filter connections from this IP
    double _checkAliveResponseTimeout;         // Timeout in seconds (default: 30.0)
};
```

---

### ConfigurationService

Manages configuration data exchange and supervisory communication.

#### Methods

##### `void Enable(unsigned laneId, const ConfigurationServiceSettings& settings)`

Starts configuration service listener.

**Parameters:**
- `laneId` - Lane identifier
- `settings` - Service configuration

**Example:**
```cpp
Hermes::ConfigurationService config;

Hermes::ConfigurationServiceSettings settings;
settings._port = 1248;  // Default configuration port
settings._machineId = "MACHINE-01";

config.Enable(1, settings);
```

##### `void Disable(unsigned laneId)`

Stops configuration service.

##### `void Signal(unsigned laneId, const T& data)`

Sends configuration messages. Type `T` can be:
- `ServiceDescriptionData` - Machine capabilities
- `BoardAvailableData` - Board info response
- `SetConfigurationData` - Apply configuration
- `GetConfigurationData` - Request configuration
- `CurrentConfigurationData` - Configuration response

**Example:**
```cpp
// Send service description
Hermes::ServiceDescriptionData description;
description._machineId = "MACHINE-01";
description._laneId = 1;
description._supportedFeatures._boardForecast = true;
description._supportedFeatures._queryWorkOrderInfo = true;

config.Signal(1, description);

// Request work order data
Hermes::GetWorkOrderDataRequest request;
request._workOrderId = "WO-12345";

config.Signal(1, request);
```

#### Settings Structure

```cpp
struct ConfigurationServiceSettings {
    std::string _machineId;                    // Machine identifier
    uint16_t _port;                            // Listen port (default: 1248)
    double _checkAliveResponseTimeout;         // Timeout in seconds
};
```

---

## Data Structures

### BoardAvailableData

Describes a PCB board ready for transfer.

```cpp
struct BoardAvailableData {
    std::string _boardId;                      // Unique board identifier
    std::string _boardIdCreatedFrom;           // Template/parent board
    Hermes::EBoardQuality _failedBoard;        // Board quality status
    std::string _productTypeId;                // Product type
    Hermes::EFlipped _flipped;                 // Board orientation
    std::string _topBarcode;                   // Top side barcode
    std::string _bottomBarcode;                // Bottom side barcode
    double _lengthInMM;                        // Board length (mm)
    double _widthInMM;                         // Board width (mm)
    double _thickness;                         // Board thickness (mm)
    double _conveyorSpeed;                     // Transport speed (mm/s)
    double _topClearanceHeight;                // Top clearance (mm)
    double _bottomClearanceHeight;             // Bottom clearance (mm)
    double _weight;                            // Board weight (kg)
    std::string _workOrderId;                  // Associated work order
    std::string _batchId;                      // Batch identifier
};
```

### BoardForecastData

Advance notification of an upcoming board.

```cpp
struct BoardForecastData {
    std::string _boardId;                      // Board identifier
    std::string _boardIdCreatedFrom;           // Template identifier
    double _lengthInMM;                        // Board length
    double _widthInMM;                         // Board width
    double _thickness;                         // Board thickness
    double _conveyorSpeed;                     // Transport speed
    double _topClearanceHeight;                // Top clearance
    double _bottomClearanceHeight;             // Bottom clearance
    double _weight;                            // Board weight
    std::string _workOrderId;                  // Work order
    std::string _batchId;                      // Batch ID
    double _timeUntilAvailable;                // ETA in seconds
};
```

### MachineReadyData

Signals downstream machine is ready to accept a board.

```cpp
struct MachineReadyData {
    Hermes::EBoardQuality _failedBoard;        // Reject if board quality matches
    Hermes::EBoardForecast _boardForecast;     // Board forecast capability
    std::string _forecastId;                   // Expected board ID
};
```

### NotificationData

Status, warning, or error message.

```cpp
struct NotificationData {
    Hermes::ENotificationCode _notificationCode;  // Type of notification
    Hermes::ESeverity _severity;                  // Importance level
    std::string _description;                     // Human-readable message
};
```

### TransportFinishedData

Confirms board transfer completion.

```cpp
struct TransportFinishedData {
    Hermes::ETransferState _transferState;     // Success/failure status
    std::string _boardId;                      // Transferred board ID
};
```

---

## Enumerations

### EBoardQuality

Board quality/status classification.

```cpp
enum class EBoardQuality {
    eUNKNOWN = 0,        // Quality not determined
    eGOOD = 1,           // Board passed all checks
    eFAILED = 2          // Board failed quality check
};
```

### EFlipped

Board orientation.

```cpp
enum class EFlipped {
    eNOT_FLIPPED = 0,    // Normal orientation
    eFLIPPED = 1         // Board is upside down
};
```

### ETransferState

Board transfer result.

```cpp
enum class ETransferState {
    eNOT_STARTED = 0,    // Transfer not initiated
    eINCOMPLETE = 1,     // Transfer failed/aborted
    eCOMPLETE = 2        // Successfully transferred
};
```

### ENotificationCode

Predefined notification types.

```cpp
enum class ENotificationCode {
    eUNSPECIFIED = 0,
    eMACHINE_READY = 1,
    eBOARD_AVAILABLE = 2,
    eBOARD_QUALITY_CHANGED = 3,
    eCONVEYOR_SPEED_CHANGED = 4,
    eMACHINE_PAUSED = 5,
    eMACHINE_STOPPED = 6,
    eCONNECTION_ERROR = 7,
    eBOARD_LOST = 8,
    eBOARD_DEFECT = 9
    // ... see Hermes.h for complete list
};
```

### ESeverity

Notification importance level.

```cpp
enum class ESeverity {
    eFATAL = 1,          // Critical error, stop line
    eERROR = 2,          // Error requiring attention
    eWARNING = 3,        // Warning, can continue
    eINFO = 4            // Informational message
};
```

---

## Callbacks

### Standard Callback Pattern

All callbacks follow this pattern:

```cpp
downstream.RegisterCallback(laneId, 
    [](const MessageType& data) {
        // Handle message
    }
);
```

### Available Callbacks

#### Downstream Callbacks

```cpp
// Board available from upstream
RegisterCallback<BoardAvailableData>(laneId, callback);

// Board forecast notification
RegisterCallback<BoardForecastData>(laneId, callback);

// Transport finished
RegisterCallback<TransportFinishedData>(laneId, callback);

// Notifications from upstream
RegisterCallback<NotificationData>(laneId, callback);

// Connection state
RegisterConnectionCallbacks(laneId, onConnect, onDisconnect);
```

#### Upstream Callbacks

```cpp
// Machine ready signal from downstream
RegisterCallback<MachineReadyData>(laneId, callback);

// Start/stop transport signals
RegisterCallback<StartTransportData>(laneId, callback);
RegisterCallback<StopTransportData>(laneId, callback);

// Notifications from downstream
RegisterCallback<NotificationData>(laneId, callback);

// Connection state
RegisterConnectionCallbacks(laneId, onConnect, onDisconnect);
```

#### Configuration Service Callbacks

```cpp
// Configuration requests
RegisterCallback<GetConfigurationRequest>(laneId, callback);
RegisterCallback<SetConfigurationRequest>(laneId, callback);

// Work order queries
RegisterCallback<GetWorkOrderDataRequest>(laneId, callback);
RegisterCallback<SetWorkOrderDataRequest>(laneId, callback);

// Board info requests
RegisterCallback<QueryBoardInfoRequest>(laneId, callback);
```

---

## Complete Example

```cpp
#include <Hermes.h>
#include <iostream>
#include <thread>

class SMTMachine {
private:
    Hermes::Upstream upstream_;
    Hermes::Downstream downstream_;
    
public:
    void Initialize() {
        SetupDownstream();
        SetupUpstream();
    }
    
    void SetupDownstream() {
        // Configure receiving from previous machine
        Hermes::DownstreamSettings settings;
        settings._laneId = 1;
        settings._machineId = "MACHINE-02";
        settings._port = 50101;
        
        // Register callbacks BEFORE enabling
        downstream_.RegisterCallback(1,
            std::bind(&SMTMachine::OnBoardAvailable, this,
                     std::placeholders::_1)
        );
        
        downstream_.RegisterCallback(1,
            std::bind(&SMTMachine::OnNotification, this,
                     std::placeholders::_1)
        );
        
        downstream_.RegisterConnectionCallbacks(1,
            []() { std::cout << "Upstream connected" << std::endl; },
            []() { std::cout << "Upstream disconnected" << std::endl; }
        );
        
        // Start listening
        downstream_.Enable(1, settings);
    }
    
    void SetupUpstream() {
        // Configure sending to next machine
        Hermes::UpstreamSettings settings;
        settings._laneId = 1;
        settings._machineId = "MACHINE-02";
        settings._hostAddress = "192.168.1.30";
        settings._port = 50101;
        
        upstream_.Connect(1, settings);
    }
    
    void OnBoardAvailable(const Hermes::BoardAvailableData& board) {
        std::cout << "=== Board Arrived ===" << std::endl;
        std::cout << "ID: " << board._boardId << std::endl;
        std::cout << "Size: " << board._lengthInMM << "x" 
                  << board._widthInMM << "mm" << std::endl;
        
        // Signal ready to receive
        Hermes::MachineReadyData ready;
        ready._failedBoard = Hermes::EBoardQuality::eGOOD;
        downstream_.Signal(1, ready);
        
        // Process board
        ProcessBoard(board);
        
        // Forward to next machine
        upstream_.Signal(1, board);
        
        // Confirm transport finished
        Hermes::TransportFinishedData finished;
        finished._transferState = Hermes::ETransferState::eCOMPLETE;
        finished._boardId = board._boardId;
        upstream_.Signal(1, finished);
    }
    
    void OnNotification(const Hermes::NotificationData& notification) {
        std::cout << "Notification [" << static_cast<int>(notification._severity) 
                  << "]: " << notification._description << std::endl;
    }
    
    void ProcessBoard(const Hermes::BoardAvailableData& board) {
        // Simulate processing time
        std::cout << "Processing board..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "Processing complete" << std::endl;
    }
    
    void Shutdown() {
        downstream_.Disable(1);
        upstream_.Disconnect(1);
    }
};

int main() {
    SMTMachine machine;
    machine.Initialize();
    
    // Run for 1 hour
    std::this_thread::sleep_for(std::chrono::hours(1));
    
    machine.Shutdown();
    return 0;
}
```

---

## Error Handling

The library uses exceptions for error conditions:

```cpp
try {
    upstream.Connect(1, settings);
} catch (const std::exception& e) {
    std::cerr << "Connection failed: " << e.what() << std::endl;
}
```

Common exceptions:
- Network connection failures
- Invalid configuration
- Protocol violations
- Timeout errors

---

## Thread Safety

- Each lane operates independently and is thread-safe
- Callbacks are invoked on internal thread - keep handlers quick
- For long operations, dispatch to worker thread from callback

```cpp
downstream.RegisterCallback(1,
    [this](const BoardAvailableData& board) {
        // Quick handling in callback
        std::thread([this, board]() {
            // Long operation in separate thread
            ProcessBoard(board);
        }).detach();
    }
);
```

---

**See also:**
- [Getting Started Guide](GETTING_STARTED.md)
- [Examples](EXAMPLES.md)
- [IPC-HERMES-9852 Standard](https://www.the-hermes-standard.info/)