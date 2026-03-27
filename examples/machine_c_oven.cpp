#include <iostream>
#include <string>
#include "HermesModern.hpp"

int main() {
    std::cout << "========================================\n";
    std::cout << " [MACHINE C] - REFLOW OVEN (Receiver)\n";
    std::cout << " Connecting to Port 50102...\n";
    std::cout << "========================================\n\n";

    // Receivers face UPSTREAM to connect to the previous machine
    Hermes::Modern::Upstream receiver(1);
    
    receiver.RegisterServiceDescriptionCallback([&](const Hermes::ServiceDescriptionData& data) {
        std::cout << "[C] Connected to " << data.m_machineId << ". Ready for boards.\n";
        receiver.Signal(1, Hermes::MachineReadyData(Hermes::EBoardQuality::eANY));
    });

    receiver.RegisterBoardAvailableCallback([&](const Hermes::BoardAvailableData& board) {
        std::cout << "[C] Board detected at entrance: " << board.m_boardId << "\n";
        std::cout << "[C] Starting conveyors to pull it in...\n";
        receiver.Signal(1, Hermes::StartTransportData(board.m_boardId));
    });

    receiver.RegisterTransportFinishedCallback([&](const Hermes::TransportFinishedData& data) {
        receiver.Signal(1, Hermes::StopTransportData(Hermes::ETransferState::eCOMPLETE, data.m_boardId));
        std::cout << "[C] Board " << data.m_boardId << " is fully inside!\n";
        std::cout << "[C] Baking... Handover cycle complete.\n\n";
        
        receiver.Signal(1, Hermes::MachineReadyData(Hermes::EBoardQuality::eANY));
    });

    // Client connects to the Sender
    Hermes::UpstreamSettings settings("Machine_C_Oven", "127.0.0.1", 50102);
    receiver.Enable(settings);

    std::cout << "Press Enter to shut down Machine C...\n\n";
    std::cin.get();
    receiver.Stop();
    return 0;
}