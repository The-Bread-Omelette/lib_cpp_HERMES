# 03. Modern API Basics

The legacy Hermes C++ library relies on strict class inheritance (`IDownstreamCallback` and `IUpstreamCallback`) and manual thread management. To simplify development, we provide `HermesModern.hpp`, a wrapper that allows you to use modern C++17 lambdas and automatically manages the background networking threads.

---

## 1. Instantiating the Node

Every Hermes connection requires a `laneId` (typically `1` for a standard single-lane machine). You instantiate either a `Downstream` (Receiver) or `Upstream` (Sender) object from the `Hermes::Modern` namespace.

```cpp
#include "HermesModern.hpp"

// Create a receiver (listens for the previous machine) on Lane 1
Hermes::Modern::Downstream receiver(1);

// Create a sender (connects to the next machine) on Lane 1
Hermes::Modern::Upstream sender(1);
2. Registering Callbacks
Instead of overriding pure virtual functions in a separate class, you register lambda functions directly to the object. You only need to register the callbacks your machine actually cares about.

C++
// Handle successful TCP connections
receiver.RegisterConnectedCallback([](const Hermes::ConnectionInfo& info) {
    std::cout << "Connected to: " << info.m_hostAddress << "\n";
});

// Handle incoming board data
receiver.RegisterBoardAvailableCallback([](const Hermes::BoardAvailableData& board) {
    std::cout << "Incoming Board ID: " << board.m_boardId << "\n";
});

// Handle disconnection or errors
receiver.RegisterDisconnectedCallback([](const Hermes::Error& err) {
    std::cout << "Disconnected. Reason: " << err.m_text << "\n";
});
3. Configuration and Execution (Enable)
To start listening (Downstream) or actively connecting (Upstream), you must populate a settings object and pass it to the Enable() method.

Crucially, calling Enable() in the modern wrapper automatically spawns the required background std::thread to process network events. You do not need to manage the blocking Run() loop yourself.

C++
Hermes::DownstreamSettings settings;
settings.m_machineId = "My_Pick_And_Place";
settings.m_clientAddress = "192.168.1.100"; // Accept connections from this IP
settings.m_port = 50101; // Standard Hermes Default Port

// Start the network event loop in the background
receiver.Enable(settings);
4. Shutting Down (Stop)
When your application is closing, or if you need to sever the connection to change line configurations, call Stop(). This will cleanly disconnect the socket, send the appropriate XML termination messages, and safely join the background thread.

C++
// Safely shut down the connection and stop the background thread
receiver.Stop();