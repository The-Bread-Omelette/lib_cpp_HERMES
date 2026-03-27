
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
    settings.m_hostAddress = "10.0.0.2";  // <-- downstream machine IP
    settings.m_port        = 50100;
    us.Enable(settings);

    std::cout << "[US] Connecting to :50100. Ctrl+C to stop.\n";
    while (g_running)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    us.Stop();
    return 0;
}