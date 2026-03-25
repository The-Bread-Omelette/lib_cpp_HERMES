
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