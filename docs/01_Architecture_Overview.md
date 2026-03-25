# 01. Architecture Overview

The Hermes Standard (IPC-HERMES-9852) is a protocol based on TCP/IP and XML designed to replace the legacy SMEMA wiring standard. This C++ library serves as a robust wrapper around the protocol, handling the low-level socket connections, XML serialization, and the strict state machines required for compliance.

To effectively use this library, you must understand three core architectural concepts: **Machine Roles**, the **Event-Driven Model**, and **Thread Management**.

---

## 1. Machine Roles (Network Topology)

In a Hermes line, machines communicate horizontally (machine-to-machine) and vertically (machine-to-factory). The library provides dedicated classes for each role:

### Horizontal Integration (The SMT Line)
* **`Downstream` (The Receiver):** Acts as a **TCP Server**. A downstream machine (e.g., a Pick & Place) opens a specific port and listens. It waits for the previous machine in the line to connect and hand over a board.
* **`Upstream` (The Sender):** Acts as a **TCP Client**. An upstream machine (e.g., a Printer) actively connects to the IP address and port of the next machine in the line to send a board.

*Note: Most machines in the middle of a line will instantiate **both** an `Upstream` object (to send to the right) and a `Downstream` object (to receive from the left).*

### Vertical Integration (The Factory)
* **`ConfigurationService`:** Used to exchange machine capabilities and supervisory line control data.
* **`VerticalService`:** Acts as a TCP Server to provide real-time board tracking data to higher-level MES/ERP systems (often working alongside IPC-CFX).

---

## 2. The Event-Driven Model

The library is entirely asynchronous and event-driven. You do not write code that says `WaitForBoard()`. Instead, you register **Callbacks** that the library triggers when network events occur.

If you are using the modern wrapper (`HermesModern.hpp`), these events are handled via standard C++ lambdas:

```cpp
// Example: The library triggers this lambda when an XML message arrives
downstream.RegisterBoardAvailableCallback([](const Hermes::BoardAvailableData& board) {
    std::cout << "Board Arrived! Barcode: " << board._topBarcode << std::endl;
});
3. Thread Management (Critical)
Because the library utilizes Boost.ASIO for high-performance networking, it relies on a continuous event loop to process incoming socket data.

The Run() Loop: All network processing happens inside the Run() method of the Upstream or Downstream objects.

Blocking Behavior: The Run() method is blocking. If you call it on your main application thread, your application's UI or main control loop will freeze.

The Solution: You must always execute the core Hermes loop in a dedicated background std::thread.

(Note: If you use the HermesModern.hpp wrapper, this background thread is automatically managed for you when you call Enable()).