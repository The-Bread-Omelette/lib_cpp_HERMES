# Hermes Standard Library — Complete API Reference

**Version:** 1.0  
**License:** Apache 2.0 — Copyright 2018 ASM Assembly Systems GmbH & Co. KG  
**Protocol:** The Hermes Standard — vendor-independent machine-to-machine communication for SMT assembly  
**Requires:** C++17, Boost 1.66+, CMake 3.15+

---

## Table of Contents

1. [Protocol primer](#1-protocol-primer)
2. [Building](#2-building)
3. [C API — Hermes.h](#3-c-api)
4. [C++ API — Hermes.hpp](#4-c-api-1)
5. [Modern C++ API — HermesModern.hpp](#5-modern-c-api)
6. [Serialization API — HermesSerialization.h/.hpp](#6-serialization-api)
7. [Data types reference — HermesData.h / HermesData.hpp](#7-data-types-reference)
8. [Error handling](#8-error-handling)
9. [Thread safety rules](#9-thread-safety-rules)
10. [Complete examples](#10-complete-examples)
11. [Bugs fixed in this release](#11-bugs-fixed)

---

## 1. Protocol primer

Hermes connects machines in a PCB assembly line over TCP/IP. Each machine has a **Downstream port** (server — listens for the previous machine) and an **Upstream port** (client — connects to the next machine).

```
[Machine A]──Upstream──connects to──Downstream──[Machine B]──Upstream──connects to──Downstream──[Machine C]
             port 50100                                        port 50101
```

**Session:** every TCP connect/disconnect cycle increments the session ID (starts at 1). All callbacks receive a `sessionId` so you can correlate events.

**Message flow for a board transfer:**

```
Downstream (server)           Upstream (client)
       │◄──── TCP connect ──────────│
       │◄──── ServiceDescription ───│  Upstream identifies itself
       │───── ServiceDescription ──►│  Downstream identifies itself
       │◄──── MachineReady ─────────│  Upstream ready to receive
       │───── BoardAvailable ───────►│  Downstream has a board
       │◄──── StartTransport ────────│  Upstream says: send it
       │  [board physically moves]   │
       │───── TransportFinished ─────►│  Downstream: done
       │◄──── StopTransport ─────────│  Upstream: confirmed received
```

**Who receives what:**

| Role | Receives FROM peer | Sends TO peer |
|------|--------------------|---------------|
| Downstream (server) | `ServiceDescription`, `MachineReady`, `RevokeMachineReady`, `StartTransport`, `StopTransport`, `QueryBoardInfo` | `ServiceDescription`, `BoardAvailable`, `RevokeBoardAvailable`, `TransportFinished`, `BoardForecast`, `SendBoardInfo` |
| Upstream (client) | `ServiceDescription`, `BoardAvailable`, `RevokeBoardAvailable`, `TransportFinished`, `BoardForecast`, `SendBoardInfo` | `ServiceDescription`, `MachineReady`, `RevokeMachineReady`, `StartTransport`, `StopTransport`, `QueryBoardInfo` |

---

## 2. Building

```bash
# Install dependencies (Raspberry Pi OS / Debian / Ubuntu)
sudo apt install -y g++ cmake libboost-all-dev

# Build the library
git clone https://github.com/hermes-org/lib_cpp.git
cd lib_cpp
mkdir build && cd build
cmake ..
make -j4

# Find the shared library
find . -name "libhermes.so"
# → ./src/Hermes/libhermes.so

# Compile your application
g++ -std=c++17 -o myapp myapp.cpp \
    -I./src/include \
    -L./build/src/Hermes -lhermes \
    -lboost_system -lpthread

# Run
LD_LIBRARY_PATH=./build/src/Hermes ./myapp
```

---

## 3. C API

**Header:** `Hermes.h` (includes `HermesData.h` and `HermesStringView.h`)  
**Link:** `-lhermes`

The C API uses opaque handles and function-pointer callbacks. Every callback struct has the same shape:

```c
struct HermesXxxCallback {
    void (*m_pCall)(void* m_pData, /* message params */);
    void* m_pData;   // passed back as first arg to m_pCall
};
```

### 3.1 Constants

```c
static const uint16_t cHERMES_BASE_PORT   = 50100U;  // default machine port
static const uint16_t cHERMES_CONFIG_PORT = 1248U;   // configuration service port
static const unsigned cHERMES_MAX_MESSAGE_SIZE = 65536U;
```

### 3.2 HermesStringView

Non-owning string (like `std::string_view`). All C structs use this. **Does not own the memory.**

```c
struct HermesStringView {
    const char* m_pData;  // NULL means "no string" (not even empty)
    size_t      m_size;
};
```

### 3.3 Downstream API

**Lifecycle:**

```c
// 1. Create
HermesDownstream* pDs = CreateHermesDownstream(
    uint32_t laneId,                      // lane number (1-based)
    const HermesDownstreamCallbacks* pCb  // all callbacks
);

// 2. Enable (start listening)
HermesDownstreamSettings settings = {0};
settings.m_machineId  = {"MyMachine", 9};
settings.m_port       = 50100;
settings.m_checkAlivePeriodInSeconds  = 60.0;
settings.m_reconnectWaitTimeInSeconds = 10.0;
EnableHermesDownstream(pDs, &settings);

// 3. Run (blocks until Stop is called from another thread)
RunHermesDownstream(pDs);

// 4. Stop (call from another thread or from a PostHermesDownstream callback)
StopHermesDownstream(pDs);

// 5. Delete (after Run returns)
DeleteHermesDownstream(pDs);
```

**Callbacks to register in `HermesDownstreamCallbacks`:**

```c
struct HermesDownstreamCallbacks {
    HermesConnectedCallback          m_connectedCallback;          // TCP connected
    HermesServiceDescriptionCallback m_serviceDescriptionCallback; // upstream identified itself
    HermesMachineReadyCallback       m_machineReadyCallback;       // upstream ready to receive
    HermesRevokeMachineReadyCallback m_revokeMachineReadyCallback; // upstream cancels ready
    HermesStartTransportCallback     m_startTransportCallback;     // upstream says: send board
    HermesStopTransportCallback      m_stopTransportCallback;      // upstream confirms received
    HermesQueryBoardInfoCallback     m_queryBoardInfoCallback;     // upstream asks board info
    HermesNotificationCallback       m_notificationCallback;       // protocol notification
    HermesStateCallback              m_stateCallback;              // state machine changed
    HermesCheckAliveCallback         m_checkAliveCallback;         // keepalive ping/pong
    HermesCommandCallback            m_commandCallback;            // vendor command
    HermesDisconnectedCallback       m_disconnectedCallback;       // TCP disconnected
    HermesTraceCallback              m_traceCallback;              // raw XML / debug log
};
```

**Signals (send to upstream):**

```c
SignalHermesDownstreamServiceDescription(pDs, sessionId, const HermesServiceDescriptionData*);
SignalHermesBoardAvailable(pDs, sessionId, const HermesBoardAvailableData*);
SignalHermesRevokeBoardAvailable(pDs, sessionId, const HermesRevokeBoardAvailableData*);
SignalHermesTransportFinished(pDs, sessionId, const HermesTransportFinishedData*);
SignalHermesBoardForecast(pDs, sessionId, const HermesBoardForecastData*);
SignalHermesSendBoardInfo(pDs, sessionId, const HermesSendBoardInfoData*);
SignalHermesDownstreamNotification(pDs, sessionId, const HermesNotificationData*);
SignalHermesDownstreamCheckAlive(pDs, sessionId, const HermesCheckAliveData*);
SignalHermesDownstreamCommand(pDs, sessionId, const HermesCommandData*);

// Reset to initial state (sends notification then resets state machine)
ResetHermesDownstream(pDs, const HermesNotificationData*);

// Disable (stop accepting new connections, notify current peer)
DisableHermesDownstream(pDs, const HermesNotificationData*);

// Testing only — send raw XML string
SignalHermesDownstreamRawXml(pDs, sessionId, HermesStringView rawXml);
ResetHermesDownstreamRawXml(pDs, HermesStringView rawXml);
```

**Post a task onto the Hermes event thread (thread-safe):**

```c
struct HermesVoidCallback {
    void (*m_pCall)(void* m_pData);
    void* m_pData;
};
PostHermesDownstream(pDs, HermesVoidCallback);
```

### 3.4 Upstream API

**Lifecycle:**

```c
HermesUpstream* pUs = CreateHermesUpstream(laneId, const HermesUpstreamCallbacks*);

HermesUpstreamSettings settings = {0};
settings.m_machineId   = {"MyMachine", 9};
settings.m_hostAddress = {"192.168.1.2", 11};  // downstream machine IP
settings.m_port        = 50100;
settings.m_checkAlivePeriodInSeconds  = 60.0;
settings.m_reconnectWaitTimeInSeconds = 10.0;
EnableHermesUpstream(pUs, &settings);

RunHermesUpstream(pUs);   // blocks
StopHermesUpstream(pUs);
DeleteHermesUpstream(pUs);
```

**Callbacks in `HermesUpstreamCallbacks`:**

```c
struct HermesUpstreamCallbacks {
    HermesConnectedCallback           m_connectedCallback;
    HermesServiceDescriptionCallback  m_serviceDescriptionCallback;  // downstream identified
    HermesBoardAvailableCallback      m_boardAvailableCallback;      // downstream has board
    HermesRevokeBoardAvailableCallback m_revokeBoardAvailableCallback;
    HermesTransportFinishedCallback   m_transportFinishedCallback;   // downstream done
    HermesBoardForecastCallback       m_boardForecastCallback;       // board coming soon
    HermesSendBoardInfoCallback       m_sendBoardInfoCallback;       // board metadata
    HermesNotificationCallback        m_notificationCallback;
    HermesStateCallback               m_stateCallback;
    HermesCheckAliveCallback          m_checkAliveCallback;
    HermesCommandCallback             m_commandCallback;
    HermesDisconnectedCallback        m_disconnectedCallback;
    HermesTraceCallback               m_traceCallback;
};
```

**Signals (send to downstream):**

```c
SignalHermesUpstreamServiceDescription(pUs, sessionId, const HermesServiceDescriptionData*);
SignalHermesMachineReady(pUs, sessionId, const HermesMachineReadyData*);
SignalHermesRevokeMachineReady(pUs, sessionId, const HermesRevokeMachineReadyData*);
SignalHermesStartTransport(pUs, sessionId, const HermesStartTransportData*);
SignalHermesStopTransport(pUs, sessionId, const HermesStopTransportData*);
SignalHermesQueryBoardInfo(pUs, sessionId, const HermesQueryBoardInfoData*);
SignalHermesUpstreamNotification(pUs, sessionId, const HermesNotificationData*);
SignalHermesUpstreamCheckAlive(pUs, sessionId, const HermesCheckAliveData*);
SignalHermesUpstreamCommand(pUs, sessionId, const HermesCommandData*);
ResetHermesUpstream(pUs, const HermesNotificationData*);
DisableHermesUpstream(pUs, const HermesNotificationData*);
PostHermesUpstream(pUs, HermesVoidCallback);
```

### 3.5 Configuration client API

Get or set a machine's Hermes lane configuration over TCP (port 1248).

```c
// Get configuration (synchronous, blocks until complete or timeout)
struct HermesGetConfigurationCallbacks {
    HermesCurrentConfigurationCallback m_currentConfigurationCallback;
    HermesErrorCallback                m_errorCallback;
    HermesTraceCallback                m_traceCallback;
};
GetHermesConfiguration(
    HermesStringView hostName,        // e.g. "192.168.1.2"
    unsigned timeoutInSeconds,        // e.g. 10
    const HermesGetConfigurationCallbacks*
);

// Set configuration (synchronous)
struct HermesSetConfigurationCallbacks {
    HermesCurrentConfigurationCallback m_currentConfigurationCallback;
    HermesErrorCallback                m_errorCallback;
    HermesNotificationCallback         m_notificationCallback;
    HermesTraceCallback                m_traceCallback;
};
SetHermesConfiguration(
    HermesStringView hostName,
    const HermesSetConfigurationData*,
    unsigned timeoutInSeconds,
    const HermesSetConfigurationCallbacks*
);
```

### 3.6 Configuration service API

Expose a configuration service on the machine (port 1248) so remote tools can query/set config.

```c
HermesConfigurationService* pSvc = CreateHermesConfigurationService(
    const HermesConfigurationServiceCallbacks*
);

struct HermesConfigurationServiceCallbacks {
    HermesConnectedCallback        m_connectedCallback;
    HermesSetConfigurationCallback m_setConfigurationCallback;
    HermesGetConfigurationCallback m_getConfigurationCallback;
    HermesDisconnectedCallback     m_disconnectedCallback;
    HermesTraceCallback            m_traceCallback;
};

// Inside m_getConfigurationCallback, call:
SignalHermesCurrentConfiguration(pSvc, sessionId, const HermesCurrentConfigurationData*);
// Inside m_setConfigurationCallback, call:
SignalHermesConfigurationNotification(pSvc, sessionId, const HermesNotificationData*); // on error only

EnableHermesConfigurationService(pSvc, const HermesConfigurationServiceSettings*);
RunHermesConfigurationService(pSvc);  // blocks
StopHermesConfigurationService(pSvc);
DeleteHermesConfigurationService(pSvc);
```

### 3.7 Vertical service API (machine side)

The vertical channel connects a machine to a supervisory system (MES/ERP).

```c
HermesVerticalService* pVs = CreateHermesVerticalService(
    const HermesVerticalServiceCallbacks*
);
EnableHermesVerticalService(pVs, const HermesVerticalServiceSettings*);
RunHermesVerticalService(pVs);

// Signals:
SignalHermesVerticalServiceDescription(pVs, sessionId, const HermesSupervisoryServiceDescriptionData*);
SignalHermesQueryWorkOrderInfo(pVs, sessionId, const HermesQueryWorkOrderInfoData*);
SignalHermesReplyWorkOrderInfo(pVs, sessionId, const HermesReplyWorkOrderInfoData*);
SignalHermesVerticalServiceNotification(pVs, sessionId, const HermesNotificationData*);
SignalHermesVerticalServiceCheckAlive(pVs, sessionId, const HermesCheckAliveData*);
SignalHermesVerticalCurrentConfiguration(pVs, sessionId, const HermesCurrentConfigurationData*);
SignalHermesSendHermesCapabilities(pVs, sessionId, const HermesSendHermesCapabilitiesData*);

// sessionId == 0 → broadcast to all clients with FeatureBoardTracking
SignalHermesBoardArrived(pVs, sessionId, const HermesBoardArrivedData*);
SignalHermesBoardDeparted(pVs, sessionId, const HermesBoardDepartedData*);

ResetHermesVerticalServiceSession(pVs, sessionId, const HermesNotificationData*);
StopHermesVerticalService(pVs);
DeleteHermesVerticalService(pVs);
```

### 3.8 Vertical client API (supervisory system side)

```c
HermesVerticalClient* pVc = CreateHermesVerticalClient(
    const HermesVerticalClientCallbacks*
);
EnableHermesVerticalClient(pVc, const HermesVerticalClientSettings*);
RunHermesVerticalClient(pVc);

// Signals:
SignalHermesVerticalClientDescription(pVc, sessionId, const HermesSupervisoryServiceDescriptionData*);
SignalHermesSendWorkOrderInfo(pVc, sessionId, const HermesSendWorkOrderInfoData*);
SignalHermesVerticalGetConfiguration(pVc, sessionId, const HermesGetConfigurationData*);
SignalHermesVerticalSetConfiguration(pVc, sessionId, const HermesSetConfigurationData*);
SignalHermesVerticalQueryHermesCapabilities(pVc, sessionId, const HermesQueryHermesCapabilitiesData*);
SignalHermesVerticalClientNotification(pVc, sessionId, const HermesNotificationData*);
SignalHermesVerticalClientCheckAlive(pVc, sessionId, const HermesCheckAliveData*);
ResetHermesVerticalClient(pVc, const HermesNotificationData*);
StopHermesVerticalClient(pVc);
DeleteHermesVerticalClient(pVc);
```

---

## 4. C++ API

**Header:** `Hermes.hpp` (includes `HermesDataConversion.hpp` → `HermesData.hpp`)  
Use this when you need full session control, `Post()`, raw XML, or are implementing a class that handles both Upstream and Downstream on the same object.

### 4.1 Hermes::Downstream

```cpp
// Constructor — takes laneId and a reference to your callback implementation
Hermes::Downstream downstream(unsigned laneId, Hermes::IDownstreamCallback& callback);

// Lifecycle
downstream.Enable(const DownstreamSettings&);  // start listening
downstream.Run();                               // blocks until Stop()
downstream.Stop();                              // shut down
// Destructor calls DeleteHermesDownstream automatically

// Post a callable onto the Hermes network thread (thread-safe from any thread)
downstream.Post([&]() {
    downstream.Signal(sessionId, boardAvailableData);
});

// Signals — call from within Post() or from a callback (both are on the network thread)
downstream.Signal(sessionId, const ServiceDescriptionData&);
downstream.Signal(sessionId, const BoardAvailableData&);
downstream.Signal(sessionId, const RevokeBoardAvailableData&);
downstream.Signal(sessionId, const TransportFinishedData&);
downstream.Signal(sessionId, const BoardForecastData&);
downstream.Signal(sessionId, const SendBoardInfoData&);
downstream.Signal(sessionId, const NotificationData&);
downstream.Signal(sessionId, const CheckAliveData&);
downstream.Signal(sessionId, const CommandData&);
downstream.Reset(const NotificationData&);
downstream.Disable(const NotificationData&);

// Testing only
downstream.Signal(sessionId, StringView rawXml);
downstream.Reset(StringView rawXml);
```

### 4.2 IDownstreamCallback — pure virtual interface

Inherit from this and implement every pure virtual method.

```cpp
struct MyDownstreamCallback : Hermes::IDownstreamCallback
{
    // Connection events — all pure virtual
    void OnConnected(unsigned sessionId, EState, const ConnectionInfo&) override;
    void OnDisconnected(unsigned sessionId, EState, const Error&) override;
    void OnState(unsigned sessionId, EState) override;
    void OnTrace(unsigned sessionId, ETraceType, StringView trace) override;

    // Messages received FROM the Upstream machine — pure virtual:
    void On(unsigned sessionId, EState, const ServiceDescriptionData&) override;
    void On(unsigned sessionId, EState, const MachineReadyData&) override;
    void On(unsigned sessionId, EState, const RevokeMachineReadyData&) override;
    void On(unsigned sessionId, EState, const StartTransportData&) override;
    void On(unsigned sessionId, EState, const StopTransportData&) override;
    void On(unsigned sessionId, const NotificationData&) override;
    void On(unsigned sessionId, const CommandData&) override;

    // Optional — have default empty implementations (not pure virtual):
    void On(unsigned sessionId, const CheckAliveData&) override {}
    void On(unsigned sessionId, const QueryBoardInfoData&) override {}
};
```

### 4.3 Hermes::Upstream

```cpp
Hermes::Upstream upstream(unsigned laneId, Hermes::IUpstreamCallback& callback);

upstream.Enable(const UpstreamSettings&);
upstream.Run();
upstream.Stop();

upstream.Post([&]() { upstream.Signal(sessionId, machineReadyData); });

upstream.Signal(sessionId, const ServiceDescriptionData&);
upstream.Signal(sessionId, const MachineReadyData&);
upstream.Signal(sessionId, const RevokeMachineReadyData&);
upstream.Signal(sessionId, const StartTransportData&);
upstream.Signal(sessionId, const StopTransportData&);
upstream.Signal(sessionId, const QueryBoardInfoData&);
upstream.Signal(sessionId, const NotificationData&);
upstream.Signal(sessionId, const CheckAliveData&);
upstream.Signal(sessionId, const CommandData&);
upstream.Reset(const NotificationData&);
upstream.Disable(const NotificationData&);
upstream.Signal(sessionId, StringView rawXml);
upstream.Reset(StringView rawXml);
```

### 4.4 IUpstreamCallback — pure virtual interface

```cpp
struct MyUpstreamCallback : Hermes::IUpstreamCallback
{
    void OnConnected(unsigned sessionId, EState, const ConnectionInfo&) override;
    void OnDisconnected(unsigned sessionId, EState, const Error&) override;
    void OnState(unsigned sessionId, EState) override;
    void OnTrace(unsigned sessionId, ETraceType, StringView) override;

    // Messages received FROM the Downstream machine — pure virtual:
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

### 4.5 Configuration service C++ wrapper

```cpp
Hermes::ConfigurationService svc(Hermes::IConfigurationServiceCallback& callback);
svc.Enable(const ConfigurationServiceSettings&);
svc.Run();
svc.Stop();
svc.Post(callable);

// Free functions for the client side:
auto [config, error] = Hermes::GetConfiguration("192.168.1.2", 10, nullptr);
Error err = Hermes::SetConfiguration("192.168.1.2", setConfigData, 10, nullptr, nullptr, nullptr);
```

### 4.6 Dual-role helper — UpstreamCallbackHelper / DownstreamCallbackHelper

When one class implements both `IUpstreamCallback` and `IDownstreamCallback`, some `On()` method signatures clash. Use the helpers to disambiguate:

```cpp
struct MyHandler
    : Hermes::UpstreamCallbackHelper
    , Hermes::DownstreamCallbackHelper
{
    // Upstream methods get "Upstream" prefix:
    void OnUpstreamConnected(unsigned sessionId, EState, const ConnectionInfo&) override;
    void OnUpstream(unsigned sessionId, EState, const ServiceDescriptionData&) override;
    void OnUpstream(unsigned sessionId, const NotificationData&) override;
    void OnUpstream(unsigned sessionId, const CheckAliveData&) override;
    void OnUpstream(unsigned sessionId, const CommandData&) override;
    void OnUpstreamState(unsigned sessionId, EState) override;
    void OnUpstreamDisconnected(unsigned sessionId, EState, const Error&) override;
    void OnUpstreamTrace(unsigned sessionId, ETraceType, StringView) override;

    // Downstream methods get "Downstream" prefix:
    void OnDownstreamConnected(unsigned sessionId, EState, const ConnectionInfo&) override;
    void OnDownstream(unsigned sessionId, EState, const ServiceDescriptionData&) override;
    void OnDownstream(unsigned sessionId, const NotificationData&) override;
    void OnDownstream(unsigned sessionId, const CheckAliveData&) override;
    void OnDownstream(unsigned sessionId, const CommandData&) override;
    void OnDownstreamState(unsigned sessionId, EState) override;
    void OnDownstreamDisconnected(unsigned sessionId, EState, const Error&) override;
    void OnDownstreamTrace(unsigned sessionId, ETraceType, StringView) override;

    // Unambiguous methods remain unchanged:
    void On(unsigned sessionId, EState, const BoardAvailableData&) override;
    void On(unsigned sessionId, EState, const MachineReadyData&) override;
    // etc.
};
```

---

## 5. Modern C++ API

**Header:** `HermesModern.hpp`  
**Namespace:** `Hermes::Modern`

Uses `std::function` callbacks instead of virtual interfaces. Manages the network thread internally. Recommended for all new application code.

### 5.1 Modern::Downstream

```cpp
Hermes::Modern::Downstream ds(unsigned laneId);

// Register callbacks before calling Enable()
ds.RegisterConnectedCallback(   [](unsigned sessionId, const ConnectionInfo& info) { });
ds.RegisterDisconnectedCallback([](unsigned sessionId, const Error& err) { });
ds.RegisterStateChangeCallback( [](unsigned sessionId, EState state) { });
ds.RegisterTraceCallback(       [](unsigned sessionId, ETraceType, StringView msg) { });

// Messages received FROM the Upstream machine:
ds.RegisterServiceDescriptionCallback( [](unsigned sessionId, EState, const ServiceDescriptionData&) { });
ds.RegisterMachineReadyCallback(       [](unsigned sessionId, EState, const MachineReadyData&) { });
ds.RegisterRevokeMachineReadyCallback( [](unsigned sessionId, EState, const RevokeMachineReadyData&) { });
ds.RegisterStartTransportCallback(     [](unsigned sessionId, EState, const StartTransportData&) { });
ds.RegisterStopTransportCallback(      [](unsigned sessionId, EState, const StopTransportData&) { });
ds.RegisterQueryBoardInfoCallback(     [](unsigned sessionId, const QueryBoardInfoData&) { });

// Auxiliary:
ds.RegisterNotificationCallback([](unsigned sessionId, const NotificationData&) { });
ds.RegisterCheckAliveCallback(  [](unsigned sessionId, const CheckAliveData&) { });
ds.RegisterCommandCallback(     [](unsigned sessionId, const CommandData&) { });

// Lifecycle
DownstreamSettings settings;
settings.m_machineId = "MyMachine";
settings.m_port      = 50100;
ds.Enable(settings);  // starts listening, launches network thread
ds.Stop();            // blocks until network thread finishes. Safe to call multiple times.

// Send messages TO the Upstream (call from inside Post() or from within a callback)
ds.Post([&ds, sessionId]() {
    ds.Signal(sessionId, boardAvailableData);
});
```

### 5.2 Modern::Upstream

```cpp
Hermes::Modern::Upstream us(unsigned laneId);

us.RegisterConnectedCallback(   [](unsigned sessionId, const ConnectionInfo&) { });
us.RegisterDisconnectedCallback([](unsigned sessionId, const Error&) { });
us.RegisterStateChangeCallback( [](unsigned sessionId, EState) { });
us.RegisterTraceCallback(       [](unsigned sessionId, ETraceType, StringView) { });

// Messages received FROM the Downstream machine:
us.RegisterServiceDescriptionCallback(   [](unsigned sessionId, EState, const ServiceDescriptionData&) { });
us.RegisterBoardAvailableCallback(       [](unsigned sessionId, EState, const BoardAvailableData&) { });
us.RegisterRevokeBoardAvailableCallback( [](unsigned sessionId, EState, const RevokeBoardAvailableData&) { });
us.RegisterTransportFinishedCallback(    [](unsigned sessionId, EState, const TransportFinishedData&) { });
us.RegisterBoardForecastCallback(        [](unsigned sessionId, EState, const BoardForecastData&) { });
us.RegisterSendBoardInfoCallback(        [](unsigned sessionId, const SendBoardInfoData&) { });
us.RegisterNotificationCallback(         [](unsigned sessionId, const NotificationData&) { });
us.RegisterCheckAliveCallback(           [](unsigned sessionId, const CheckAliveData&) { });
us.RegisterCommandCallback(              [](unsigned sessionId, const CommandData&) { });

UpstreamSettings settings;
settings.m_machineId   = "MyMachine";
settings.m_hostAddress = "192.168.1.2";  // downstream machine IP
settings.m_port        = 50100;
us.Enable(settings);
us.Stop();

us.Post([&us, sessionId]() {
    us.Signal(sessionId, machineReadyData);
});
```

---

## 6. Serialization API

**C header:** `HermesSerialization.h`  
**C++ header:** `HermesSerialization.hpp`

### 6.1 C API

```c
// Callback that receives the serialized XML string
struct HermesSerializationCallback {
    void (*m_pCall)(void* m_pData, HermesStringView result);
    void* m_pData;
};

// Serialize any message to XML:
HermesSerializeServiceDescription(const HermesServiceDescriptionData*, HermesSerializationCallback);
HermesSerializeBoardAvailable(const HermesBoardAvailableData*, HermesSerializationCallback);
HermesSerializeMachineReady(const HermesMachineReadyData*, HermesSerializationCallback);
HermesSerializeStartTransport(const HermesStartTransportData*, HermesSerializationCallback);
HermesSerializeStopTransport(const HermesStopTransportData*, HermesSerializationCallback);
HermesSerializeTransportFinished(const HermesTransportFinishedData*, HermesSerializationCallback);
HermesSerializeRevokeBoardAvailable(const HermesRevokeBoardAvailableData*, HermesSerializationCallback);
HermesSerializeRevokeMachineReady(const HermesRevokeMachineReadyData*, HermesSerializationCallback);
HermesSerializeBoardForecast(const HermesBoardForecastData*, HermesSerializationCallback);
HermesSerializeQueryBoardInfo(const HermesQueryBoardInfoData*, HermesSerializationCallback);
HermesSerializeSendBoardInfo(const HermesSendBoardInfoData*, HermesSerializationCallback);
HermesSerializeNotification(const HermesNotificationData*, HermesSerializationCallback);
HermesSerializeCheckAlive(const HermesCheckAliveData*, HermesSerializationCallback);
HermesSerializeGetConfiguration(const HermesGetConfigurationData*, HermesSerializationCallback);
HermesSerializeSetConfiguration(const HermesSetConfigurationData*, HermesSerializationCallback);
HermesSerializeCurrentConfiguration(const HermesCurrentConfigurationData*, HermesSerializationCallback);
HermesSerializeCommand(const HermesCommandData*, HermesSerializationCallback);
// Vertical:
HermesSerializeSupervisoryServiceDescription(...);
HermesSerializeBoardArrived(...);
HermesSerializeBoardDeparted(...);
HermesSerializeQueryWorkOrderInfo(...);
HermesSerializeSendWorkOrderInfo(...);
HermesSerializeReplyWorkOrderInfo(...);
HermesSerializeQueryHermesCapabilities(...);
HermesSerializeSendHermesCapabilities(...);

// Deserialize — fills in whichever callback matches the XML message type
void HermesDeserialize(HermesStringView xml, const HermesDeserializationCallbacks*);
```

### 6.2 C++ API

```cpp
#include "HermesSerialization.hpp"

// Serialize
BoardAvailableData board;
board.m_boardId          = "PCB-001";
board.m_boardIdCreatedBy = "MachineA";
board.m_failedBoard      = EBoardQuality::eGOOD;
board.m_flippedBoard     = EFlippedBoard::eTOP_SIDE_IS_UP;
board.m_optionalLengthInMM = 150.0;

std::string xml = Hermes::ToXml(board);

// Deserialize
Hermes::Optional<BoardAvailableData> result = Hermes::FromXml<BoardAvailableData>(xml);
if (result.has_value()) {
    std::cout << result->m_boardId << "\n";
}
```

`ToXml()` and `FromXml<T>()` are available for all message types.

---

## 7. Data types reference

All types are in `HermesData.hpp` (C++) or `HermesData.h` (C).

### Key enums

| C++ enum | Values |
|----------|--------|
| `EState` | `eNOT_CONNECTED`, `eSOCKET_CONNECTED`, `eSERVICE_DESCRIPTION_DOWNSTREAM`, `eNOT_AVAILABLE_NOT_READY`, `eBOARD_AVAILABLE`, `eMACHINE_READY`, `eAVAILABLE_AND_READY`, `eTRANSPORTING`, `eTRANSPORT_STOPPED`, `eTRANSPORT_FINISHED`, `eDISCONNECTED` |
| `ETraceType` | `eSENT`, `eRECEIVED`, `eDEBUG`, `eINFO`, `eWARNING`, `eERROR` |
| `EBoardQuality` | `eANY`, `eGOOD`, `eBAD` |
| `EFlippedBoard` | `eSIDE_UP_IS_UNKNOWN`, `eTOP_SIDE_IS_UP`, `eBOTTOM_SIDE_IS_UP` |
| `ETransferState` | `eUNKNOWN`, `eNOT_STARTED`, `eINCOMPLETE`, `eCOMPLETE` |
| `ENotificationCode` | `eUNSPECIFIC`, `ePROTOCOL_ERROR`, `eCONNECTION_REFUSED_BECAUSE_OF_ESTABLISHED_CONNECTION`, `eCONNECTION_RESET_BECAUSE_OF_CHANGED_CONFIGURATION`, `eCONFIGURATION_ERROR`, `eMACHINE_SHUTDOWN`, `eBOARD_FORECAST_ERROR` |
| `ESeverity` | `eUNKNOWN`, `eFATAL`, `eERROR`, `eWARNING`, `eINFO` |
| `EErrorCode` | `eSUCCESS`, `eIMPLEMENTATION_ERROR`, `ePEER_ERROR`, `eCLIENT_ERROR`, `eNETWORK_ERROR`, `eTIMEOUT` |
| `EVerticalState` | `eNOT_CONNECTED`, `eSOCKET_CONNECTED`, `eSUPERVISORY_SERVICE_DESCRIPTION`, `eCONNECTED`, `eDISCONNECTED` |

### Key C++ structs (HermesData.hpp)

```cpp
// ServiceDescriptionData — exchanged by both sides at connection start
struct ServiceDescriptionData {
    std::string           m_machineId;           // required
    uint32_t              m_laneId;              // required, 1-based
    Optional<std::string> m_optionalInterfaceId;
    std::string           m_version;
    // SupportedFeatures omitted for brevity
};

// BoardAvailableData — sent by Downstream to signal a board is ready
struct BoardAvailableData {
    std::string           m_boardId;               // required
    std::string           m_boardIdCreatedBy;       // required
    EBoardQuality         m_failedBoard;            // required
    EFlippedBoard         m_flippedBoard;           // required
    Optional<std::string> m_optionalProductTypeId;
    Optional<std::string> m_optionalTopBarcode;
    Optional<std::string> m_optionalBottomBarcode;
    Optional<double>      m_optionalLengthInMM;
    Optional<double>      m_optionalWidthInMM;
    Optional<double>      m_optionalThicknessInMM;
    Optional<double>      m_optionalConveyorSpeedInMMPerSecs;
    Optional<double>      m_optionalTopClearanceHeightInMM;
    Optional<double>      m_optionalBottomClearanceHeightInMM;
    Optional<double>      m_optionalWeightInGrams;
    Optional<std::string> m_optionalWorkOrderId;
    Optional<std::string> m_optionalBatchId;
    Optional<uint16_t>    m_optionalRoute;
    Optional<uint16_t>    m_optionalAction;
    SubBoards             m_optionalSubBoards;     // std::vector<SubBoard>
};

// MachineReadyData — sent by Upstream to signal it can receive a board
struct MachineReadyData {
    EBoardQuality          m_failedBoard;         // required — what quality it accepts
    Optional<std::string>  m_optionalForecastId;
    Optional<std::string>  m_optionalBoardId;
    Optional<std::string>  m_optionalProductTypeId;
    Optional<EFlippedBoard> m_optionalFlippedBoard;
    Optional<std::string>  m_optionalTopBarcode;
    Optional<std::string>  m_optionalBottomBarcode;
    Optional<double>       m_optionalLengthInMM;
    Optional<double>       m_optionalWidthInMM;
    Optional<double>       m_optionalThicknessInMM;
    Optional<double>       m_optionalConveyorSpeedInMMPerSecs;
    Optional<double>       m_optionalTopClearanceHeightInMM;
    Optional<double>       m_optionalBottomClearanceHeightInMM;
    Optional<double>       m_optionalWeightInGrams;
    Optional<std::string>  m_optionalWorkOrderId;
    Optional<std::string>  m_optionalBatchId;
};

// StartTransportData — Upstream tells Downstream to release the board
struct StartTransportData {
    std::string      m_boardId;                           // required
    Optional<double> m_optionalConveyorSpeedInMMPerSecs;  // optional override
};

// StopTransportData — Upstream confirms board received
struct StopTransportData {
    ETransferState m_transferState;  // eCOMPLETE, eINCOMPLETE, eNOT_STARTED
    std::string    m_boardId;
};

// TransportFinishedData — Downstream confirms board released
struct TransportFinishedData {
    ETransferState m_transferState;
    std::string    m_boardId;
};

// NotificationData — error or status from either side
struct NotificationData {
    ENotificationCode m_notificationCode;
    ESeverity         m_severity;
    std::string       m_description;
};

// Error — returned in disconnect callbacks and configuration calls
struct Error {
    EErrorCode  m_code;   // eSUCCESS = no error
    std::string m_text;
    explicit operator bool() const;  // true if error
};

// ConnectionInfo
struct ConnectionInfo {
    std::string m_address;
    uint16_t    m_port;
    std::string m_hostName;
};

// Settings
struct DownstreamSettings {
    std::string            m_machineId;
    Optional<std::string>  m_optionalClientAddress;  // restrict to one IP
    uint16_t               m_port{0};               // 0 = cBASE_PORT (50100)
    double                 m_checkAlivePeriodInSeconds{60};
    double                 m_reconnectWaitTimeInSeconds{10};
    ECheckAliveResponseMode m_checkAliveResponseMode{ECheckAliveResponseMode::eAUTO};
    ECheckState            m_checkState{ECheckState::eSEND_AND_RECEIVE};
};

struct UpstreamSettings {
    std::string m_machineId;
    std::string m_hostAddress;                      // downstream machine IP — required
    uint16_t    m_port{0};
    double      m_checkAlivePeriodInSeconds{60};
    double      m_reconnectWaitTimeInSeconds{10};
    ECheckAliveResponseMode m_checkAliveResponseMode{ECheckAliveResponseMode::eAUTO};
    ECheckState m_checkState{ECheckState::eSEND_AND_RECEIVE};
};
```

---

## 8. Error handling

```cpp
void OnDisconnected(unsigned sessionId, EState, const Error& error) override
{
    if (error) {  // operator bool() — true if not eSUCCESS
        std::cerr << "Error " << error.m_code << ": " << error.m_text << "\n";
        // EErrorCode values:
        // eSUCCESS              — not an error
        // eIMPLEMENTATION_ERROR — bug inside the Hermes library
        // ePEER_ERROR           — remote machine misbehaved
        // eCLIENT_ERROR         — your code called the API incorrectly
        // eNETWORK_ERROR        — TCP/IP problem
        // eTIMEOUT              — timeout exceeded
    }
}
```

Notification codes (`ENotificationCode`) are what the **protocol** sends to describe issues:

```cpp
void On(unsigned sessionId, const NotificationData& n) override
{
    // n.m_notificationCode:
    // eUNSPECIFIC
    // ePROTOCOL_ERROR
    // eCONNECTION_REFUSED_BECAUSE_OF_ESTABLISHED_CONNECTION
    // eCONNECTION_RESET_BECAUSE_OF_CHANGED_CONFIGURATION
    // eCONFIGURATION_ERROR
    // eMACHINE_SHUTDOWN
    // eBOARD_FORECAST_ERROR

    // n.m_severity: eFATAL, eERROR, eWARNING, eINFO
    // n.m_description: human-readable string
}
```

---

## 9. Thread safety rules

The Hermes library runs its own event loop thread internally.

**Safe to call from any thread:**
- `Enable()`, `Stop()`
- `Post(callable)` — schedules work onto the Hermes thread

**Must only be called from the Hermes thread** (i.e. from within a callback or from a `Post()` lambda):
- All `Signal()` methods
- `Reset()`, `Disable()`

**Trace callbacks are called from the Hermes thread** — your trace handler must be thread-safe if it writes shared state.

**Pattern for correct signal dispatch:**

```cpp
// Inside a callback (already on Hermes thread) — direct call is fine
void On(unsigned sessionId, EState, const MachineReadyData&) override
{
    BoardAvailableData board;
    board.m_boardId          = "PCB-001";
    board.m_boardIdCreatedBy = "MyMachine";
    board.m_failedBoard      = EBoardQuality::eGOOD;
    board.m_flippedBoard     = EFlippedBoard::eTOP_SIDE_IS_UP;
    m_downstream.Signal(sessionId, board);  // safe — we're on the Hermes thread
}

// From your own thread — use Post()
void SendFromMyThread(unsigned sessionId)
{
    m_downstream.Post([this, sessionId]() {
        BoardAvailableData board;
        // ... fill board ...
        m_downstream.Signal(sessionId, board);
    });
}
```

---

## 10. Complete examples

### Example A — Downstream machine (C++)

```cpp
#include "Hermes.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

static std::atomic<bool> g_running{true};

struct MyDownstream : Hermes::IDownstreamCallback
{
    Hermes::Downstream* m_ds = nullptr;

    void OnConnected(unsigned sessionId, Hermes::EState, const Hermes::ConnectionInfo& info) override {
        std::cout << "[DS] Connected: " << info.m_address << "\n";
    }
    void OnDisconnected(unsigned sessionId, Hermes::EState, const Hermes::Error& err) override {
        std::cout << "[DS] Disconnected\n";
    }
    void OnState(unsigned, Hermes::EState state) override {
        std::cout << "[DS] State: " << state << "\n";
    }
    void OnTrace(unsigned, Hermes::ETraceType, Hermes::StringView) override {}

    void On(unsigned sessionId, Hermes::EState, const Hermes::ServiceDescriptionData& data) override {
        std::cout << "[DS] ServiceDescription from: " << data.m_machineId << "\n";
        m_ds->Post([this, sessionId]() {
            Hermes::ServiceDescriptionData reply;
            reply.m_machineId = "DownstreamMachine";
            reply.m_laneId    = 1;
            m_ds->Signal(sessionId, reply);
        });
    }

    void On(unsigned sessionId, Hermes::EState, const Hermes::MachineReadyData&) override {
        std::cout << "[DS] MachineReady — sending BoardAvailable\n";
        m_ds->Post([this, sessionId]() {
            Hermes::BoardAvailableData board;
            board.m_boardId          = "PCB-001";
            board.m_boardIdCreatedBy = "DownstreamMachine";
            board.m_failedBoard      = Hermes::EBoardQuality::eGOOD;
            board.m_flippedBoard     = Hermes::EFlippedBoard::eTOP_SIDE_IS_UP;
            m_ds->Signal(sessionId, board);
        });
    }

    void On(unsigned sessionId, Hermes::EState, const Hermes::StartTransportData& data) override {
        std::cout << "[DS] StartTransport: " << data.m_boardId << "\n";
        m_ds->Post([this, sessionId, id = data.m_boardId]() {
            Hermes::TransportFinishedData fin;
            fin.m_transferState = Hermes::ETransferState::eCOMPLETE;
            fin.m_boardId       = id;
            m_ds->Signal(sessionId, fin);
        });
    }

    void On(unsigned, Hermes::EState, const Hermes::RevokeMachineReadyData&) override {}
    void On(unsigned, Hermes::EState, const Hermes::StopTransportData&) override {}
    void On(unsigned, const Hermes::NotificationData& n) override {
        std::cout << "[DS] Notification: " << n.m_description << "\n";
    }
    void On(unsigned, const Hermes::CommandData&) override {}
};

int main()
{
    std::signal(SIGINT, [](int) { g_running = false; });

    MyDownstream cb;
    Hermes::Downstream ds(1, cb);
    cb.m_ds = &ds;

    Hermes::DownstreamSettings s;
    s.m_machineId = "DownstreamMachine";
    s.m_port      = 50100;
    ds.Enable(s);

    std::thread t([&ds]() { ds.Run(); });
    std::cout << "[DS] Listening on :50100\n";
    while (g_running) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ds.Stop();
    t.join();
}
```

### Example B — Upstream machine (Modern C++)

```cpp
#include "HermesModern.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

static std::atomic<bool> g_running{true};

int main()
{
    std::signal(SIGINT, [](int) { g_running = false; });

    Hermes::Modern::Upstream us(1);

    us.RegisterConnectedCallback([](unsigned, const Hermes::ConnectionInfo& info) {
        std::cout << "[US] Connected to " << info.m_address << "\n";
    });

    us.RegisterServiceDescriptionCallback(
        [&us](unsigned sessionId, Hermes::EState, const Hermes::ServiceDescriptionData& data) {
            std::cout << "[US] ServiceDescription from: " << data.m_machineId << "\n";
            us.Post([&us, sessionId]() {
                Hermes::ServiceDescriptionData reply;
                reply.m_machineId = "UpstreamMachine";
                reply.m_laneId    = 1;
                us.Signal(sessionId, reply);

                Hermes::MachineReadyData ready;
                ready.m_failedBoard = Hermes::EBoardQuality::eANY;
                us.Signal(sessionId, ready);
                std::cout << "[US] Sent ServiceDescription + MachineReady\n";
            });
        });

    us.RegisterBoardAvailableCallback(
        [&us](unsigned sessionId, Hermes::EState, const Hermes::BoardAvailableData& board) {
            std::cout << "[US] BoardAvailable: " << board.m_boardId << "\n";
            us.Post([&us, sessionId, id = board.m_boardId]() {
                Hermes::StartTransportData start;
                start.m_boardId = id;
                us.Signal(sessionId, start);
            });
        });

    us.RegisterTransportFinishedCallback(
        [&us](unsigned sessionId, Hermes::EState, const Hermes::TransportFinishedData& data) {
            std::cout << "[US] TransportFinished: " << data.m_boardId << " — transfer complete\n";
            us.Post([&us, sessionId, id = data.m_boardId]() {
                Hermes::StopTransportData stop;
                stop.m_transferState = Hermes::ETransferState::eCOMPLETE;
                stop.m_boardId       = id;
                us.Signal(sessionId, stop);
            });
        });

    us.RegisterNotificationCallback([](unsigned, const Hermes::NotificationData& n) {
        std::cout << "[US] Notification: " << n.m_description << "\n";
    });

    Hermes::UpstreamSettings s;
    s.m_machineId   = "UpstreamMachine";
    s.m_hostAddress = "192.168.1.2";  // ← downstream machine IP
    s.m_port        = 50100;
    us.Enable(s);

    std::cout << "[US] Connecting to 192.168.1.2:50100\n";
    while (g_running) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    us.Stop();
}
```

---

## 11. Bugs fixed

### HermesStringView.h
- Missing `#include <stddef.h>` — caused `size_t` to be undefined, breaking every dependent file.
- `m_size` field was absent from the struct — caused `HermesDataConversion.hpp` to fail on every string conversion.

### HermesOptional.hpp
- Custom `Optional<T>` replaced with `using Optional = std::optional<T>`. C++17 is required, so `std::optional` is correct.
- Added `operator<<` for `std::optional<T>` in `namespace Hermes` — required by internal `BuildString()` usage in `AsioServer.cpp` and other `.cpp` files that log `Optional<NetworkConfiguration>` values.

### HermesStringView.hpp
- Custom `StringView` replaced with `using StringView = std::string_view`. C++17 is required.

### HermesDataConversion.hpp
- **`Converter2C<e>` typo** — corrected to `Converter2C<Error>`. Was a hard link error.
- **`uint32_t` / `unsigned` redefinition** — on ARM (Raspberry Pi), `unsigned` and `uint32_t` are the same type. The separate `uint32_t` overload caused a redefinition error. Removed the duplicate overload — the `unsigned` overload covers both.
- **Wrong field names in `BoardAvailableData` converter** — used invented short names (`m_length`, `m_width`, `m_thickness`, `m_conveyorSpeed`, `m_topClearanceHeight`, `m_bottomClearanceHeight`, `m_weight`, `m_subBoards`). Corrected to actual field names: `m_optionalLengthInMM`, `m_optionalWidthInMM`, `m_optionalThicknessInMM`, `m_optionalConveyorSpeedInMMPerSecs`, `m_optionalTopClearanceHeightInMM`, `m_optionalBottomClearanceHeightInMM`, `m_optionalWeightInGrams`, `m_optionalSubBoards`.
- **Same wrong field names in `MachineReadyData` converter** — same fix applied.
- **Wrong field name in `StartTransportData` converter** — `m_conveyorSpeed` → `m_optionalConveyorSpeedInMMPerSecs`.

### HermesModern.hpp
- `Modern::Downstream::InternalCallbackWrapper` overrode the wrong message set (board messages instead of machine-control messages). Fixed to match `IDownstreamCallback` exactly.
- `Modern::Upstream::InternalCallbackWrapper` had the same problem in reverse. Fixed to match `IUpstreamCallback` exactly.
- `Stop()` was not safe to call twice — fixed using `std::atomic::exchange`.
- Callback signatures were missing `sessionId` — restored to full signature.
