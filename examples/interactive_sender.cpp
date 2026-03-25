#include <iostream>
#include <string>
#include "HermesModern.hpp"

void PrintMenu() {
    std::cout << "\n=========================================\n";
    std::cout << " INTERACTIVE HERMES SENDER CONSOLE\n";
    std::cout << "=========================================\n";
    std::cout << " Type a command and press Enter:\n";
    std::cout << "   service      -> Send ServiceDescription\n";
    std::cout << "   board <id>   -> Send BoardAvailable (e.g., board PCB-123)\n";
    std::cout << "   start <id>   -> Send TransportFinished (e.g., start PCB-123)\n";
    std::cout << "   exit         -> Shut down and close\n";
    std::cout << "=========================================\n";
}

int main() {
    // Senders face DOWNSTREAM and act as TCP Servers
    Hermes::Modern::Downstream sender(1);

    sender.RegisterConnectedCallback([&](const Hermes::ConnectionInfo& info) {
        std::cout << "\n[NETWORK] Connected to Receiver at " << info.m_address << "!\n> ";
    });

    sender.RegisterMachineReadyCallback([&](const Hermes::MachineReadyData& data) {
        std::cout << "\n[RECEIVER SAYS] I am MachineReady! Send me a board.\n> ";
    });

    sender.RegisterStartTransportCallback([&](const Hermes::StartTransportData& data) {
        std::cout << "\n[RECEIVER SAYS] StartTransport for " << data.m_boardId << "! My conveyors are running.\n> ";
    });

    sender.RegisterStopTransportCallback([&](const Hermes::StopTransportData& data) {
        std::cout << "\n[RECEIVER SAYS] StopTransport for " << data.m_boardId << ". I have the board completely.\n> ";
    });

    std::cout << "Opening Server on port 50101...\n";
    sender.Enable(Hermes::DownstreamSettings("Interactive_Sender", 50101));

    PrintMenu();
    std::string command;
    
    while (true) {
        std::cout << "> ";
        std::cin >> command;

        if (command == "exit") {
            break;
        } 
        else if (command == "service") {
            sender.Signal(1, Hermes::ServiceDescriptionData("Interactive_Sender", 1));
            std::cout << "[SENT] ServiceDescriptionData\n";
        } 
        else if (command == "board") {
            std::string boardId;
            std::cin >> boardId;
            sender.Signal(1, Hermes::BoardAvailableData(boardId, "Interactive_Sender", Hermes::EBoardQuality::eGOOD, Hermes::EFlippedBoard::eTOP_SIDE_IS_UP));
            std::cout << "[SENT] BoardAvailableData for " << boardId << "\n";
        } 
        else if (command == "start") {
            std::string boardId;
            std::cin >> boardId;
            sender.Signal(1, Hermes::TransportFinishedData(Hermes::ETransferState::eCOMPLETE, boardId));
            std::cout << "[SENT] TransportFinishedData for " << boardId << "\n";
        } 
        else {
            std::cout << "[ERROR] Unknown command.\n";
            std::cin.clear();
            std::cin.ignore(10000, '\n');
        }
    }

    sender.Stop();
    return 0;
}
