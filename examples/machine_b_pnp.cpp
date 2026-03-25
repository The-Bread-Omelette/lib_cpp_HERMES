#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "HermesModern.hpp"

int main() {
    std::cout << "========================================\n";
    std::cout << " [MACHINE B] - PICK & PLACE\n";
    std::cout << " Connecting to 50101 (A) | Listening on 50102 (C)\n";
    std::cout << "========================================\n\n";

    // Receiver connects UPSTREAM to A. Sender listens DOWNSTREAM for C.
    Hermes::Modern::Upstream receiver(1); 
    Hermes::Modern::Downstream sender(1);

    // --- RECEIVING FROM MACHINE A (Upstream) ---
    receiver.RegisterServiceDescriptionCallback([&](const Hermes::ServiceDescriptionData& data) {
        std::cout << "[B] Upstream " << data.m_machineId << " connected. I am ready.\n";
        receiver.Signal(1, Hermes::MachineReadyData(Hermes::EBoardQuality::eANY));
    });

    receiver.RegisterBoardAvailableCallback([&](const Hermes::BoardAvailableData& board) {
        std::cout << "[B] Board " << board.m_boardId << " waiting at upstream edge. Pulling...\n";
        receiver.Signal(1, Hermes::StartTransportData(board.m_boardId));
    });

    receiver.RegisterTransportFinishedCallback([&](const Hermes::TransportFinishedData& data) {
        receiver.Signal(1, Hermes::StopTransportData(Hermes::ETransferState::eCOMPLETE, data.m_boardId));
        std::cout << "[B] Board " << data.m_boardId << " received. Placing components...\n";
        
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::cout << "[B] Placement done! Passing to Machine C...\n";
        
        sender.Signal(1, Hermes::BoardAvailableData(data.m_boardId, "Machine_B_PnP", Hermes::EBoardQuality::eGOOD, Hermes::EFlippedBoard::eTOP_SIDE_IS_UP));
    });

    // --- SENDING TO MACHINE C (Downstream) ---
    sender.RegisterConnectedCallback([&](const Hermes::ConnectionInfo& info) {
        std::cout << "[B] Connected to Downstream at " << info.m_address << "\n";
        sender.Signal(1, Hermes::ServiceDescriptionData("Machine_B_PnP", 1));
    });

    sender.RegisterMachineReadyCallback([&](const Hermes::MachineReadyData&) {
        std::cout << "[B] Machine C is ready for boards.\n";
    });

    sender.RegisterStartTransportCallback([&](const Hermes::StartTransportData& data) {
        std::cout << "[B] Machine C is pulling. Running exit conveyors...\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        sender.Signal(1, Hermes::TransportFinishedData(Hermes::ETransferState::eCOMPLETE, data.m_boardId));
        std::cout << "[B] Board successfully handed off to Machine C.\n\n";
        
        receiver.Signal(1, Hermes::MachineReadyData(Hermes::EBoardQuality::eANY));
    });

    // Start Server first, then connect Client
    sender.Enable(Hermes::DownstreamSettings("Machine_B_PnP", 50102));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    receiver.Enable(Hermes::UpstreamSettings("Machine_B_PnP", "127.0.0.1", 50101));

    std::cout << "Press Enter to shut down Machine B...\n\n";
    std::cin.get();
    receiver.Stop();
    sender.Stop();
    return 0;
}