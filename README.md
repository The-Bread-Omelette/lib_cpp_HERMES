# Hermes C++ Library — Complete Reference

**Protocol:** The Hermes Standard — vendor-independent machine-to-machine communication for SMT assembly lines  
**License:** Apache 2.0  
**Requires:** C++17, Boost 1.66+, CMake 3.15+

---

## Table of Contents

1. [What is Hermes](#1-what-is-hermes)
2. [Architecture overview](#2-architecture-overview)
3. [Building the library](#3-building-the-library)
4. [Header map — what to include](#4-header-map)
5. [Core types reference](#5-core-types-reference)
6. [Modern C++ API — HermesModern.hpp](#6-modern-c-api)
7. [Low-level C++ API — Hermes.hpp](#7-low-level-c-api)
8. [Serialization API — HermesSerialization.hpp](#8-serialization-api)
9. [Configuration service](#9-configuration-service)
10. [Vertical interface](#10-vertical-interface)
11. [Complete examples](#11-complete-examples)
12. [Bugs fixed in this version](#12-bugs-fixed)

---

## 1. What is Hermes

Hermes is an open TCP/IP + XML protocol that connects machines in an electronics assembly line. Each machine has an **Upstream** port (faces the previous machine) and a **Downstream** port (faces the next machine). PCB boards flow from Upstream to Downstream along the lane.

```
[Machine A] --downstream:50100--> [Machine B] --downstream:50101--> [Machine C]
             <--upstream:50100---              <--upstream:50101---
```

The Downstream machine **listens** on a port. The Upstream machine **connects** to it. So:

- `Hermes::Downstream` / `Modern::Downstream` — **server role**, listens for incoming connections
- `Hermes::Upstream` / `Modern::Upstream` — **client role**, connects to the downstream machine

---

## 2. Architecture overview

```
Your application
      |
      |--- HermesModern.hpp     (std::function callbacks — recommended)
      |--- Hermes.hpp           (virtual interface callbacks — advanced)
      |--- HermesSerialization  (XML serialize/deserialize)
      |
      +---> Hermes C API (Hermes.h / HermesData.h)
      |         compiled into libhermes.so
      |
      +---> Boost.Asio (networking)
      +---> pugixml (XML parsing)
```

### Message flow — horizontal (machine to machine)

```
Downstream machine                    Upstream machine
(Modern::Downstream)                  (Modern::Upstream)
        |                                     |
        |<-- TCP connect -------------------- |
        |                                     |
        |<-- ServiceDescription ------------- |  (Upstream identifies itself)
        |--> ServiceDescription -----------> |  (Downstream identifies itself)
        |                                     |
        |<-- MachineReady ------------------- |  (Upstream ready to receive)
        |--> BoardAvailable --------------> |  (Downstream has a board)
        |                                     |
        |<-- StartTransport ----------------- |  (Upstream says: send it)
        |  [board physically moves]           |
        |--> TransportFinished -----------> |  (Downstream confirms done)
        |                                     |
        |<-- StopTransport  ---------------- |  (Upstream confirms received)
```

### Who receives what

| You create        | You listen for (receive)                                          | You send                                              |
|-------------------|-------------------------------------------------------------------|-------------------------------------------------------|
| `Modern::Downstream` | `ServiceDescription`, `MachineReady`, `RevokeMachineReady`, `StartTransport`, `StopTransport`, `QueryBoardInfo` | `ServiceDescription`, `BoardAvailable`, `RevokeBoardAvailable`, `TransportFinished`, `BoardForecast`, `SendBoardInfo` |
| `Modern::Upstream`   | `ServiceDescription`, `BoardAvailable`, `RevokeBoardAvailable`, `TransportFinished`, `BoardForecast`, `SendBoardInfo` | `ServiceDescription`, `MachineReady`, `RevokeMachineReady`, `StartTransport`, `StopTransport`, `QueryBoardInfo` |

---

## 3. Building the library

### Prerequisites

```bash
# Debian / Ubuntu / Raspberry Pi OS
sudo apt install -y g++ cmake libboost-all-dev

# macOS
brew install cmake boost

# Windows
# Install Boost via vcpkg: vcpkg install boost
```

### Build

```bash
git clone https://github.com/hermes-org/lib_cpp.git
cd lib_cpp
mkdir build && cd build
cmake ..
make -j4
```

This produces `build/src/Hermes/libhermes.so` (Linux/macOS) or `hermes.dll` (Windows).

### Compile your application

```bash
g++ -std=c++17 -o myapp myapp.cpp \
    -I./src/include \
    -L./build/src/Hermes -lhermes \
    -lboost_system -lpthread

# Run (Linux — tell the linker where to find the .so)
LD_LIBRARY_PATH=./build/src/Hermes ./myapp
```

---

## 4. Header map

| Header | What it gives you | When to include it |
|--------|------------------|--------------------|
| `HermesModern.hpp` | `Modern::Downstream`, `Modern::Upstream` with std::function callbacks | **Start here. Recommended for all new code.** |
| `Hermes.hpp` | `Hermes::Downstream`, `Hermes::Upstream`, virtual callback interfaces | When you need session IDs, raw XML, or fine-grained control |
| `HermesData.hpp` | All C++ data structs, enums, settings structs | Included automatically by the above |
| `HermesSerialization.hpp` | `ToXml()`, `FromXml<T>()` | When you need to inspect or log raw XML messages |
| `HermesOptional.hpp` | `Hermes::Optional<T>` = `std::optional<T>` | Included automatically |
| `HermesStringView.hpp` | `Hermes::StringView` = `std::string_view` | Included automatically |

---

## 5. Core types reference

### EState — connection state machine

```cpp
enum class EState {
    eNOT_CONNECTED,               // No TCP connection
    eSOCKET_CONNECTED,            // TCP connected, waiting for ServiceDescription
    eSERVICE_DESCRIPTION_DOWNSTREAM, // ServiceDescription exchanged
    eNOT_AVAILABLE_NOT_READY,     // Connected, neither side ready
    eBOARD_AVAILABLE,             // Downstream has a board waiting
    eMACHINE_READY,               // Upstream is ready to receive
    eAVAILABLE_AND_READY,         // Both sides ready — transport can start
    eTRANSPORTING,                // Board is moving
    eTRANSPORT_STOPPED,           // Transport paused
    eTRANSPORT_FINISHED,          // Board delivered
    eDISCONNECTED                 // Connection closed
};
```

### ETraceType — log levels

```cpp
enum class ETraceType {
    eSENT,      // Raw XML sent over the wire
    eRECEIVED,  // Raw XML received
    eDEBUG,
    eINFO,
    eWARNING,
    eERROR
};
```

### Error

```cpp
struct Error {
    EErrorCode  m_code;   // eSUCCESS, eNETWORK_ERROR, eTIMEOUT, etc.
    std::string m_text;   // Human-readable description

    explicit operator bool() const; // true if error (code != eSUCCESS)
};
```

### ConnectionInfo

```cpp
struct ConnectionInfo {
    std::string m_address;   // Remote IP address
    uint16_t    m_port;      // Remote port
    std::string m_hostName;  // Remote hostname (if resolvable)
};
```

### ServiceDescriptionData

Exchanged in both directions at connection start. Identifies the machine.

```cpp
struct ServiceDescriptionData {
    std::string            m_machineId;           // Required. Your machine identifier.
    uint32_t               m_laneId;              // Required. Which lane (1-based).
    Optional<std::string>  m_optionalInterfaceId; // Optional. Interface label.
    std::string            m_version;             // Protocol version string.
};
```

### BoardAvailableData

Sent by Downstream to signal a board is ready for transfer.

```cpp
struct BoardAvailableData {
    std::string            m_boardId;               // Required. Unique board identifier.
    std::string            m_boardIdCreatedBy;       // Required. Machine that created the ID.
    EBoardQuality          m_failedBoard;            // eANY, eGOOD, eBAD
    Optional<std::string>  m_optionalProductTypeId;
    EFlippedBoard          m_flippedBoard;           // eSIDE_UP_IS_UNKNOWN, eTOP_SIDE_IS_UP, eBOTTOM_SIDE_IS_UP
    Optional<std::string>  m_optionalTopBarcode;
    Optional<std::string>  m_optionalBottomBarcode;
    Optional<double>       m_length;                // mm
    Optional<double>       m_width;                 // mm
    Optional<double>       m_thickness;             // mm
    Optional<double>       m_conveyorSpeed;         // mm/s
    Optional<double>       m_topClearanceHeight;    // mm
    Optional<double>       m_bottomClearanceHeight; // mm
    Optional<double>       m_weight;                // grams
    Optional<std::string>  m_optionalWorkOrderId;
    Optional<std::string>  m_optionalBatchId;
    Optional<uint16_t>     m_optionalRoute;
    Optional<uint16_t>     m_optionalAction;
    std::vector<SubBoard>  m_subBoards;
};
```

### MachineReadyData

Sent by Upstream to signal it is ready to receive a board.

```cpp
struct MachineReadyData {
    EBoardQuality          m_failedBoard;            // What quality board it can accept
    Optional<std::string>  m_optionalForecastId;
    Optional<std::string>  m_optionalBoardId;
    Optional<std::string>  m_optionalProductTypeId;
    Optional<EFlippedBoard> m_optionalFlippedBoard;
    Optional<std::string>  m_optionalTopBarcode;
    Optional<std::string>  m_optionalBottomBarcode;
    Optional<double>       m_length;
    Optional<double>       m_width;
    Optional<double>       m_thickness;
    Optional<double>       m_conveyorSpeed;
    Optional<double>       m_topClearanceHeight;
    Optional<double>       m_bottomClearanceHeight;
    Optional<double>       m_weight;
    Optional<std::string>  m_optionalWorkOrderId;
    Optional<std::string>  m_optionalBatchId;
};
```

### StartTransportData / StopTransportData / TransportFinishedData

```cpp
struct StartTransportData {
    std::string      m_boardId;         // Which board to transport
    Optional<double> m_conveyorSpeed;   // mm/s, optional override
};

struct StopTransportData {
    ETransferState  m_transferState;  // eCOMPLETE, eINCOMPLETE, eNOT_STARTED
    std::string     m_boardId;
};

struct TransportFinishedData {
    ETransferState  m_transferState;
    std::string     m_boardId;
};
```

### NotificationData

Sent by either side to report errors or status.

```cpp
struct NotificationData {
    ENotificationCode  m_notificationCode;  // ePROTOCOL_ERROR, eMACHINE_SHUTDOWN, etc.
    ESeverity          m_severity;          // eFATAL, eERROR, eWARNING, eINFO
    std::string        m_description;       // Human-readable message
};
```

### Settings structs

```cpp
struct DownstreamSettings {
    std::string            m_machineId;                        // Required
    Optional<std::string>  m_optionalClientAddress;            // Restrict to one client IP
    uint16_t               m_port{0};                         // 0 = use cBASE_PORT (50100)
    double                 m_checkAlivePeriodInSeconds{60};
    double                 m_reconnectWaitTimeInSeconds{10};
    ECheckAliveResponseMode m_checkAliveResponseMode{eAUTO};
    ECheckState            m_checkState{eSEND_AND_RECEIVE};
};

struct UpstreamSettings {
    std::string  m_machineId;                        // Required
    std::string  m_hostAddress;                      // Required. IP of the downstream machine.
    uint16_t     m_port{0};                         // 0 = use cBASE_PORT (50100)
    double       m_checkAlivePeriodInSeconds{60};
    double       m_reconnectWaitTimeInSeconds{10};
    ECheckAliveResponseMode m_checkAliveResponseMode{eAUTO};
    ECheckState  m_checkState{eSEND_AND_RECEIVE};
};
```

---

## 6. Modern C++ API

`HermesModern.hpp` provides `Modern::Downstream` and `Modern::Upstream`. These wrap the low-level virtual callback interface with `std::function` callbacks. This is the recommended API for new code.

### Modern::Downstream

```cpp
Hermes::Modern::Downstream ds(unsigned laneId);
```

**Register callbacks:**

```cpp
// Connection events — sessionId identifies the active connection
ds.RegisterConnectedCallback(   [](unsigned sessionId, const ConnectionInfo& info) { ... });
ds.RegisterDisconnectedCallback([](unsigned sessionId, const Error& err) { ... });
ds.RegisterStateChangeCallback( [](unsigned sessionId, EState state) { ... });
ds.RegisterTraceCallback(       [](unsigned sessionId, ETraceType type, StringView msg) { ... });

// Messages received FROM the Upstream machine
ds.RegisterServiceDescriptionCallback( [](unsigned sessionId, EState, const ServiceDescriptionData& d) { ... });
ds.RegisterMachineReadyCallback(       [](unsigned sessionId, EState, const MachineReadyData& d) { ... });
ds.RegisterRevokeMachineReadyCallback( [](unsigned sessionId, EState, const RevokeMachineReadyData& d) { ... });
ds.RegisterStartTransportCallback(     [](unsigned sessionId, EState, const StartTransportData& d) { ... });
ds.RegisterStopTransportCallback(      [](unsigned sessionId, EState, const StopTransportData& d) { ... });
ds.RegisterQueryBoardInfoCallback(     [](unsigned sessionId, const QueryBoardInfoData& d) { ... });

// Auxiliary
ds.RegisterNotificationCallback([](unsigned sessionId, const NotificationData& d) { ... });
ds.RegisterCheckAliveCallback(  [](unsigned sessionId, const CheckAliveData& d) { ... });
ds.RegisterCommandCallback(     [](unsigned sessionId, const CommandData& d) { ... });
```

**Lifecycle:**

```cpp
DownstreamSettings settings;
settings.m_machineId = "MyMachine";
settings.m_port      = 50100;

ds.Enable(settings);   // starts listening, launches network thread
// ... application runs ...
ds.Stop();             // stops network thread, closes connection
```

**Send messages to the Upstream:**

```cpp
// Must call from inside Post() or from within a callback — both are on the network thread
ds.Post([&ds, sessionId]() {
    BoardAvailableData board;
    board.m_boardId          = "BOARD-001";
    board.m_boardIdCreatedBy = "MyMachine";
    board.m_failedBoard      = EBoardQuality::eGOOD;
    board.m_flippedBoard     = EFlippedBoard::eTOP_SIDE_IS_UP;
    ds.Signal(sessionId, board);
});
```

### Modern::Upstream

```cpp
Hermes::Modern::Upstream us(unsigned laneId);
```

**Register callbacks:**

```cpp
us.RegisterConnectedCallback(   [](unsigned sessionId, const ConnectionInfo& info) { ... });
us.RegisterDisconnectedCallback([](unsigned sessionId, const Error& err) { ... });
us.RegisterStateChangeCallback( [](unsigned sessionId, EState state) { ... });
us.RegisterTraceCallback(       [](unsigned sessionId, ETraceType type, StringView msg) { ... });

// Messages received FROM the Downstream machine
us.RegisterServiceDescriptionCallback(   [](unsigned sessionId, EState, const ServiceDescriptionData& d) { ... });
us.RegisterBoardAvailableCallback(       [](unsigned sessionId, EState, const BoardAvailableData& d) { ... });
us.RegisterRevokeBoardAvailableCallback( [](unsigned sessionId, EState, const RevokeBoardAvailableData& d) { ... });
us.RegisterTransportFinishedCallback(    [](unsigned sessionId, EState, const TransportFinishedData& d) { ... });
us.RegisterBoardForecastCallback(        [](unsigned sessionId, EState, const BoardForecastData& d) { ... });
us.RegisterSendBoardInfoCallback(        [](unsigned sessionId, const SendBoardInfoData& d) { ... });

// Auxiliary
us.RegisterNotificationCallback([](unsigned sessionId, const NotificationData& d) { ... });
us.RegisterCheckAliveCallback(  [](unsigned sessionId, const CheckAliveData& d) { ... });
us.RegisterCommandCallback(     [](unsigned sessionId, const CommandData& d) { ... });
```

**Lifecycle:**

```cpp
UpstreamSettings settings;
settings.m_machineId   = "MyMachine";
settings.m_hostAddress = "192.168.1.2";  // IP of the Downstream machine
settings.m_port        = 50100;

us.Enable(settings);
// ...
us.Stop();
```

**Send messages to the Downstream:**

```cpp
us.Post([&us, sessionId]() {
    MachineReadyData ready;
    ready.m_failedBoard = EBoardQuality::eANY;
    us.Signal(sessionId, ready);
});
```

---

## 7. Low-level C++ API

`Hermes.hpp` provides `Hermes::Downstream` and `Hermes::Upstream` with virtual callback interfaces. Use this when you need the session ID in every callback, raw XML signals, or multiple sessions.

### Implementing IDownstreamCallback

```cpp
struct MyDownstreamHandler : Hermes::IDownstreamCallback
{
    // Pure virtuals — must implement all of these:
    void OnConnected(unsigned sessionId, EState, const ConnectionInfo&) override;
    void OnDisconnected(unsigned sessionId, EState, const Error&) override;
    void OnState(unsigned sessionId, EState) override;
    void OnTrace(unsigned sessionId, ETraceType, StringView) override;

    // Messages from Upstream — pure virtual:
    void On(unsigned sessionId, EState, const ServiceDescriptionData&) override;
    void On(unsigned sessionId, EState, const MachineReadyData&) override;
    void On(unsigned sessionId, EState, const RevokeMachineReadyData&) override;
    void On(unsigned sessionId, EState, const StartTransportData&) override;
    void On(unsigned sessionId, EState, const StopTransportData&) override;
    void On(unsigned sessionId, const NotificationData&) override;
    void On(unsigned sessionId, const CommandData&) override;

    // Optional — have default empty implementations:
    void On(unsigned sessionId, const CheckAliveData&) override {}
    void On(unsigned sessionId, const QueryBoardInfoData&) override {}
};
```

**Usage:**

```cpp
MyDownstreamHandler handler;
Hermes::Downstream downstream(1, handler);  // lane 1

DownstreamSettings settings;
settings.m_machineId = "Machine-A";
settings.m_port      = 50100;
downstream.Enable(settings);
downstream.Run();   // blocks until Stop() is called from another thread
```

### Implementing IUpstreamCallback

```cpp
struct MyUpstreamHandler : Hermes::IUpstreamCallback
{
    void OnConnected(unsigned sessionId, EState, const ConnectionInfo&) override;
    void OnDisconnected(unsigned sessionId, EState, const Error&) override;
    void OnState(unsigned sessionId, EState) override;
    void OnTrace(unsigned sessionId, ETraceType, StringView) override;

    // Messages from Downstream — pure virtual:
    void On(unsigned sessionId, EState, const ServiceDescriptionData&) override;
    void On(unsigned sessionId, EState, const BoardAvailableData&) override;
    void On(unsigned sessionId, EState, const RevokeBoardAvailableData&) override;
    void On(unsigned sessionId, EState, const TransportFinishedData&) override;
    void On(unsigned sessionId, const NotificationData&) override;
    void On(unsigned sessionId, const CommandData&) override;

    // Optional:
    void On(unsigned sessionId, const CheckAliveData&) override {}
    void On(unsigned sessionId, EState, const BoardForecastData&) override {}
    void On(unsigned sessionId, const SendBoardInfoData&) override {}
};
```

### Sending signals from within callbacks

Always use `Post()` to send signals — it schedules the send on the Hermes network thread safely:

```cpp
void On(unsigned sessionId, EState, const MachineReadyData&) override
{
    // DO NOT call Signal() directly here in the virtual callback API.
    // Use Post() to dispatch onto the network thread:
    m_downstream->Post([this, sessionId]() {
        BoardAvailableData board;
        board.m_boardId          = "B-001";
        board.m_boardIdCreatedBy = "MachineA";
        board.m_failedBoard      = EBoardQuality::eGOOD;
        board.m_flippedBoard     = EFlippedBoard::eTOP_SIDE_IS_UP;
        m_downstream->Signal(sessionId, board);
    });
}
```

> In `Modern::Downstream` / `Modern::Upstream`, callbacks already run on the network thread, so you can call `Signal()` directly inside them.

---

## 8. Serialization API

`HermesSerialization.hpp` lets you convert any data struct to XML and back. Useful for logging, testing, or debugging.

```cpp
#include "HermesSerialization.hpp"

// Serialize to XML string
BoardAvailableData board;
board.m_boardId          = "BOARD-001";
board.m_boardIdCreatedBy = "Machine-A";
board.m_failedBoard      = EBoardQuality::eGOOD;
board.m_flippedBoard     = EFlippedBoard::eTOP_SIDE_IS_UP;

std::string xml = Hermes::ToXml(board);
// xml = "<BoardAvailable><BoardId>BOARD-001</BoardId>...</BoardAvailable>"

// Deserialize from XML string
Hermes::Optional<BoardAvailableData> result = Hermes::FromXml<BoardAvailableData>(xml);
if (result.has_value()) {
    std::cout << result->m_boardId << "\n";  // "BOARD-001"
}
```

All message types are supported:

```cpp
Hermes::ToXml(const ServiceDescriptionData&)
Hermes::ToXml(const BoardAvailableData&)
Hermes::ToXml(const RevokeBoardAvailableData&)
Hermes::ToXml(const MachineReadyData&)
Hermes::ToXml(const RevokeMachineReadyData&)
Hermes::ToXml(const StartTransportData&)
Hermes::ToXml(const StopTransportData&)
Hermes::ToXml(const TransportFinishedData&)
Hermes::ToXml(const BoardForecastData&)
Hermes::ToXml(const QueryBoardInfoData&)
Hermes::ToXml(const SendBoardInfoData&)
Hermes::ToXml(const NotificationData&)
Hermes::ToXml(const CheckAliveData&)
Hermes::ToXml(const SetConfigurationData&)
Hermes::ToXml(const CurrentConfigurationData&)
Hermes::ToXml(const GetConfigurationData&)
Hermes::ToXml(const CommandData&)

// Vertical:
Hermes::ToXml(const SupervisoryServiceDescriptionData&)
Hermes::ToXml(const BoardArrivedData&)
Hermes::ToXml(const BoardDepartedData&)
Hermes::ToXml(const QueryWorkOrderInfoData&)
Hermes::ToXml(const SendWorkOrderInfoData&)
Hermes::ToXml(const ReplyWorkOrderInfoData&)
Hermes::ToXml(const QueryHermesCapabilitiesData&)
Hermes::ToXml(const SendHermesCapabilitiesData&)
```

---

## 9. Configuration service

The configuration service allows a remote tool (e.g. a factory MES) to read and write the machine's lane configuration over TCP on port 1248.

```cpp
#include "Hermes.hpp"

struct MyConfigHandler : Hermes::IConfigurationServiceCallback
{
    void OnConnected(unsigned sessionId, const ConnectionInfo& info) override
    {
        std::cout << "Config client connected: " << info.m_address << "\n";
    }

    // Remote client reads our configuration
    CurrentConfigurationData OnGetConfiguration(unsigned, const ConnectionInfo&) override
    {
        CurrentConfigurationData config;
        config.m_optionalMachineId = "Machine-A";

        UpstreamConfiguration up;
        up.m_upstreamLaneId = 1;
        up.m_hostAddress    = "192.168.1.2";
        up.m_port           = 50100;
        config.m_upstreamConfigurations.push_back(up);

        return config;
    }

    // Remote client writes a new configuration — return Error{} for success
    Error OnSetConfiguration(unsigned, const ConnectionInfo&,
                             const SetConfigurationData& newConfig) override
    {
        // Apply newConfig to your machine...
        return Error{}; // success
    }

    void OnDisconnected(unsigned, const Error&) override {}
    void OnTrace(unsigned, ETraceType, StringView) override {}
};

// Usage:
MyConfigHandler handler;
Hermes::ConfigurationService service(handler);

ConfigurationServiceSettings settings;
settings.m_port = cCONFIG_PORT; // 1248

service.Enable(settings);
service.Run(); // blocks
```

### Remote configuration client

Query or set a machine's configuration from another process:

```cpp
// Get configuration
auto [config, error] = Hermes::GetConfiguration("192.168.1.2", 10, nullptr);
if (!error) {
    std::cout << "Machine: " << config.m_optionalMachineId.value_or("unknown") << "\n";
}

// Set configuration
SetConfigurationData newConfig;
newConfig.m_machineId = "Machine-A";

Error err = Hermes::SetConfiguration(
    "192.168.1.2",   // host
    newConfig,       // configuration to write
    10,              // timeout seconds
    nullptr,         // optional: resulting config out
    nullptr,         // optional: notifications out
    nullptr          // optional: trace callback
);
```

---

## 10. Vertical interface

The vertical interface connects a machine to a supervisory system (MES/ERP) on a separate TCP port. `VerticalService` is the machine side. `VerticalClient` is the supervisory system side.

```cpp
struct MyVerticalHandler : Hermes::IVerticalServiceCallback
{
    void OnConnected(unsigned sessionId, EVerticalState, const ConnectionInfo&) override {}

    void On(unsigned sessionId, EVerticalState,
            const SupervisoryServiceDescriptionData& data) override
    {
        std::cout << "Supervisor connected: " << data.m_systemId << "\n";
    }

    void On(unsigned sessionId, const GetConfigurationData&, const ConnectionInfo&) override {}
    void On(unsigned sessionId, const SetConfigurationData&, const ConnectionInfo&) override {}
    void On(unsigned sessionId, const NotificationData&) override {}
    void On(unsigned sessionId, const QueryHermesCapabilitiesData&) override {}
    void OnDisconnected(unsigned sessionId, EVerticalState, const Error&) override {}
    void OnTrace(unsigned sessionId, ETraceType, StringView) override {}
};
```

---

## 11. Complete examples

### Example 1 — Downstream machine (listens, receives a board)

```cpp
#include "HermesModern.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

static std::atomic<bool> g_running{true};

int main()
{
    std::signal(SIGINT, [](int) { g_running = false; });

    Hermes::Modern::Downstream ds(1); // lane 1

    // --- Connection events ---
    ds.RegisterConnectedCallback([](unsigned sessionId, const Hermes::ConnectionInfo& info) {
        std::cout << "[DS] Upstream connected from " << info.m_address << "\n";
    });

    ds.RegisterDisconnectedCallback([](unsigned sessionId, const Hermes::Error& err) {
        std::cout << "[DS] Upstream disconnected\n";
    });

    ds.RegisterStateChangeCallback([](unsigned sessionId, Hermes::EState state) {
        std::cout << "[DS] State: " << state << "\n";
    });

    // --- Receive ServiceDescription from Upstream, reply with ours ---
    ds.RegisterServiceDescriptionCallback(
        [&ds](unsigned sessionId, Hermes::EState, const Hermes::ServiceDescriptionData& data) {
            std::cout << "[DS] Received ServiceDescription from: " << data.m_machineId << "\n";

            // Reply with our own ServiceDescription
            ds.Post([&ds, sessionId]() {
                Hermes::ServiceDescriptionData reply;
                reply.m_machineId = "Machine-Downstream";
                reply.m_laneId    = 1;
                ds.Signal(sessionId, reply);
                std::cout << "[DS] Sent ServiceDescription\n";
            });
        });

    // --- When Upstream is MachineReady, send BoardAvailable ---
    ds.RegisterMachineReadyCallback(
        [&ds](unsigned sessionId, Hermes::EState, const Hermes::MachineReadyData&) {
            std::cout << "[DS] Upstream is MachineReady — sending BoardAvailable\n";

            ds.Post([&ds, sessionId]() {
                Hermes::BoardAvailableData board;
                board.m_boardId          = "PCB-2024-001";
                board.m_boardIdCreatedBy = "Machine-Downstream";
                board.m_failedBoard      = Hermes::EBoardQuality::eGOOD;
                board.m_flippedBoard     = Hermes::EFlippedBoard::eTOP_SIDE_IS_UP;
                ds.Signal(sessionId, board);
            });
        });

    // --- When Upstream starts transport, confirm when done ---
    ds.RegisterStartTransportCallback(
        [&ds](unsigned sessionId, Hermes::EState, const Hermes::StartTransportData& data) {
            std::cout << "[DS] StartTransport for board: " << data.m_boardId << "\n";

            // Simulate board moving
            std::this_thread::sleep_for(std::chrono::seconds(1));

            ds.Post([&ds, sessionId, boardId = data.m_boardId]() {
                Hermes::TransportFinishedData finished;
                finished.m_transferState = Hermes::ETransferState::eCOMPLETE;
                finished.m_boardId       = boardId;
                ds.Signal(sessionId, finished);
                std::cout << "[DS] TransportFinished sent\n";
            });
        });

    // --- Notifications ---
    ds.RegisterNotificationCallback([](unsigned, const Hermes::NotificationData& n) {
        std::cout << "[DS] Notification: " << n.m_description << "\n";
    });

    // --- Start ---
    Hermes::DownstreamSettings settings;
    settings.m_machineId = "Machine-Downstream";
    settings.m_port      = 50100;
    ds.Enable(settings);

    std::cout << "[DS] Listening on port 50100. Ctrl+C to stop.\n";
    while (g_running)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ds.Stop();
    return 0;
}
```

### Example 2 — Upstream machine (connects, sends MachineReady)

```cpp
#include "HermesModern.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

static std::atomic<bool> g_running{true};

int main()
{
    std::signal(SIGINT, [](int) { g_running = false; });

    Hermes::Modern::Upstream us(1); // lane 1

    us.RegisterConnectedCallback([](unsigned sessionId, const Hermes::ConnectionInfo& info) {
        std::cout << "[US] Connected to downstream at " << info.m_address << "\n";
    });

    us.RegisterDisconnectedCallback([](unsigned sessionId, const Hermes::Error& err) {
        std::cout << "[US] Disconnected\n";
    });

    us.RegisterStateChangeCallback([](unsigned sessionId, Hermes::EState state) {
        std::cout << "[US] State: " << state << "\n";
    });

    // --- Receive ServiceDescription, reply and send MachineReady ---
    us.RegisterServiceDescriptionCallback(
        [&us](unsigned sessionId, Hermes::EState, const Hermes::ServiceDescriptionData& data) {
            std::cout << "[US] Received ServiceDescription from: " << data.m_machineId << "\n";

            us.Post([&us, sessionId]() {
                // Send our ServiceDescription
                Hermes::ServiceDescriptionData reply;
                reply.m_machineId = "Machine-Upstream";
                reply.m_laneId    = 1;
                us.Signal(sessionId, reply);

                // Then send MachineReady
                Hermes::MachineReadyData ready;
                ready.m_failedBoard = Hermes::EBoardQuality::eANY;
                us.Signal(sessionId, ready);
                std::cout << "[US] Sent ServiceDescription + MachineReady\n";
            });
        });

    // --- Receive BoardAvailable, trigger StartTransport ---
    us.RegisterBoardAvailableCallback(
        [&us](unsigned sessionId, Hermes::EState, const Hermes::BoardAvailableData& board) {
            std::cout << "[US] BoardAvailable: " << board.m_boardId << "\n";

            us.Post([&us, sessionId, boardId = board.m_boardId]() {
                Hermes::StartTransportData start;
                start.m_boardId = boardId;
                us.Signal(sessionId, start);
                std::cout << "[US] StartTransport sent\n";
            });
        });

    // --- Receive TransportFinished, send StopTransport ---
    us.RegisterTransportFinishedCallback(
        [&us](unsigned sessionId, Hermes::EState, const Hermes::TransportFinishedData& data) {
            std::cout << "[US] TransportFinished for board: " << data.m_boardId << "\n";

            us.Post([&us, sessionId, boardId = data.m_boardId]() {
                Hermes::StopTransportData stop;
                stop.m_transferState = Hermes::ETransferState::eCOMPLETE;
                stop.m_boardId       = boardId;
                us.Signal(sessionId, stop);
                std::cout << "[US] StopTransport sent — full board transfer complete\n";
            });
        });

    us.RegisterNotificationCallback([](unsigned, const Hermes::NotificationData& n) {
        std::cout << "[US] Notification: " << n.m_description << "\n";
    });

    // EDIT THIS — set the downstream machine's IP:
    Hermes::UpstreamSettings settings;
    settings.m_machineId   = "Machine-Upstream";
    settings.m_hostAddress = "192.168.1.2";  // <-- downstream machine IP
    settings.m_port        = 50100;
    us.Enable(settings);

    std::cout << "[US] Connecting to 192.168.1.2:50100. Ctrl+C to stop.\n";
    while (g_running)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    us.Stop();
    return 0;
}
```

### Example 3 — XML serialization and inspection

```cpp
#include "HermesSerialization.hpp"
#include <iostream>

int main()
{
    // Serialize
    Hermes::BoardAvailableData board;
    board.m_boardId          = "PCB-001";
    board.m_boardIdCreatedBy = "Machine-A";
    board.m_failedBoard      = Hermes::EBoardQuality::eGOOD;
    board.m_flippedBoard     = Hermes::EFlippedBoard::eTOP_SIDE_IS_UP;
    board.m_length           = 150.0;
    board.m_width            = 100.0;

    std::string xml = Hermes::ToXml(board);
    std::cout << "XML:\n" << xml << "\n";

    // Deserialize
    auto result = Hermes::FromXml<Hermes::BoardAvailableData>(xml);
    if (result.has_value()) {
        std::cout << "BoardId: " << result->m_boardId << "\n";
        if (result->m_length.has_value())
            std::cout << "Length:  " << *result->m_length << "mm\n";
    }
    return 0;
}
```