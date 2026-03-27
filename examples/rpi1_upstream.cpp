#include "Hermes.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <termios.h>
#include <unistd.h>

static std::atomic<bool> g_running{true};
static unsigned g_activeSession = 0;
static std::atomic<int> g_currentStateInt{0};

bool kbhit() {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
}

struct UpstreamCallback : Hermes::IUpstreamCallback {
    Hermes::Upstream* m_pUpstream = nullptr;

    void OnConnected(unsigned sessionId, Hermes::EState, const Hermes::ConnectionInfo&) override {
        g_activeSession = sessionId;
        m_pUpstream->Post([this, sessionId]() {
            Hermes::ServiceDescriptionData data;
            data.m_machineId = "RPi1_Upstream";
            m_pUpstream->Signal(sessionId, data);
        });
    }

    void On(unsigned sessionId, Hermes::EState, const Hermes::BoardAvailableData& b) override {
        std::cout << "\n[RPi1] BOARD DETECTED: " << b.m_boardId << "\n";
        m_pUpstream->Post([this, sessionId, b]() {
            Hermes::StartTransportData st;
            st.m_boardId = b.m_boardId;
            m_pUpstream->Signal(sessionId, st);
            std::cout << "[RPi1] Auto-Sent: StartTransport\n";
        });
    }

    void On(unsigned sessionId, Hermes::EState, const Hermes::TransportFinishedData& tf) override {
        std::cout << "[RPi1] RPi2 Transfer Complete. Executing Loop Reset...\n";
        m_pUpstream->Post([this, sessionId, tf]() {
            Hermes::StopTransportData stop;
            stop.m_boardId = tf.m_boardId;
            stop.m_transferState = Hermes::ETransferState::eCOMPLETE;
            m_pUpstream->Signal(sessionId, stop);
            std::cout << "[RPi1] Sent StopTransport. System Ready for Next Cycle.\n";
        });
    }

    void OnState(unsigned, Hermes::EState s) override { 
        g_currentStateInt = static_cast<int>(s);
        std::cout << "[RPi1] State Transition: " << g_currentStateInt << "\n"; 
    }

    void OnDisconnected(unsigned, Hermes::EState, const Hermes::Error& e) override { 
        g_activeSession = 0; std::cout << "[RPi1] Disconnected: " << e.m_text << "\n";
    }

    void On(unsigned, Hermes::EState, const Hermes::ServiceDescriptionData&) override {}
    void On(unsigned, Hermes::EState, const Hermes::RevokeBoardAvailableData&) override {}
    void On(unsigned, const Hermes::NotificationData&) override {}
    void On(unsigned, const Hermes::CheckAliveData&) override {}
    void On(unsigned, const Hermes::CommandData&) override {}
    void OnTrace(unsigned, Hermes::ETraceType, Hermes::StringView) override {}
};

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

int main() {
    std::signal(SIGINT, [](int) { g_running = false; });
    set_raw_mode(true);
    
    UpstreamCallback callback;
    Hermes::Upstream upstream(1, callback);
    callback.m_pUpstream = &upstream;
    
    Hermes::UpstreamSettings settings;
    settings.m_machineId = "RPi1_Upstream";
    settings.m_hostAddress = "10.0.0.2"; 
    settings.m_port = 50100;
    
    upstream.Enable(settings);
    std::thread netThread([&]() { upstream.Run(); });

    std::cout << "Upstream Node Active. Tap 'r' to signal MachineReady.\n";

    while (g_running) {
        if (kbhit()) {
            char c = getchar();
            if (g_activeSession != 0) {
                if (c == 'r' || c == 'R') {
                    std::cout << "[RPi1] Manual Override: Sending Ready...\n";
                    upstream.Post([&upstream]() {
                        Hermes::MachineReadyData mr;
                        upstream.Signal(g_activeSession, mr);
                    });
                } else if (c == 'q' || c == 'Q') g_running = false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    set_raw_mode(false);
    upstream.Stop();
    if (netThread.joinable()) netThread.join();
    return 0;
}
