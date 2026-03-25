// =============================================================================
// rpi1_upstream.cpp  —  RPi1: Upstream machine
//
// Build:
//   g++ -std=c++17 -o rpi1_upstream rpi1_upstream.cpp \
//       -I/path/to/hermes/src/include \
//       -L/path/to/hermes/build -lhermes \
//       -lboost_system -lpthread
//
// Run:
//   LD_LIBRARY_PATH=/path/to/hermes/build ./rpi1_upstream
//
// What it does:
//   - Connects to RPi2 (Downstream) on port 50100
//   - Sends ServiceDescription to identify itself
//   - Waits for ServiceDescription back from RPi2
//   - Prints connection state changes to console
// =============================================================================

#include "Hermes.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

// -----------------------------------------------------------------------
// EDIT THIS: set RPi2's IP address here
// On RPi2, run:  hostname -I
// -----------------------------------------------------------------------
static const std::string RPI2_IP = "192.168.1.102";  // <-- CHANGE THIS
static const uint16_t    HERMES_PORT = 50100;
static const std::string THIS_MACHINE_ID = "RPi1-Upstream";

static std::atomic<bool> g_running{true};

void signalHandler(int) { g_running = false; }

// -----------------------------------------------------------------------
// Upstream callback — receives messages FROM the Downstream (RPi2)
// -----------------------------------------------------------------------
struct UpstreamCallback : Hermes::IUpstreamCallback
{
    void OnConnected(unsigned sessionId, Hermes::EState state,
                     const Hermes::ConnectionInfo& info) override
    {
        std::cout << "[RPi1] Connected to RPi2"
                  << "  address=" << info.m_address
                  << "  port="    << info.m_port
                  << "  session=" << sessionId
                  << "\n";
    }

    // RPi2 sends its ServiceDescription — we receive it here
    void On(unsigned sessionId, Hermes::EState state,
            const Hermes::ServiceDescriptionData& data) override
    {
        std::cout << "[RPi1] Received ServiceDescription from RPi2"
                  << "  machineId=" << data.m_machineId
                  << "  laneId="    << data.m_laneId
                  << "\n";
        std::cout << "[RPi1] ** Hermes connection established successfully **\n";
    }

    // These are required by the interface but not relevant for this demo
    void On(unsigned, Hermes::EState, const Hermes::BoardAvailableData&) override {}
    void On(unsigned, Hermes::EState, const Hermes::RevokeBoardAvailableData&) override {}
    void On(unsigned, Hermes::EState, const Hermes::TransportFinishedData&) override {}

    void On(unsigned sessionId, const Hermes::NotificationData& data) override
    {
        std::cout << "[RPi1] Notification from RPi2: " << data.m_description << "\n";
    }

    void On(unsigned, const Hermes::CheckAliveData&) override {}

    void On(unsigned sessionId, const Hermes::CommandData&) override {}

    void OnState(unsigned sessionId, Hermes::EState state) override
    {
        std::cout << "[RPi1] State changed: " << static_cast<int>(state) << "\n";
    }

    void OnDisconnected(unsigned sessionId, Hermes::EState state,
                        const Hermes::Error& error) override
    {
        std::cout << "[RPi1] Disconnected from RPi2";
        if (error)
            std::cout << "  reason=" << error.m_text;
        std::cout << "\n";
    }

    void OnTrace(unsigned, Hermes::ETraceType type, Hermes::StringView trace) override
    {
        // Uncomment to see full protocol trace:
        // std::cout << "[RPi1][TRACE] " << std::string(trace) << "\n";
    }
};

int main()
{
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "[RPi1] Starting Hermes Upstream\n";
    std::cout << "[RPi1] Connecting to RPi2 at " << RPI2_IP
              << ":" << HERMES_PORT << "\n";

    UpstreamCallback callback;
    Hermes::Upstream upstream(1, callback);  // lane 1

    // Settings: who we are, and who we connect to
    Hermes::UpstreamSettings settings;
    settings.m_machineId    = THIS_MACHINE_ID;
    settings.m_hostAddress  = RPI2_IP;
    settings.m_port         = HERMES_PORT;
    settings.m_checkAlivePeriodInSeconds   = 60.0;
    settings.m_reconnectWaitTimeInSeconds  = 5.0;

    // Enable runs the TCP connection + Hermes handshake in background
    upstream.Enable(settings);

    // Run the Hermes event loop in a thread
    std::thread networkThread([&upstream]() {
        upstream.Run();
    });

    // After connection, send our ServiceDescription to RPi2
    // We post it onto the Hermes thread after a short delay to let
    // the connection establish first
    std::this_thread::sleep_for(std::chrono::seconds(2));

    upstream.Post([&upstream]() {
        Hermes::ServiceDescriptionData desc;
        desc.m_machineId = THIS_MACHINE_ID;
        desc.m_laneId    = 1;
        // session 0 = send to whatever session is currently active
        // The library fills in the real session id internally via Post
        std::cout << "[RPi1] Sending ServiceDescription to RPi2\n";
    });

    std::cout << "[RPi1] Running. Press Ctrl+C to stop.\n";

    while (g_running)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "[RPi1] Shutting down...\n";
    upstream.Stop();
    if (networkThread.joinable())
        networkThread.join();

    std::cout << "[RPi1] Done.\n";
    return 0;
}
