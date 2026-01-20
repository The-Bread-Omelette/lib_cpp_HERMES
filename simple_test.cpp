// Simple Hermes Library Test
// No external dependencies except Hermes itself
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include "Hermes.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

// Simple event for synchronization
class SimpleEvent {
#ifdef _WIN32
    HANDLE handle;
public:
    SimpleEvent() { handle = CreateEvent(NULL, TRUE, FALSE, NULL); }
    ~SimpleEvent() { CloseHandle(handle); }
    void Set() { SetEvent(handle); }
    void Wait() { WaitForSingleObject(handle, INFINITE); }
#else
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool signaled;
public:
    SimpleEvent() : signaled(false) {
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);
    }
    ~SimpleEvent() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }
    void Set() {
        pthread_mutex_lock(&mutex);
        signaled = true;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }
    void Wait() {
        pthread_mutex_lock(&mutex);
        while (!signaled) pthread_cond_wait(&cond, &mutex);
        pthread_mutex_unlock(&mutex);
    }
#endif
};

// Global events for test synchronization
SimpleEvent downstreamConnected;
SimpleEvent upstreamConnected;

// Callbacks
void OnDownstreamConnected(void*, uint32_t sessionId, EHermesState, const HermesConnectionInfo*) {
    std::cout << "[DOWNSTREAM] Connected - Session " << sessionId << std::endl;
    downstreamConnected.Set();
}

void OnUpstreamConnected(void*, uint32_t sessionId, EHermesState, const HermesConnectionInfo*) {
    std::cout << "[UPSTREAM] Connected - Session " << sessionId << std::endl;
    upstreamConnected.Set();
}

void OnTrace(void*, unsigned sessionId, EHermesTraceType type, HermesStringView trace) {
    const char* typeStr = "UNKNOWN";
    switch(type) {
        case eHERMES_TRACE_TYPE_DEBUG: typeStr = "DEBUG"; break;
        case eHERMES_TRACE_TYPE_INFO: typeStr = "INFO"; break;
        case eHERMES_TRACE_TYPE_WARNING: typeStr = "WARN"; break;
        case eHERMES_TRACE_TYPE_ERROR: typeStr = "ERROR"; break;
        case eHERMES_TRACE_TYPE_SENT: typeStr = "SENT"; break;
        case eHERMES_TRACE_TYPE_RECEIVED: typeStr = "RECV"; break;
    }
    std::cout << "[" << typeStr << "] ";
    std::cout.write(trace.m_pData, trace.m_size);
    std::cout << std::endl;
}

#ifdef _WIN32
DWORD WINAPI RunDownstreamThread(void* param) {
    RunHermesDownstream(static_cast<HermesDownstream*>(param));
    return 0;
}

DWORD WINAPI RunUpstreamThread(void* param) {
    RunHermesUpstream(static_cast<HermesUpstream*>(param));
    return 0;
}
#else
void* RunDownstreamThread(void* param) {
    RunHermesDownstream(static_cast<HermesDownstream*>(param));
    return nullptr;
}

void* RunUpstreamThread(void* param) {
    RunHermesUpstream(static_cast<HermesUpstream*>(param));
    return nullptr;
}
#endif

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Hermes Library Self-Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Create downstream (server)
    std::cout << "[1/5] Creating downstream connection..." << std::endl;
    HermesDownstreamCallbacks downCallbacks = {};
    downCallbacks.m_connectedCallback = {OnDownstreamConnected, nullptr};
    downCallbacks.m_traceCallback = {OnTrace, nullptr};
    
    HermesDownstream* pDown = CreateHermesDownstream(1, &downCallbacks);
    
    const char* machineId = "TestMachine";
    HermesDownstreamSettings downSettings = {};
    downSettings.m_machineId = {machineId, static_cast<uint32_t>(strlen(machineId))};
    downSettings.m_port = 50100;
    downSettings.m_checkAlivePeriodInSeconds = 60;
    downSettings.m_reconnectWaitTimeInSeconds = 5;
    downSettings.m_checkAliveResponseMode = eHERMES_CHECK_ALIVE_RESPONSE_MODE_AUTO;
    downSettings.m_checkState = eHERMES_CHECK_STATE_SEND_AND_RECEIVE;
    
    EnableHermesDownstream(pDown, &downSettings);
    
#ifdef _WIN32
    HANDLE downThread = CreateThread(0, 0, RunDownstreamThread, pDown, 0, 0);
#else
    pthread_t downThread;
    pthread_create(&downThread, NULL, RunDownstreamThread, pDown);
#endif
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Create upstream (client)
    std::cout << "[2/5] Creating upstream connection..." << std::endl;
    HermesUpstreamCallbacks upCallbacks = {};
    upCallbacks.m_connectedCallback = {OnUpstreamConnected, nullptr};
    upCallbacks.m_traceCallback = {OnTrace, nullptr};
    
    HermesUpstream* pUp = CreateHermesUpstream(1, &upCallbacks);
    
    const char* localhost = "127.0.0.1";
    HermesUpstreamSettings upSettings = {};
    upSettings.m_machineId = {machineId, static_cast<uint32_t>(strlen(machineId))};
    upSettings.m_hostAddress = {localhost, static_cast<uint32_t>(strlen(localhost))};
    upSettings.m_port = 50100;
    upSettings.m_checkAlivePeriodInSeconds = 60;
    upSettings.m_reconnectWaitTimeInSeconds = 5;
    upSettings.m_checkAliveResponseMode = eHERMES_CHECK_ALIVE_RESPONSE_MODE_AUTO;
    upSettings.m_checkState = eHERMES_CHECK_STATE_SEND_AND_RECEIVE;
    
    EnableHermesUpstream(pUp, &upSettings);
    
#ifdef _WIN32
    HANDLE upThread = CreateThread(0, 0, RunUpstreamThread, pUp, 0, 0);
#else
    pthread_t upThread;
    pthread_create(&upThread, NULL, RunUpstreamThread, pUp);
#endif

    std::cout << "[3/5] Waiting for connections..." << std::endl;
    
    // Wait for both to connect
    downstreamConnected.Wait();
    upstreamConnected.Wait();

    std::cout << "[4/5] Both connections established!" << std::endl;
    std::cout << "[5/5] Cleaning up..." << std::endl;
    
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Cleanup
    StopHermesUpstream(pUp);
    StopHermesDownstream(pDown);
    
#ifdef _WIN32
    WaitForSingleObject(upThread, INFINITE);
    WaitForSingleObject(downThread, INFINITE);
    CloseHandle(upThread);
    CloseHandle(downThread);
#else
    pthread_join(upThread, NULL);
    pthread_join(downThread, NULL);
#endif
    
    DeleteHermesUpstream(pUp);
    DeleteHermesDownstream(pDown);

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  TEST PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "The Hermes library is working correctly." << std::endl;
    std::cout << std::endl;

    return 0;
}