#include "Hermes.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <termios.h>
#include <unistd.h>
#include <string>

static std::atomic<bool> g_running{true};
static unsigned g_activeSession = 0;
static std::atomic<int> g_currentStateInt{0};
static int g_counter = 100;
static std::string g_currentBoardId = "";

void set_raw_mode(bool enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

bool kbhit() {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}

struct DownstreamCallback : Hermes::IDownstreamCallback {
    Hermes::Downstream* m_pDownstream = nullptr;

    void OnConnected(unsigned sessionId, Hermes::EState, const Hermes::ConnectionInfo&) override {
        g_activeSession = sessionId;
        std::cout << "\n[RPi2] CONNECTION ESTABLISHED.\n";
    }

    void On(unsigned sessionId, Hermes::EState, const Hermes::ServiceDescriptionData&) override {
        m_pDownstream->Post([this, sessionId]() {
            Hermes::ServiceDescriptionData reply;
            reply.m_machineId = "RPi2_Downstream";
            m_pDownstream->Signal(sessionId, reply);
        });
    }

    void OnState(unsigned, Hermes::EState s) override { 
        g_currentStateInt = static_cast<int>(s);
        std::cout << "[RPi2] State Transition: " << g_currentStateInt << "\n"; 
    }

    void On(unsigned, Hermes::EState, const Hermes::StartTransportData&) override {
        std::cout << "[RPi2] === TRANSPORT ENGAGED (Motors ON) ===\n";
    }

    void OnDisconnected(unsigned, Hermes::EState, const Hermes::Error& e) override { 
        g_activeSession = 0; std::cout << "[RPi2] Disconnected: " << e.m_text << "\n";
    }

    void On(unsigned, Hermes::EState, const Hermes::MachineReadyData&) override { std::cout << "[RPi2] Upstream Node is READY.\n"; }
    void On(unsigned, Hermes::EState, const Hermes::StopTransportData&) override { std::cout << "[RPi2] StopTransport Received. Hardware Reset Verified.\n"; }
    void On(unsigned, const Hermes::NotificationData&) override {}
    void On(unsigned, const Hermes::CheckAliveData&) override {}
    void On(unsigned, const Hermes::CommandData&) override {}
    void OnTrace(unsigned, Hermes::ETraceType, Hermes::StringView) override {}
    void On(unsigned, Hermes::EState, const Hermes::RevokeMachineReadyData&) override {}
};

int main() {
    std::signal(SIGINT, [](int) { g_running = false; });
    set_raw_mode(true);
    
    DownstreamCallback callback;
    Hermes::Downstream downstream(1, callback);
    callback.m_pDownstream = &downstream;
    
    Hermes::DownstreamSettings settings;
    settings.m_machineId = "RPi2_Downstream";
    settings.m_port = 50100;
    
    downstream.Enable(settings);
    std::thread netThread([&]() { downstream.Run(); });

    std::cout << "Downstream Node Active. Tap 'b' (BoardAvailable) or 't' (TransportFinished).\n";

    while (g_running) {
        if (kbhit()) {
            char c = getchar();
            if (g_activeSession != 0) {
                if (c == 'b' || c == 'B') {
                    g_currentBoardId = "BOARD_" + std::to_string(++g_counter);
                    downstream.Post([&downstream]() {
                        Hermes::BoardAvailableData ba;
                        ba.m_boardId = g_currentBoardId;
                        downstream.Signal(g_activeSession, ba);
                        std::cout << "[RPi2] Sent BoardAvailable: " << g_currentBoardId << "\n";
                    });
                } 
                else if (c == 't' || c == 'T') {
                    downstream.Post([&downstream]() {
                        Hermes::TransportFinishedData tf;
                        tf.m_boardId = g_currentBoardId; 
                        tf.m_transferState = Hermes::ETransferState::eCOMPLETE;
                        downstream.Signal(g_activeSession, tf);
                        std::cout << "[RPi2] Sent TransportFinished for " << g_currentBoardId << "\n";
                    });
                } 
                else if (c == 'q' || c == 'Q') g_running = false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    set_raw_mode(false);
    downstream.Stop();
    if (netThread.joinable()) netThread.join();
    return 0;
}
