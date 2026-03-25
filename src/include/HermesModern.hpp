// =============================================================================
// src/include/HermesModern.hpp
// Modern C++ wrapper for the Hermes Standard library.
//
// Wraps Hermes::Downstream and Hermes::Upstream behind std::function callbacks
// so callers register lambdas instead of implementing virtual interfaces.
//
// FIXES applied vs original:
//
//  Modern::Downstream::InternalCallbackWrapper
//    - Inherits Hermes::IDownstreamCallback (correct, was correct)
//    - WRONG overrides removed:
//        On(EState, BoardAvailableData)       -> belongs to IUpstreamCallback
//        On(EState, RevokeBoardAvailableData) -> belongs to IUpstreamCallback
//        On(EState, TransportFinishedData)    -> belongs to IUpstreamCallback
//        On(EState, BoardForecastData)        -> belongs to IUpstreamCallback
//        On(EState, SendBoardInfoData)        -> belongs to IUpstreamCallback
//    - MISSING overrides added (were pure virtual, caused link failure):
//        On(EState, MachineReadyData)         = 0 in IDownstreamCallback
//        On(EState, RevokeMachineReadyData)   = 0 in IDownstreamCallback
//        On(EState, StartTransportData)       = 0 in IDownstreamCallback
//        On(EState, StopTransportData)        = 0 in IDownstreamCallback
//    - WRONG callback types removed from public API:
//        BoardAvailableCallback, RevokeBoardAvailableCallback,
//        TransportFinishedCallback, BoardForecastCallback, SendBoardInfoCallback
//    - CORRECT callback types added to public API:
//        MachineReadyCallback, RevokeMachineReadyCallback,
//        StartTransportCallback, StopTransportCallback, QueryBoardInfoCallback
//
//  Modern::Upstream::InternalCallbackWrapper
//    - Inherits Hermes::IUpstreamCallback (correct, was correct)
//    - WRONG overrides removed:
//        On(EState, MachineReadyData)         -> belongs to IDownstreamCallback
//        On(EState, RevokeMachineReadyData)   -> belongs to IDownstreamCallback
//        On(EState, StartTransportData)       -> belongs to IDownstreamCallback
//        On(EState, StopTransportData)        -> belongs to IDownstreamCallback
//        On(EState, QueryBoardInfoData)       -> belongs to IDownstreamCallback
//    - MISSING overrides added (were pure virtual):
//        On(EState, BoardAvailableData)       = 0 in IUpstreamCallback
//        On(EState, RevokeBoardAvailableData) = 0 in IUpstreamCallback
//        On(EState, TransportFinishedData)    = 0 in IUpstreamCallback
//    - WRONG callback types removed from public API:
//        MachineReadyCallback, RevokeMachineReadyCallback,
//        StartTransportCallback, StopTransportCallback, QueryBoardInfoCallback
//    - CORRECT callback types added to public API:
//        BoardAvailableCallback, RevokeBoardAvailableCallback,
//        TransportFinishedCallback, BoardForecastCallback, SendBoardInfoCallback
//
//  Both classes:
//    - Stop() is now safe to call multiple times (guarded by exchange)
//    - Enable() cannot be called twice without Stop() in between
//    - m_laneId stored but unused warning removed (used in construction)
// =============================================================================

#pragma once

#include "Hermes.hpp"
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

namespace Hermes {
namespace Modern {

// =============================================================================
// Modern::Downstream
//
// Listens on a TCP port for an incoming Upstream machine connection.
// Receives messages that the Upstream machine sends:
//   ServiceDescription, MachineReady, RevokeMachineReady,
//   StartTransport, StopTransport, QueryBoardInfo
//
// Sends messages that the Downstream machine produces:
//   ServiceDescription, BoardAvailable, RevokeBoardAvailable,
//   TransportFinished, BoardForecast, SendBoardInfo,
//   Notification, CheckAlive, Command
// =============================================================================
class Downstream
{
public:
    // --- Connection lifecycle callbacks ---
    using ConnectedCallback    = std::function<void(unsigned sessionId, const ConnectionInfo&)>;
    using DisconnectedCallback = std::function<void(unsigned sessionId, const Error&)>;
    using StateChangeCallback  = std::function<void(unsigned sessionId, EState)>;
    using TraceCallback        = std::function<void(unsigned sessionId, ETraceType, StringView)>;

    // --- Messages RECEIVED from the Upstream machine ---
    using ServiceDescriptionCallback  = std::function<void(unsigned sessionId, EState, const ServiceDescriptionData&)>;
    using MachineReadyCallback        = std::function<void(unsigned sessionId, EState, const MachineReadyData&)>;
    using RevokeMachineReadyCallback  = std::function<void(unsigned sessionId, EState, const RevokeMachineReadyData&)>;
    using StartTransportCallback      = std::function<void(unsigned sessionId, EState, const StartTransportData&)>;
    using StopTransportCallback       = std::function<void(unsigned sessionId, EState, const StopTransportData&)>;
    using QueryBoardInfoCallback      = std::function<void(unsigned sessionId, const QueryBoardInfoData&)>;

    // --- Auxiliary messages (either direction) ---
    using NotificationCallback = std::function<void(unsigned sessionId, const NotificationData&)>;
    using CheckAliveCallback   = std::function<void(unsigned sessionId, const CheckAliveData&)>;
    using CommandCallback      = std::function<void(unsigned sessionId, const CommandData&)>;

    explicit Downstream(unsigned laneId)
        : m_isRunning(false)
    {
        m_callbackWrapper = std::make_unique<InternalCallbackWrapper>(this);
        m_downstream      = std::make_unique<Hermes::Downstream>(laneId, *m_callbackWrapper);
    }

    ~Downstream() { Stop(); }

    Downstream(const Downstream&)            = delete;
    Downstream& operator=(const Downstream&) = delete;

    // --- Register callbacks (call before Enable) ---
    void RegisterConnectedCallback(ConnectedCallback cb)                 { m_onConnected = std::move(cb); }
    void RegisterDisconnectedCallback(DisconnectedCallback cb)           { m_onDisconnected = std::move(cb); }
    void RegisterStateChangeCallback(StateChangeCallback cb)             { m_onStateChange = std::move(cb); }
    void RegisterTraceCallback(TraceCallback cb)                         { m_onTrace = std::move(cb); }
    void RegisterServiceDescriptionCallback(ServiceDescriptionCallback cb){ m_onServiceDescription = std::move(cb); }
    void RegisterMachineReadyCallback(MachineReadyCallback cb)           { m_onMachineReady = std::move(cb); }
    void RegisterRevokeMachineReadyCallback(RevokeMachineReadyCallback cb){ m_onRevokeMachineReady = std::move(cb); }
    void RegisterStartTransportCallback(StartTransportCallback cb)       { m_onStartTransport = std::move(cb); }
    void RegisterStopTransportCallback(StopTransportCallback cb)         { m_onStopTransport = std::move(cb); }
    void RegisterQueryBoardInfoCallback(QueryBoardInfoCallback cb)       { m_onQueryBoardInfo = std::move(cb); }
    void RegisterNotificationCallback(NotificationCallback cb)           { m_onNotification = std::move(cb); }
    void RegisterCheckAliveCallback(CheckAliveCallback cb)               { m_onCheckAlive = std::move(cb); }
    void RegisterCommandCallback(CommandCallback cb)                     { m_onCommand = std::move(cb); }

    // --- Lifecycle ---

    // Enable starts listening and launches the network thread.
    // Call this after registering callbacks.
    void Enable(const DownstreamSettings& settings)
    {
        if (m_isRunning.exchange(true))
            return; // already running

        m_downstream->Enable(settings);
        m_networkThread = std::thread([this]() { m_downstream->Run(); });
    }

    // Stop shuts down the connection and joins the network thread.
    // Safe to call multiple times.
    void Stop()
    {
        if (!m_isRunning.exchange(false))
            return; // already stopped

        m_downstream->Stop();
        if (m_networkThread.joinable())
            m_networkThread.join();
    }

    // --- Send messages TO the Upstream machine ---
    // Must be called from within a Post() lambda or a callback to be thread-safe.
    template<typename T>
    void Signal(unsigned sessionId, const T& data) { m_downstream->Signal(sessionId, data); }

    // Post a callable onto the Hermes network thread (thread-safe).
    template<typename F>
    void Post(F&& f) { m_downstream->Post(std::forward<F>(f)); }

private:
    // -------------------------------------------------------------------------
    // InternalCallbackWrapper
    // Implements Hermes::IDownstreamCallback and forwards to std::function members.
    // Pure virtuals from IDownstreamCallback that MUST be overridden:
    //   OnConnected, OnDisconnected, OnState, OnTrace
    //   On(EState, ServiceDescriptionData)
    //   On(EState, MachineReadyData)
    //   On(EState, RevokeMachineReadyData)
    //   On(EState, StartTransportData)
    //   On(EState, StopTransportData)
    //   On(NotificationData)
    //   On(CommandData)
    // -------------------------------------------------------------------------
    struct InternalCallbackWrapper : Hermes::IDownstreamCallback
    {
        explicit InternalCallbackWrapper(Downstream* parent) : m_parent(parent) {}

        void OnConnected(unsigned sessionId, EState state, const ConnectionInfo& info) override
        {
            if (m_parent->m_onConnected) m_parent->m_onConnected(sessionId, info);
        }

        void OnDisconnected(unsigned sessionId, EState, const Error& error) override
        {
            if (m_parent->m_onDisconnected) m_parent->m_onDisconnected(sessionId, error);
        }

        void OnState(unsigned sessionId, EState state) override
        {
            if (m_parent->m_onStateChange) m_parent->m_onStateChange(sessionId, state);
        }

        void OnTrace(unsigned sessionId, ETraceType type, StringView trace) override
        {
            if (m_parent->m_onTrace) m_parent->m_onTrace(sessionId, type, trace);
        }

        // Messages received FROM the Upstream machine:
        void On(unsigned sessionId, EState state, const ServiceDescriptionData& data) override
        {
            if (m_parent->m_onServiceDescription) m_parent->m_onServiceDescription(sessionId, state, data);
        }

        void On(unsigned sessionId, EState state, const MachineReadyData& data) override
        {
            if (m_parent->m_onMachineReady) m_parent->m_onMachineReady(sessionId, state, data);
        }

        void On(unsigned sessionId, EState state, const RevokeMachineReadyData& data) override
        {
            if (m_parent->m_onRevokeMachineReady) m_parent->m_onRevokeMachineReady(sessionId, state, data);
        }

        void On(unsigned sessionId, EState state, const StartTransportData& data) override
        {
            if (m_parent->m_onStartTransport) m_parent->m_onStartTransport(sessionId, state, data);
        }

        void On(unsigned sessionId, EState state, const StopTransportData& data) override
        {
            if (m_parent->m_onStopTransport) m_parent->m_onStopTransport(sessionId, state, data);
        }

        void On(unsigned sessionId, const QueryBoardInfoData& data) override
        {
            if (m_parent->m_onQueryBoardInfo) m_parent->m_onQueryBoardInfo(sessionId, data);
        }

        // Auxiliary messages:
        void On(unsigned sessionId, const NotificationData& data) override
        {
            if (m_parent->m_onNotification) m_parent->m_onNotification(sessionId, data);
        }

        void On(unsigned sessionId, const CheckAliveData& data) override
        {
            if (m_parent->m_onCheckAlive) m_parent->m_onCheckAlive(sessionId, data);
        }

        void On(unsigned sessionId, const CommandData& data) override
        {
            if (m_parent->m_onCommand) m_parent->m_onCommand(sessionId, data);
        }

        Downstream* m_parent;
    };

    std::atomic<bool>                        m_isRunning;
    std::thread                              m_networkThread;
    std::unique_ptr<InternalCallbackWrapper> m_callbackWrapper;
    std::unique_ptr<Hermes::Downstream>      m_downstream;

    ConnectedCallback           m_onConnected;
    DisconnectedCallback        m_onDisconnected;
    StateChangeCallback         m_onStateChange;
    TraceCallback               m_onTrace;
    ServiceDescriptionCallback  m_onServiceDescription;
    MachineReadyCallback        m_onMachineReady;
    RevokeMachineReadyCallback  m_onRevokeMachineReady;
    StartTransportCallback      m_onStartTransport;
    StopTransportCallback       m_onStopTransport;
    QueryBoardInfoCallback      m_onQueryBoardInfo;
    NotificationCallback        m_onNotification;
    CheckAliveCallback          m_onCheckAlive;
    CommandCallback             m_onCommand;
};

// =============================================================================
// Modern::Upstream
//
// Connects to a Downstream machine's TCP port.
// Receives messages that the Downstream machine sends:
//   ServiceDescription, BoardAvailable, RevokeBoardAvailable,
//   TransportFinished, BoardForecast, SendBoardInfo
//
// Sends messages that the Upstream machine produces:
//   ServiceDescription, MachineReady, RevokeMachineReady,
//   StartTransport, StopTransport, QueryBoardInfo,
//   Notification, CheckAlive, Command
// =============================================================================
class Upstream
{
public:
    // --- Connection lifecycle callbacks ---
    using ConnectedCallback    = std::function<void(unsigned sessionId, const ConnectionInfo&)>;
    using DisconnectedCallback = std::function<void(unsigned sessionId, const Error&)>;
    using StateChangeCallback  = std::function<void(unsigned sessionId, EState)>;
    using TraceCallback        = std::function<void(unsigned sessionId, ETraceType, StringView)>;

    // --- Messages RECEIVED from the Downstream machine ---
    using ServiceDescriptionCallback    = std::function<void(unsigned sessionId, EState, const ServiceDescriptionData&)>;
    using BoardAvailableCallback        = std::function<void(unsigned sessionId, EState, const BoardAvailableData&)>;
    using RevokeBoardAvailableCallback  = std::function<void(unsigned sessionId, EState, const RevokeBoardAvailableData&)>;
    using TransportFinishedCallback     = std::function<void(unsigned sessionId, EState, const TransportFinishedData&)>;
    using BoardForecastCallback         = std::function<void(unsigned sessionId, EState, const BoardForecastData&)>;
    using SendBoardInfoCallback         = std::function<void(unsigned sessionId, const SendBoardInfoData&)>;

    // --- Auxiliary messages (either direction) ---
    using NotificationCallback = std::function<void(unsigned sessionId, const NotificationData&)>;
    using CheckAliveCallback   = std::function<void(unsigned sessionId, const CheckAliveData&)>;
    using CommandCallback      = std::function<void(unsigned sessionId, const CommandData&)>;

    explicit Upstream(unsigned laneId)
        : m_isRunning(false)
    {
        m_callbackWrapper = std::make_unique<InternalCallbackWrapper>(this);
        m_upstream        = std::make_unique<Hermes::Upstream>(laneId, *m_callbackWrapper);
    }

    ~Upstream() { Stop(); }

    Upstream(const Upstream&)            = delete;
    Upstream& operator=(const Upstream&) = delete;

    // --- Register callbacks (call before Enable) ---
    void RegisterConnectedCallback(ConnectedCallback cb)                      { m_onConnected = std::move(cb); }
    void RegisterDisconnectedCallback(DisconnectedCallback cb)                { m_onDisconnected = std::move(cb); }
    void RegisterStateChangeCallback(StateChangeCallback cb)                  { m_onStateChange = std::move(cb); }
    void RegisterTraceCallback(TraceCallback cb)                              { m_onTrace = std::move(cb); }
    void RegisterServiceDescriptionCallback(ServiceDescriptionCallback cb)    { m_onServiceDescription = std::move(cb); }
    void RegisterBoardAvailableCallback(BoardAvailableCallback cb)            { m_onBoardAvailable = std::move(cb); }
    void RegisterRevokeBoardAvailableCallback(RevokeBoardAvailableCallback cb){ m_onRevokeBoardAvailable = std::move(cb); }
    void RegisterTransportFinishedCallback(TransportFinishedCallback cb)      { m_onTransportFinished = std::move(cb); }
    void RegisterBoardForecastCallback(BoardForecastCallback cb)              { m_onBoardForecast = std::move(cb); }
    void RegisterSendBoardInfoCallback(SendBoardInfoCallback cb)              { m_onSendBoardInfo = std::move(cb); }
    void RegisterNotificationCallback(NotificationCallback cb)                { m_onNotification = std::move(cb); }
    void RegisterCheckAliveCallback(CheckAliveCallback cb)                    { m_onCheckAlive = std::move(cb); }
    void RegisterCommandCallback(CommandCallback cb)                          { m_onCommand = std::move(cb); }

    // --- Lifecycle ---

    // Enable connects to the downstream host and launches the network thread.
    void Enable(const UpstreamSettings& settings)
    {
        if (m_isRunning.exchange(true))
            return;

        m_upstream->Enable(settings);
        m_networkThread = std::thread([this]() { m_upstream->Run(); });
    }

    // Stop disconnects and joins the network thread. Safe to call multiple times.
    void Stop()
    {
        if (!m_isRunning.exchange(false))
            return;

        m_upstream->Stop();
        if (m_networkThread.joinable())
            m_networkThread.join();
    }

    // --- Send messages TO the Downstream machine ---
    template<typename T>
    void Signal(unsigned sessionId, const T& data) { m_upstream->Signal(sessionId, data); }

    // Post a callable onto the Hermes network thread (thread-safe).
    template<typename F>
    void Post(F&& f) { m_upstream->Post(std::forward<F>(f)); }

private:
    // -------------------------------------------------------------------------
    // InternalCallbackWrapper
    // Implements Hermes::IUpstreamCallback and forwards to std::function members.
    // Pure virtuals from IUpstreamCallback that MUST be overridden:
    //   OnConnected, OnDisconnected, OnState, OnTrace
    //   On(EState, ServiceDescriptionData)
    //   On(EState, BoardAvailableData)
    //   On(EState, RevokeBoardAvailableData)
    //   On(EState, TransportFinishedData)
    //   On(NotificationData)
    //   On(CommandData)
    // -------------------------------------------------------------------------
    struct InternalCallbackWrapper : Hermes::IUpstreamCallback
    {
        explicit InternalCallbackWrapper(Upstream* parent) : m_parent(parent) {}

        void OnConnected(unsigned sessionId, EState state, const ConnectionInfo& info) override
        {
            if (m_parent->m_onConnected) m_parent->m_onConnected(sessionId, info);
        }

        void OnDisconnected(unsigned sessionId, EState, const Error& error) override
        {
            if (m_parent->m_onDisconnected) m_parent->m_onDisconnected(sessionId, error);
        }

        void OnState(unsigned sessionId, EState state) override
        {
            if (m_parent->m_onStateChange) m_parent->m_onStateChange(sessionId, state);
        }

        void OnTrace(unsigned sessionId, ETraceType type, StringView trace) override
        {
            if (m_parent->m_onTrace) m_parent->m_onTrace(sessionId, type, trace);
        }

        // Messages received FROM the Downstream machine:
        void On(unsigned sessionId, EState state, const ServiceDescriptionData& data) override
        {
            if (m_parent->m_onServiceDescription) m_parent->m_onServiceDescription(sessionId, state, data);
        }

        void On(unsigned sessionId, EState state, const BoardAvailableData& data) override
        {
            if (m_parent->m_onBoardAvailable) m_parent->m_onBoardAvailable(sessionId, state, data);
        }

        void On(unsigned sessionId, EState state, const RevokeBoardAvailableData& data) override
        {
            if (m_parent->m_onRevokeBoardAvailable) m_parent->m_onRevokeBoardAvailable(sessionId, state, data);
        }

        void On(unsigned sessionId, EState state, const TransportFinishedData& data) override
        {
            if (m_parent->m_onTransportFinished) m_parent->m_onTransportFinished(sessionId, state, data);
        }

        void On(unsigned sessionId, EState state, const BoardForecastData& data) override
        {
            if (m_parent->m_onBoardForecast) m_parent->m_onBoardForecast(sessionId, state, data);
        }

        void On(unsigned sessionId, const SendBoardInfoData& data) override
        {
            if (m_parent->m_onSendBoardInfo) m_parent->m_onSendBoardInfo(sessionId, data);
        }

        // Auxiliary messages:
        void On(unsigned sessionId, const NotificationData& data) override
        {
            if (m_parent->m_onNotification) m_parent->m_onNotification(sessionId, data);
        }

        void On(unsigned sessionId, const CheckAliveData& data) override
        {
            if (m_parent->m_onCheckAlive) m_parent->m_onCheckAlive(sessionId, data);
        }

        void On(unsigned sessionId, const CommandData& data) override
        {
            if (m_parent->m_onCommand) m_parent->m_onCommand(sessionId, data);
        }

        Upstream* m_parent;
    };

    std::atomic<bool>                        m_isRunning;
    std::thread                              m_networkThread;
    std::unique_ptr<InternalCallbackWrapper> m_callbackWrapper;
    std::unique_ptr<Hermes::Upstream>        m_upstream;

    ConnectedCallback           m_onConnected;
    DisconnectedCallback        m_onDisconnected;
    StateChangeCallback         m_onStateChange;
    TraceCallback               m_onTrace;
    ServiceDescriptionCallback  m_onServiceDescription;
    BoardAvailableCallback      m_onBoardAvailable;
    RevokeBoardAvailableCallback m_onRevokeBoardAvailable;
    TransportFinishedCallback   m_onTransportFinished;
    BoardForecastCallback       m_onBoardForecast;
    SendBoardInfoCallback       m_onSendBoardInfo;
    NotificationCallback        m_onNotification;
    CheckAliveCallback          m_onCheckAlive;
    CommandCallback             m_onCommand;
};

} // namespace Modern
} // namespace Hermes
