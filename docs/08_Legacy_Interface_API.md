# 08. Legacy Interface API (Advanced)

While the `HermesModern.hpp` wrapper simplifies development using lambdas, advanced users may prefer to interact directly with the raw C++ library to minimize overhead or to integrate Hermes deeply into an existing class hierarchy.

To use the raw API, you must inherit from the core interfaces and manage the blocking event loops manually.

---

## 1. The Core Interfaces

The library provides two primary abstract classes:
* `Hermes::IDownstreamCallback`
* `Hermes::IUpstreamCallback`

If you inherit directly from these, your compiler will force you to implement every pure virtual method (e.g., `OnConnected`, `OnState`, and the various overloaded `On` methods for data). 

---

## 2. The Naming Collision Problem

In a real factory, most machines sit in the middle of the line. This means your application class will likely need to act as both a Sender and a Receiver. 

If you try to inherit from both base interfaces simultaneously, you will encounter **ambiguous function names**. For example, both interfaces define this exact method:
`virtual void On(unsigned sessionId, EState state, const ServiceDescriptionData& data) = 0;`

The compiler won't know if you are receiving a service description from the previous machine or the next machine.

---

## 3. The "Helper" Solution

To solve the naming collision, the library provides two specialized helper structs in `Hermes.hpp`:
* `Hermes::UpstreamCallbackHelper`
* `Hermes::DownstreamCallbackHelper`

These helpers internally catch the ambiguous `On()` methods and reroute them to uniquely named functions like `OnUpstream(...)` and `OnDownstream(...)`.

---

## 4. Legacy Implementation Example

Here is how you use the helpers to build a machine that sends and receives simultaneously using the raw API:

```cpp
#include "Hermes.hpp"
#include <thread>
#include <iostream>

// 1. Inherit from both Helpers, NOT the base interfaces
class MySmtMachine : public Hermes::UpstreamCallbackHelper, 
                     public Hermes::DownstreamCallbackHelper 
{
public:
    // 2. Instantiate the core objects, passing `*this` as the callback reference
    MySmtMachine() : m_upstream(1, *this), m_downstream(1, *this) {}

    void Start() {
        // 3. YOU must manage the blocking threads manually
        m_upThread = std::thread([this]() { m_upstream.Run(); });
        m_downThread = std::thread([this]() { m_downstream.Run(); });

        Hermes::UpstreamSettings upSettings("MyMachine", "192.168.1.51", 50101);
        Hermes::DownstreamSettings downSettings("MyMachine", 50101);

        m_upstream.Enable(upSettings);
        m_downstream.Enable(downSettings);
    }

    void Stop() {
        m_upstream.Stop();
        m_downstream.Stop();
        if (m_upThread.joinable()) m_upThread.join();
        if (m_downThread.joinable()) m_downThread.join();
    }

protected:
    // 4. Implement the uniquely named virtual methods
    void OnUpstreamConnected(unsigned sessionId, Hermes::EState state, const Hermes::ConnectionInfo& info) override {
        std::cout << "Sender Connected to next machine!\n";
    }

    void OnDownstreamConnected(unsigned sessionId, Hermes::EState state, const Hermes::ConnectionInfo& info) override {
        std::cout << "Receiver Accepted connection from previous machine!\n";
    }

    // ... You must implement all remaining OnUpstream and OnDownstream virtuals ...

private:
    Hermes::Upstream m_upstream;
    Hermes::Downstream m_downstream;
    std::thread m_upThread;
    std::thread m_downThread;
};