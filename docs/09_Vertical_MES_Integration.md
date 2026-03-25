# 09. Vertical Integration (MES/ERP)

While `Upstream` and `Downstream` handle **Horizontal** communication (machine passing boards to machine), the Hermes Standard also defines a **Vertical** channel. This allows higher-level factory software (like an MES, ERP, or line controller) to monitor and control the machines in real-time.

In the ASM `lib_cpp` library, this is handled by the `VerticalService` and `VerticalClient` classes.

---

## 1. The Architecture

In a standard smart factory setup:
* **The Machine (You):** Acts as the **`VerticalService`** (TCP Server). You open a port (often `1248` or `50100`) and wait.
* **The MES/Factory Cloud:** Acts as the **`VerticalClient`** (TCP Client). It connects to every machine on the line to gather data.

---

## 2. The Vertical Handshake

When an MES connects to your machine, you must exchange supervisory descriptions. This is entirely separate from the machine-to-machine horizontal handshake.

1. **MES Connects:** Your `OnConnected` callback fires.
2. **Exchange Descriptions:** The MES sends its `SupervisoryServiceDescriptionData`. Your machine responds with its own, detailing what features it supports (e.g., `FeatureBoardTracking`, `FeatureQueryWorkOrderInfo`).

---

## 3. Real-Time Board Tracking

The most common use case for Vertical Integration is letting the factory know exactly where every PCB is located. If the MES enables `FeatureBoardTracking`, your machine is responsible for firing off two specific messages during its operation:

### A. `BoardArrivedData`
You must signal this to the `VerticalService` the moment a board successfully enters your machine.
**Key Fields:**
* `m_machineId`: Your machine's ID.
* `m_boardId`: The UUID of the board that just entered.
* `m_boardTransfer`: How it got there (e.g., `EBoardArrivedTransfer::eTRANSFERRED` from an upstream machine, or `eLOADED` if an operator manually placed it).

### B. `BoardDepartedData`
You must signal this the moment a board completely leaves your machine.
**Key Fields:**
* `m_machineId`: Your machine's ID.
* `m_boardId`: The UUID of the board.
* `m_boardTransfer`: How it left (e.g., `EBoardDepartedTransfer::eTRANSFERRED` to a downstream machine, or `eREMOVED` if an operator took it out).

---

## 4. Advanced: Work Orders and Routing

If your machine supports dynamic recipes or routing, the Vertical channel allows you to ask the MES what to do with a board.

1. **Query:** When a board arrives, your machine sends a `QueryWorkOrderInfoData` containing the board's barcode.
2. **Reply:** The MES responds with `ReplyWorkOrderInfoData`, telling your machine which recipe/program to load for that specific barcode.
3. **Route:** The MES can also inject `m_route` attributes, telling a sorting conveyor whether to pass the board through, send it to an inspection queue, or reject it to a scrap bin.

---

## 5. Raw Implementation Example

If you are using the raw C++ interface (bypassing lambdas), your vertical implementation will look like this:

```cpp
#include "Hermes.hpp"

class MyLineMonitor : public Hermes::IVerticalServiceCallback {
public:
    MyLineMonitor() : m_verticalService(*this) {}

    void Start() {
        Hermes::VerticalServiceSettings settings("My_Machine", 1248);
        m_verticalService.Enable(settings);
        // Ensure you run m_verticalService.Run() in a background thread!
    }

    // --- Override the pure virtuals ---
    void OnConnected(unsigned sessionId, Hermes::EVerticalState state, const Hermes::ConnectionInfo& info) override {
        // MES has connected to us
    }

    void On(unsigned sessionId, Hermes::EVerticalState state, const Hermes::SupervisoryServiceDescriptionData& data) override {
        // MES told us who it is. We can now reply with our capabilities.
    }
    
    // ... Implement remaining virtuals ...

private:
    Hermes::VerticalService m_verticalService;
};