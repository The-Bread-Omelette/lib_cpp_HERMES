// ==============================================================================
// src/include/HermesModern.hpp
// Modern C++ Wrapper for the Hermes Standard Library (Complete Suite)
// ==============================================================================
#pragma once

#include "Hermes.hpp"
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

namespace Hermes {
namespace Modern {

// ==============================================================================
// Downstream Wrapper (Receiver - Horizontal)
// ==============================================================================
class Downstream {
public:
    using ConnectedCallback = std::function<void(const ConnectionInfo&)>;
    using DisconnectedCallback = std::function<void(const Error&)>;
    using StateChangeCallback = std::function<void(EState)>;
    using TraceCallback = std::function<void(ETraceType, StringView)>;
    
    using ServiceDescriptionCallback = std::function<void(const ServiceDescriptionData&)>;
    using MachineReadyCallback = std::function<void(const MachineReadyData&)>;
    using RevokeMachineReadyCallback = std::function<void(const RevokeMachineReadyData&)>;
    using StartTransportCallback = std::function<void(const StartTransportData&)>;
    using StopTransportCallback = std::function<void(const StopTransportData&)>;
    using QueryBoardInfoCallback = std::function<void(const QueryBoardInfoData&)>;
    
    using NotificationCallback = std::function<void(const NotificationData&)>;
    using CheckAliveCallback = std::function<void(const CheckAliveData&)>;
    using CommandCallback = std::function<void(const CommandData&)>;

    Downstream(unsigned laneId) : m_laneId(laneId), m_isRunning(false) {
        m_callbackWrapper = std::make_unique<InternalCallbackWrapper>(this);
        m_downstream = std::make_unique<Hermes::Downstream>(laneId, *m_callbackWrapper);
    }

    ~Downstream() { Stop(); }

    void RegisterConnectedCallback(ConnectedCallback cb) { m_onConnected = cb; }
    void RegisterDisconnectedCallback(DisconnectedCallback cb) { m_onDisconnected = cb; }
    void RegisterStateChangeCallback(StateChangeCallback cb) { m_onStateChange = cb; }
    void RegisterTraceCallback(TraceCallback cb) { m_onTrace = cb; }
    
    void RegisterServiceDescriptionCallback(ServiceDescriptionCallback cb) { m_onServiceDescription = cb; }
    void RegisterMachineReadyCallback(MachineReadyCallback cb) { m_onMachineReady = cb; }
    void RegisterRevokeMachineReadyCallback(RevokeMachineReadyCallback cb) { m_onRevokeMachineReady = cb; }
    void RegisterStartTransportCallback(StartTransportCallback cb) { m_onStartTransport = cb; }
    void RegisterStopTransportCallback(StopTransportCallback cb) { m_onStopTransport = cb; }
    void RegisterQueryBoardInfoCallback(QueryBoardInfoCallback cb) { m_onQueryBoardInfo = cb; }
    void RegisterNotificationCallback(NotificationCallback cb) { m_onNotification = cb; }
    void RegisterCheckAliveCallback(CheckAliveCallback cb) { m_onCheckAlive = cb; }
    void RegisterCommandCallback(CommandCallback cb) { m_onCommand = cb; }

    void Enable(const DownstreamSettings& settings) {
        if (m_isRunning) return;
        m_downstream->Enable(settings);
        m_isRunning = true;
        m_networkThread = std::thread([this]() { m_downstream->Run(); });
    }

    void Stop() {
        if (m_isRunning) {
            m_downstream->Stop();
            if (m_networkThread.joinable()) m_networkThread.join();
            m_isRunning = false;
        }
    }

    template<typename T>
    void Signal(unsigned sessionId, const T& data) { m_downstream->Signal(sessionId, data); }

private:
    class InternalCallbackWrapper : public Hermes::IDownstreamCallback {
        Downstream* m_parent;
    public:
        InternalCallbackWrapper(Downstream* parent) : m_parent(parent) {}

        void OnConnected(unsigned sessionId, Hermes::EState state, const Hermes::ConnectionInfo& info) override {
            if (m_parent->m_onConnected) m_parent->m_onConnected(info);
        }
        void OnDisconnected(unsigned sessionId, Hermes::EState state, const Hermes::Error& error) override {
            if (m_parent->m_onDisconnected) m_parent->m_onDisconnected(error);
        }
        void OnState(unsigned sessionId, Hermes::EState state) override {
            if (m_parent->m_onStateChange) m_parent->m_onStateChange(state);
        }
        void OnTrace(unsigned sessionId, Hermes::ETraceType type, Hermes::StringView trace) override {
            if (m_parent->m_onTrace) m_parent->m_onTrace(type, trace);
        }

        void On(unsigned sessionId, Hermes::EState state, const Hermes::ServiceDescriptionData& data) override {
            if (m_parent->m_onServiceDescription) m_parent->m_onServiceDescription(data);
        }
        void On(unsigned sessionId, Hermes::EState state, const Hermes::MachineReadyData& data) override {
            if (m_parent->m_onMachineReady) m_parent->m_onMachineReady(data);
        }
        void On(unsigned sessionId, Hermes::EState state, const Hermes::RevokeMachineReadyData& data) override {
            if (m_parent->m_onRevokeMachineReady) m_parent->m_onRevokeMachineReady(data);
        }
        void On(unsigned sessionId, Hermes::EState state, const Hermes::StartTransportData& data) override {
            if (m_parent->m_onStartTransport) m_parent->m_onStartTransport(data);
        }
        void On(unsigned sessionId, Hermes::EState state, const Hermes::StopTransportData& data) override {
            if (m_parent->m_onStopTransport) m_parent->m_onStopTransport(data);
        }
        void On(unsigned sessionId, const Hermes::QueryBoardInfoData& data) override {
            if (m_parent->m_onQueryBoardInfo) m_parent->m_onQueryBoardInfo(data);
        }
        void On(unsigned sessionId, const Hermes::NotificationData& data) override {
            if (m_parent->m_onNotification) m_parent->m_onNotification(data);
        }
        void On(unsigned sessionId, const Hermes::CheckAliveData& data) override {
            if (m_parent->m_onCheckAlive) m_parent->m_onCheckAlive(data);
        }
        void On(unsigned sessionId, const Hermes::CommandData& data) override {
            if (m_parent->m_onCommand) m_parent->m_onCommand(data);
        }
    };

    unsigned m_laneId;
    std::atomic<bool> m_isRunning;
    std::thread m_networkThread;
    std::unique_ptr<InternalCallbackWrapper> m_callbackWrapper;
    std::unique_ptr<Hermes::Downstream> m_downstream;

    ConnectedCallback m_onConnected;
    DisconnectedCallback m_onDisconnected;
    StateChangeCallback m_onStateChange;
    TraceCallback m_onTrace;
    
    ServiceDescriptionCallback m_onServiceDescription;
    MachineReadyCallback m_onMachineReady;
    RevokeMachineReadyCallback m_onRevokeMachineReady;
    StartTransportCallback m_onStartTransport;
    StopTransportCallback m_onStopTransport;
    QueryBoardInfoCallback m_onQueryBoardInfo;
    
    NotificationCallback m_onNotification;
    CheckAliveCallback m_onCheckAlive;
    CommandCallback m_onCommand;
};

// ==============================================================================
// Upstream Wrapper (Sender - Horizontal)
// ==============================================================================
class Upstream {
public:
    using ConnectedCallback = std::function<void(const ConnectionInfo&)>;
    using DisconnectedCallback = std::function<void(const Error&)>;
    using StateChangeCallback = std::function<void(EState)>;
    using TraceCallback = std::function<void(ETraceType, StringView)>;

    using ServiceDescriptionCallback = std::function<void(const ServiceDescriptionData&)>;
    using BoardAvailableCallback = std::function<void(const BoardAvailableData&)>;
    using RevokeBoardAvailableCallback = std::function<void(const RevokeBoardAvailableData&)>;
    using TransportFinishedCallback = std::function<void(const TransportFinishedData&)>;
    using BoardForecastCallback = std::function<void(const BoardForecastData&)>;
    using SendBoardInfoCallback = std::function<void(const SendBoardInfoData&)>;
    
    using NotificationCallback = std::function<void(const NotificationData&)>;
    using CheckAliveCallback = std::function<void(const CheckAliveData&)>;
    using CommandCallback = std::function<void(const CommandData&)>;

    Upstream(unsigned laneId) : m_laneId(laneId), m_isRunning(false) {
        m_callbackWrapper = std::make_unique<InternalCallbackWrapper>(this);
        m_upstream = std::make_unique<Hermes::Upstream>(laneId, *m_callbackWrapper);
    }

    ~Upstream() { Stop(); }

    void RegisterConnectedCallback(ConnectedCallback cb) { m_onConnected = cb; }
    void RegisterDisconnectedCallback(DisconnectedCallback cb) { m_onDisconnected = cb; }
    void RegisterStateChangeCallback(StateChangeCallback cb) { m_onStateChange = cb; }
    void RegisterTraceCallback(TraceCallback cb) { m_onTrace = cb; }

    void RegisterServiceDescriptionCallback(ServiceDescriptionCallback cb) { m_onServiceDescription = cb; }
    void RegisterBoardAvailableCallback(BoardAvailableCallback cb) { m_onBoardAvailable = cb; }
    void RegisterRevokeBoardAvailableCallback(RevokeBoardAvailableCallback cb) { m_onRevokeBoardAvailable = cb; }
    void RegisterTransportFinishedCallback(TransportFinishedCallback cb) { m_onTransportFinished = cb; }
    void RegisterBoardForecastCallback(BoardForecastCallback cb) { m_onBoardForecast = cb; }
    void RegisterSendBoardInfoCallback(SendBoardInfoCallback cb) { m_onSendBoardInfo = cb; }
    void RegisterNotificationCallback(NotificationCallback cb) { m_onNotification = cb; }
    void RegisterCheckAliveCallback(CheckAliveCallback cb) { m_onCheckAlive = cb; }
    void RegisterCommandCallback(CommandCallback cb) { m_onCommand = cb; }

    void Enable(const UpstreamSettings& settings) {
        if (m_isRunning) return;
        m_upstream->Enable(settings);
        m_isRunning = true;
        m_networkThread = std::thread([this]() { m_upstream->Run(); });
    }

    void Stop() {
        if (m_isRunning) {
            m_upstream->Stop();
            if (m_networkThread.joinable()) m_networkThread.join();
            m_isRunning = false;
        }
    }

    template<typename T>
    void Signal(unsigned sessionId, const T& data) { m_upstream->Signal(sessionId, data); }

private:
    class InternalCallbackWrapper : public Hermes::IUpstreamCallback {
        Upstream* m_parent;
    public:
        InternalCallbackWrapper(Upstream* parent) : m_parent(parent) {}

        void OnConnected(unsigned sessionId, Hermes::EState state, const Hermes::ConnectionInfo& info) override {
            if (m_parent->m_onConnected) m_parent->m_onConnected(info);
        }
        void OnDisconnected(unsigned sessionId, Hermes::EState state, const Hermes::Error& error) override {
            if (m_parent->m_onDisconnected) m_parent->m_onDisconnected(error);
        }
        void OnState(unsigned sessionId, Hermes::EState state) override {
            if (m_parent->m_onStateChange) m_parent->m_onStateChange(state);
        }
        void OnTrace(unsigned sessionId, Hermes::ETraceType type, Hermes::StringView trace) override {
            if (m_parent->m_onTrace) m_parent->m_onTrace(type, trace);
        }

        void On(unsigned sessionId, Hermes::EState state, const Hermes::ServiceDescriptionData& data) override {
            if (m_parent->m_onServiceDescription) m_parent->m_onServiceDescription(data);
        }
        void On(unsigned sessionId, Hermes::EState state, const Hermes::BoardAvailableData& data) override {
            if (m_parent->m_onBoardAvailable) m_parent->m_onBoardAvailable(data);
        }
        void On(unsigned sessionId, Hermes::EState state, const Hermes::RevokeBoardAvailableData& data) override {
            if (m_parent->m_onRevokeBoardAvailable) m_parent->m_onRevokeBoardAvailable(data);
        }
        void On(unsigned sessionId, Hermes::EState state, const Hermes::TransportFinishedData& data) override {
            if (m_parent->m_onTransportFinished) m_parent->m_onTransportFinished(data);
        }
        void On(unsigned sessionId, Hermes::EState state, const Hermes::BoardForecastData& data) override {
            if (m_parent->m_onBoardForecast) m_parent->m_onBoardForecast(data);
        }
        void On(unsigned sessionId, const Hermes::SendBoardInfoData& data) override {
            if (m_parent->m_onSendBoardInfo) m_parent->m_onSendBoardInfo(data);
        }
        void On(unsigned sessionId, const Hermes::NotificationData& data) override {
            if (m_parent->m_onNotification) m_parent->m_onNotification(data);
        }
        void On(unsigned sessionId, const Hermes::CheckAliveData& data) override {
            if (m_parent->m_onCheckAlive) m_parent->m_onCheckAlive(data);
        }
        void On(unsigned sessionId, const Hermes::CommandData& data) override {
            if (m_parent->m_onCommand) m_parent->m_onCommand(data);
        }
    };

    unsigned m_laneId;
    std::atomic<bool> m_isRunning;
    std::thread m_networkThread;
    std::unique_ptr<InternalCallbackWrapper> m_callbackWrapper;
    std::unique_ptr<Hermes::Upstream> m_upstream;

    ConnectedCallback m_onConnected;
    DisconnectedCallback m_onDisconnected;
    StateChangeCallback m_onStateChange;
    TraceCallback m_onTrace;
    
    ServiceDescriptionCallback m_onServiceDescription;
    BoardAvailableCallback m_onBoardAvailable;
    RevokeBoardAvailableCallback m_onRevokeBoardAvailable;
    TransportFinishedCallback m_onTransportFinished;
    BoardForecastCallback m_onBoardForecast;
    SendBoardInfoCallback m_onSendBoardInfo;
    
    NotificationCallback m_onNotification;
    CheckAliveCallback m_onCheckAlive;
    CommandCallback m_onCommand;
};


// ==============================================================================
// VerticalService Wrapper (Machine Server - Vertical)
// ==============================================================================
class VerticalService {
public:
    using ConnectedCallback = std::function<void(const ConnectionInfo&, EVerticalState)>;
    using DisconnectedCallback = std::function<void(const Error&, EVerticalState)>;
    using TraceCallback = std::function<void(ETraceType, StringView)>;

    using SupervisoryServiceDescriptionCallback = std::function<void(const SupervisoryServiceDescriptionData&, EVerticalState)>;
    using GetConfigurationCallback = std::function<void(const GetConfigurationData&, const ConnectionInfo&)>;
    using SetConfigurationCallback = std::function<void(const SetConfigurationData&, const ConnectionInfo&)>;
    using SendWorkOrderInfoCallback = std::function<void(const SendWorkOrderInfoData&)>;
    using QueryHermesCapabilitiesCallback = std::function<void(const QueryHermesCapabilitiesData&)>;
    
    using NotificationCallback = std::function<void(const NotificationData&)>;
    using CheckAliveCallback = std::function<void(const CheckAliveData&)>;

    VerticalService() : m_isRunning(false) {
        m_callbackWrapper = std::make_unique<InternalCallbackWrapper>(this);
        m_verticalService = std::make_unique<Hermes::VerticalService>(*m_callbackWrapper);
    }

    ~VerticalService() { Stop(); }

    void RegisterConnectedCallback(ConnectedCallback cb) { m_onConnected = cb; }
    void RegisterDisconnectedCallback(DisconnectedCallback cb) { m_onDisconnected = cb; }
    void RegisterTraceCallback(TraceCallback cb) { m_onTrace = cb; }

    void RegisterSupervisoryServiceDescriptionCallback(SupervisoryServiceDescriptionCallback cb) { m_onSupervisoryServiceDescription = cb; }
    void RegisterGetConfigurationCallback(GetConfigurationCallback cb) { m_onGetConfiguration = cb; }
    void RegisterSetConfigurationCallback(SetConfigurationCallback cb) { m_onSetConfiguration = cb; }
    void RegisterSendWorkOrderInfoCallback(SendWorkOrderInfoCallback cb) { m_onSendWorkOrderInfo = cb; }
    void RegisterQueryHermesCapabilitiesCallback(QueryHermesCapabilitiesCallback cb) { m_onQueryHermesCapabilities = cb; }
    void RegisterNotificationCallback(NotificationCallback cb) { m_onNotification = cb; }
    void RegisterCheckAliveCallback(CheckAliveCallback cb) { m_onCheckAlive = cb; }

    void Enable(const VerticalServiceSettings& settings) {
        if (m_isRunning) return;
        m_verticalService->Enable(settings);
        m_isRunning = true;
        m_networkThread = std::thread([this]() { m_verticalService->Run(); });
    }

    void Stop() {
        if (m_isRunning) {
            m_verticalService->Stop();
            if (m_networkThread.joinable()) m_networkThread.join();
            m_isRunning = false;
        }
    }

    template<typename T>
    void Signal(unsigned sessionId, const T& data) { m_verticalService->Signal(sessionId, data); }
    
    template<typename T>
    void SignalBroadcast(const T& data) { m_verticalService->Signal(data); }

private:
    class InternalCallbackWrapper : public Hermes::IVerticalServiceCallback {
        VerticalService* m_parent;
    public:
        InternalCallbackWrapper(VerticalService* parent) : m_parent(parent) {}

        void OnConnected(unsigned sessionId, Hermes::EVerticalState state, const Hermes::ConnectionInfo& info) override {
            if (m_parent->m_onConnected) m_parent->m_onConnected(info, state);
        }
        void OnDisconnected(unsigned sessionId, Hermes::EVerticalState state, const Hermes::Error& error) override {
            if (m_parent->m_onDisconnected) m_parent->m_onDisconnected(error, state);
        }
        void OnTrace(unsigned sessionId, Hermes::ETraceType type, Hermes::StringView trace) override {
            if (m_parent->m_onTrace) m_parent->m_onTrace(type, trace);
        }

        void On(unsigned sessionId, Hermes::EVerticalState state, const Hermes::SupervisoryServiceDescriptionData& data) override {
            if (m_parent->m_onSupervisoryServiceDescription) m_parent->m_onSupervisoryServiceDescription(data, state);
        }
        void On(unsigned sessionId, const Hermes::GetConfigurationData& data, const Hermes::ConnectionInfo& info) override {
            if (m_parent->m_onGetConfiguration) m_parent->m_onGetConfiguration(data, info);
        }
        void On(unsigned sessionId, const Hermes::SetConfigurationData& data, const Hermes::ConnectionInfo& info) override {
            if (m_parent->m_onSetConfiguration) m_parent->m_onSetConfiguration(data, info);
        }
        void On(unsigned sessionId, const Hermes::SendWorkOrderInfoData& data) override {
            if (m_parent->m_onSendWorkOrderInfo) m_parent->m_onSendWorkOrderInfo(data);
        }
        void On(unsigned sessionId, const Hermes::QueryHermesCapabilitiesData& data) override {
            if (m_parent->m_onQueryHermesCapabilities) m_parent->m_onQueryHermesCapabilities(data);
        }
        void On(unsigned sessionId, const Hermes::NotificationData& data) override {
            if (m_parent->m_onNotification) m_parent->m_onNotification(data);
        }
        void On(unsigned sessionId, const Hermes::CheckAliveData& data) override {
            if (m_parent->m_onCheckAlive) m_parent->m_onCheckAlive(data);
        }
    };

    std::atomic<bool> m_isRunning;
    std::thread m_networkThread;
    std::unique_ptr<InternalCallbackWrapper> m_callbackWrapper;
    std::unique_ptr<Hermes::VerticalService> m_verticalService;

    ConnectedCallback m_onConnected;
    DisconnectedCallback m_onDisconnected;
    TraceCallback m_onTrace;

    SupervisoryServiceDescriptionCallback m_onSupervisoryServiceDescription;
    GetConfigurationCallback m_onGetConfiguration;
    SetConfigurationCallback m_onSetConfiguration;
    SendWorkOrderInfoCallback m_onSendWorkOrderInfo;
    QueryHermesCapabilitiesCallback m_onQueryHermesCapabilities;
    NotificationCallback m_onNotification;
    CheckAliveCallback m_onCheckAlive;
};


// ==============================================================================
// VerticalClient Wrapper (MES/Factory Cloud Client - Vertical)
// ==============================================================================
class VerticalClient {
public:
    using ConnectedCallback = std::function<void(const ConnectionInfo&, EVerticalState)>;
    using DisconnectedCallback = std::function<void(const Error&, EVerticalState)>;
    using TraceCallback = std::function<void(ETraceType, StringView)>;

    using SupervisoryServiceDescriptionCallback = std::function<void(const SupervisoryServiceDescriptionData&, EVerticalState)>;
    using BoardArrivedCallback = std::function<void(const BoardArrivedData&)>;
    using BoardDepartedCallback = std::function<void(const BoardDepartedData&)>;
    using QueryWorkOrderInfoCallback = std::function<void(const QueryWorkOrderInfoData&)>;
    using ReplyWorkOrderInfoCallback = std::function<void(const ReplyWorkOrderInfoData&)>;
    using CurrentConfigurationCallback = std::function<void(const CurrentConfigurationData&)>;
    using SendHermesCapabilitiesCallback = std::function<void(const SendHermesCapabilitiesData&)>;
    
    using NotificationCallback = std::function<void(const NotificationData&)>;
    using CheckAliveCallback = std::function<void(const CheckAliveData&)>;

    VerticalClient() : m_isRunning(false) {
        m_callbackWrapper = std::make_unique<InternalCallbackWrapper>(this);
        m_verticalClient = std::make_unique<Hermes::VerticalClient>(*m_callbackWrapper);
    }

    ~VerticalClient() { Stop(); }

    void RegisterConnectedCallback(ConnectedCallback cb) { m_onConnected = cb; }
    void RegisterDisconnectedCallback(DisconnectedCallback cb) { m_onDisconnected = cb; }
    void RegisterTraceCallback(TraceCallback cb) { m_onTrace = cb; }

    void RegisterSupervisoryServiceDescriptionCallback(SupervisoryServiceDescriptionCallback cb) { m_onSupervisoryServiceDescription = cb; }
    void RegisterBoardArrivedCallback(BoardArrivedCallback cb) { m_onBoardArrived = cb; }
    void RegisterBoardDepartedCallback(BoardDepartedCallback cb) { m_onBoardDeparted = cb; }
    void RegisterQueryWorkOrderInfoCallback(QueryWorkOrderInfoCallback cb) { m_onQueryWorkOrderInfo = cb; }
    void RegisterReplyWorkOrderInfoCallback(ReplyWorkOrderInfoCallback cb) { m_onReplyWorkOrderInfo = cb; }
    void RegisterCurrentConfigurationCallback(CurrentConfigurationCallback cb) { m_onCurrentConfiguration = cb; }
    void RegisterSendHermesCapabilitiesCallback(SendHermesCapabilitiesCallback cb) { m_onSendHermesCapabilities = cb; }
    void RegisterNotificationCallback(NotificationCallback cb) { m_onNotification = cb; }
    void RegisterCheckAliveCallback(CheckAliveCallback cb) { m_onCheckAlive = cb; }

    void Enable(const VerticalClientSettings& settings) {
        if (m_isRunning) return;
        m_verticalClient->Enable(settings);
        m_isRunning = true;
        m_networkThread = std::thread([this]() { m_verticalClient->Run(); });
    }

    void Stop() {
        if (m_isRunning) {
            m_verticalClient->Stop();
            if (m_networkThread.joinable()) m_networkThread.join();
            m_isRunning = false;
        }
    }

    template<typename T>
    void Signal(unsigned sessionId, const T& data) { m_verticalClient->Signal(sessionId, data); }

private:
    class InternalCallbackWrapper : public Hermes::IVerticalClientCallback {
        VerticalClient* m_parent;
    public:
        InternalCallbackWrapper(VerticalClient* parent) : m_parent(parent) {}

        void OnConnected(unsigned sessionId, Hermes::EVerticalState state, const Hermes::ConnectionInfo& info) override {
            if (m_parent->m_onConnected) m_parent->m_onConnected(info, state);
        }
        void OnDisconnected(unsigned sessionId, Hermes::EVerticalState state, const Hermes::Error& error) override {
            if (m_parent->m_onDisconnected) m_parent->m_onDisconnected(error, state);
        }
        void OnTrace(unsigned sessionId, Hermes::ETraceType type, Hermes::StringView trace) override {
            if (m_parent->m_onTrace) m_parent->m_onTrace(type, trace);
        }

        void On(unsigned sessionId, Hermes::EVerticalState state, const Hermes::SupervisoryServiceDescriptionData& data) override {
            if (m_parent->m_onSupervisoryServiceDescription) m_parent->m_onSupervisoryServiceDescription(data, state);
        }
        void On(unsigned sessionId, const Hermes::BoardArrivedData& data) override {
            if (m_parent->m_onBoardArrived) m_parent->m_onBoardArrived(data);
        }
        void On(unsigned sessionId, const Hermes::BoardDepartedData& data) override {
            if (m_parent->m_onBoardDeparted) m_parent->m_onBoardDeparted(data);
        }
        void On(unsigned sessionId, const Hermes::QueryWorkOrderInfoData& data) override {
            if (m_parent->m_onQueryWorkOrderInfo) m_parent->m_onQueryWorkOrderInfo(data);
        }
        void On(unsigned sessionId, const Hermes::ReplyWorkOrderInfoData& data) override {
            if (m_parent->m_onReplyWorkOrderInfo) m_parent->m_onReplyWorkOrderInfo(data);
        }
        void On(unsigned sessionId, const Hermes::CurrentConfigurationData& data) override {
            if (m_parent->m_onCurrentConfiguration) m_parent->m_onCurrentConfiguration(data);
        }
        void On(unsigned sessionId, const Hermes::SendHermesCapabilitiesData& data) override {
            if (m_parent->m_onSendHermesCapabilities) m_parent->m_onSendHermesCapabilities(data);
        }
        void On(unsigned sessionId, const Hermes::NotificationData& data) override {
            if (m_parent->m_onNotification) m_parent->m_onNotification(data);
        }
        void On(unsigned sessionId, const Hermes::CheckAliveData& data) override {
            if (m_parent->m_onCheckAlive) m_parent->m_onCheckAlive(data);
        }
    };

    std::atomic<bool> m_isRunning;
    std::thread m_networkThread;
    std::unique_ptr<InternalCallbackWrapper> m_callbackWrapper;
    std::unique_ptr<Hermes::VerticalClient> m_verticalClient;

    ConnectedCallback m_onConnected;
    DisconnectedCallback m_onDisconnected;
    TraceCallback m_onTrace;

    SupervisoryServiceDescriptionCallback m_onSupervisoryServiceDescription;
    BoardArrivedCallback m_onBoardArrived;
    BoardDepartedCallback m_onBoardDeparted;
    QueryWorkOrderInfoCallback m_onQueryWorkOrderInfo;
    ReplyWorkOrderInfoCallback m_onReplyWorkOrderInfo;
    CurrentConfigurationCallback m_onCurrentConfiguration;
    SendHermesCapabilitiesCallback m_onSendHermesCapabilities;
    NotificationCallback m_onNotification;
    CheckAliveCallback m_onCheckAlive;
};

} // namespace Modern
} // namespace Hermes