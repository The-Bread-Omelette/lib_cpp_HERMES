// =============================================================================
// rpi2_downstream.cpp  —  RPi2: Downstream machine
//
// Build:
//   g++ -std=c++17 -o rpi2_downstream rpi2_downstream.cpp \
//       -I/path/to/hermes/src/include \
//       -L/path/to/hermes/build -lhermes \
//       -lboost_system -lpthread
//
// Run:
//   LD_LIBRARY_PATH=/path/to/hermes/build ./rpi2_downstream
//
// What it does:
//   - Listens on port 50100 for an incoming connection from RPi1
//   - Receives ServiceDescription from RPi1
//   - Sends ServiceDescription back to RPi1
//   - Prints connection state changes to console
// =============================================================================

#include "Hermes.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

static const uint16_t    HERMES_PORT      = 50100;
static const std::string THIS_MACHINE_ID  = "RPi2-Downstream";

static std::atomic<bool>  g_running{true};
static unsigned           g_activeSession{0};

// Forward declare so callback can reference it
Hermes::Downstream* g_downstream = nullptr;

void signalHandler(int) { g_running = false; }

// -----------------------------------------------------------------------
// Downstream callback — receives messages FROM the Upstream (RPi1)
// -----------------------------------------------------------------------
struct DownstreamCallback : Hermes::IDownstreamCallback
{
    void OnConnected(unsigned sessionId, Hermes::EState state,
                     const Hermes::ConnectionInfo& info) override
    {
        g_activeSession = sessionId;
        std::cout << "[RPi2] RPi1 connected"
                  << "  address=" << info.m_address
                  << "  port="    << info.m_port
                  << "  session=" << sessionId
                  << "\n";
    }

    // RPi1 sends its ServiceDescription — we receive it here
    void On(unsigned sessionId, Hermes::EState state,
            const Hermes::ServiceDescriptionData& data) override
    {
        std::cout << "[RPi2] Received ServiceDescription from RPi1"
                  << "  machineId=" << data.m_machineId
                  << "  laneId="    << data.m_laneId
                  << "\n";

        // Respond with our own ServiceDescription
        if (g_downstream)
        {
            g_downstream->Post([sessionId]() {
                Hermes::ServiceDescriptionData reply;
                reply.m_machineId = THIS_MACHINE_ID;
                reply.m_laneId    = 1;

                std::cout << "[RPi2] Sending ServiceDescription back to RPi1\n";
                g_downstream->Signal(sessionId, reply);
                std::cout << "[RPi2] ** Hermes connection established successfully **\n";
            });
        }
    }

    // These are required by the interface — not used in this demo
    void On(unsigned, Hermes::EState, const Hermes::MachineReadyData&) override {}
    void On(unsigned, Hermes::EState, const Hermes::RevokeMachineReadyData&) override {}
    void On(unsigned, Hermes::EState, const Hermes::StartTransportData&) override {}
    void On(unsigned, Hermes::EState, const Hermes::StopTransportData&) override {}

    void On(unsigned sessionId, const Hermes::NotificationData& data) override
    {
        std::cout << "[RPi2] Notification from RPi1: " << data.m_description << "\n";
    }

    void On(unsigned, const Hermes::CheckAliveData&) override {}

    void On(unsigned, const Hermes::CommandData&) override {}

    void OnState(unsigned sessionId, Hermes::EState state) override
    {
        std::cout << "[RPi2] State changed: " << static_cast<int>(state) << "\n";
    }

    void OnDisconnected(unsigned sessionId, Hermes::EState state,
                        const Hermes::Error& error) override
    {
        std::cout << "[RPi2] RPi1 disconnected";
        if (error)
            std::cout << "  reason=" << error.m_text;
        std::cout << "\n";
        g_activeSession = 0;
    }

    void OnTrace(unsigned, Hermes::ETraceType, Hermes::StringView) override
    {
        // Uncomment to see full protocol trace:
        // std::cout << "[RPi2][TRACE] " << std::string(trace) << "\n";
    }
};

int main()
{
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "[RPi2] Starting Hermes Downstream\n";
    std::cout << "[RPi2] Listening on port " << HERMES_PORT
              << " for RPi1...\n";

    DownstreamCallback callback;
    Hermes::Downstream downstream(1, callback);  // lane 1
    g_downstream = &downstream;

    // Settings: who we are, and which port to listen on
    // m_optionalClientAddress is left empty = accept from any IP
    Hermes::DownstreamSettings settings;
    settings.m_machineId   = THIS_MACHINE_ID;
    settings.m_port        = HERMES_PORT;
    settings.m_checkAlivePeriodInSeconds  = 60.0;
    settings.m_reconnectWaitTimeInSeconds = 5.0;

    downstream.Enable(settings);

    // Run Hermes event loop in a background thread
    std::thread networkThread([&downstream]() {
        downstream.Run();
    });

    std::cout << "[RPi2] Running. Press Ctrl+C to stop.\n";

    while (g_running)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "[RPi2] Shutting down...\n";
    downstream.Stop();
    if (networkThread.joinable())
        networkThread.join();

    g_downstream = nullptr;
    std::cout << "[RPi2] Done.\n";
    return 0;
}
