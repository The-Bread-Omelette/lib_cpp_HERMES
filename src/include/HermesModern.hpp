/***********************************************************************
Copyright 2018 ASM Assembly Systems GmbH & Co. KG

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
************************************************************************/

// Copyright (c) ASM Assembly Systems GmbH & Co. KG
//
// HermesModern.hpp
// ----------------
// Modern C++17 callback-based wrappers over the Hermes C++ interface (Hermes.hpp).
//
// These wrappers replace the pure-virtual callback interface with std::function
// registrations, removing the need to subclass IDownstreamCallback etc. directly.
//
// Covered classes:
//   Hermes::Modern::Downstream        -- horizontal lane, downstream side (receives from upstream machine)
//   Hermes::Modern::Upstream          -- horizontal lane, upstream side (receives from downstream machine)
//   Hermes::Modern::VerticalService   -- vertical interface, service/server side
//   Hermes::Modern::VerticalClient    -- vertical interface, client side
//   Hermes::Modern::ConfigurationService -- configuration service
//
// Threading model:
//   Each class spawns one internal thread that runs the Hermes IO loop.
//   Enable() must be called after the IO loop has started; it is posted into
//   the loop via Post() to guarantee correct sequencing.
//   All registered callbacks are invoked from the IO thread.
//
// Usage example (Downstream):
//
//   Hermes::Modern::Downstream ds(1);
//   ds.RegisterBoardAvailableCallback([](const Hermes::BoardAvailableData& d) {
//       // handle board available
//   });
//   Hermes::DownstreamSettings settings("MyMachine", 50101);
//   ds.Enable(settings);
//   // ...
//   ds.Stop();

#pragma once

#include "Hermes.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

namespace Hermes {
namespace Modern {

// =============================================================================
// Downstream
//
// Wraps Hermes::Downstream (horizontal lane, downstream port).
// The downstream side *receives* messages sent by the upstream machine:
//   ServiceDescriptionData, MachineReadyData, RevokeMachineReadyData,
//   StartTransportData, StopTransportData, QueryBoardInfoData (optional),
//   NotificationData, CheckAliveData (optional), CommandData.
//
// The downstream side *sends*:
//   ServiceDescriptionData, BoardAvailableData, RevokeBoardAvailableData,
//   TransportFinishedData, BoardForecastData, SendBoardInfoData,
//   NotificationData, CheckAliveData, CommandData.
// =============================================================================
class Downstream
{
public:
    // ---- Connection lifecycle callbacks ----

    /// Called when a TCP connection is established with the upstream machine.
    using ConnectedCallback    = std::function<void(unsigned sessionId, EState, const ConnectionInfo&)>;
    /// Called when the TCP connection is dropped.
    using DisconnectedCallback = std::function<void(unsigned sessionId, EState, const Error&)>;
    /// Called on every protocol state machine transition.
    using StateCallback        = std::function<void(unsigned sessionId, EState)>;
    /// Called for every internal trace/log event. Useful for debugging.
    using TraceCallback        = std::function<void(unsigned sessionId, ETraceType, StringView)>;

    // ---- Received message callbacks (sent by the upstream machine) ----

    /// Received: upstream declares its service capabilities.
    using ServiceDescriptionCallback    = std::function<void(unsigned sessionId, EState, const ServiceDescriptionData&)>;
    /// Received: upstream signals it is ready to accept a board.
    using MachineReadyCallback          = std::function<void(unsigned sessionId, EState, const MachineReadyData&)>;
    /// Received: upstream revokes a previously sent MachineReady.
    using RevokeMachineReadyCallback    = std::function<void(unsigned sessionId, EState, const RevokeMachineReadyData&)>;
    /// Received: upstream commands the conveyor to start moving the board.
    using StartTransportCallback        = std::function<void(unsigned sessionId, EState, const StartTransportData&)>;
    /// Received: upstream commands the conveyor to stop.
    using StopTransportCallback         = std::function<void(unsigned sessionId, EState, const StopTransportData&)>;
    /// Received: upstream requests board information (optional message).
    using QueryBoardInfoCallback        = std::function<void(unsigned sessionId, const QueryBoardInfoData&)>;
    /// Received: upstream sends a notification (e.g. error, shutdown).
    using NotificationCallback          = std::function<void(unsigned sessionId, const NotificationData&)>;
    /// Received: upstream sends a check-alive ping/pong (optional message).
    using CheckAliveCallback            = std::function<void(unsigned sessionId, const CheckAliveData&)>;
    /// Received: upstream sends a command.
    using CommandCallback               = std::function<void(unsigned sessionId, const CommandData&)>;

    /// @param laneId  The lane number this downstream port belongs to (1-based).
    explicit Downstream(unsigned laneId)
        : m_laneId(laneId)
    {
        m_callbackWrapper = std::make_unique<CallbackWrapper>(this);
        m_downstream      = std::make_unique<Hermes::Downstream>(laneId, *m_callbackWrapper);
    }

    ~Downstream() { Stop(); }

    Downstream(const Downstream&)            = delete;
    Downstream& operator=(const Downstream&) = delete;

    // ---- Callback registration ----
    void RegisterConnectedCallback(ConnectedCallback cb)                   { m_onConnected = std::move(cb); }
    void RegisterDisconnectedCallback(DisconnectedCallback cb)             { m_onDisconnected = std::move(cb); }
    void RegisterStateCallback(StateCallback cb)                           { m_onState = std::move(cb); }
    void RegisterTraceCallback(TraceCallback cb)                           { m_onTrace = std::move(cb); }
    void RegisterServiceDescriptionCallback(ServiceDescriptionCallback cb) { m_onServiceDescription = std::move(cb); }
    void RegisterMachineReadyCallback(MachineReadyCallback cb)             { m_onMachineReady = std::move(cb); }
    void RegisterRevokeMachineReadyCallback(RevokeMachineReadyCallback cb) { m_onRevokeMachineReady = std::move(cb); }
    void RegisterStartTransportCallback(StartTransportCallback cb)         { m_onStartTransport = std::move(cb); }
    void RegisterStopTransportCallback(StopTransportCallback cb)           { m_onStopTransport = std::move(cb); }
    void RegisterQueryBoardInfoCallback(QueryBoardInfoCallback cb)         { m_onQueryBoardInfo = std::move(cb); }
    void RegisterNotificationCallback(NotificationCallback cb)             { m_onNotification = std::move(cb); }
    void RegisterCheckAliveCallback(CheckAliveCallback cb)                 { m_onCheckAlive = std::move(cb); }
    void RegisterCommandCallback(CommandCallback cb)                       { m_onCommand = std::move(cb); }

    // ---- Lifecycle ----

    /// Start the IO loop and enable the downstream port.
    /// Enable() is posted into the IO thread to guarantee it executes after Run() starts.
    /// Safe to call only once; subsequent calls while running are no-ops.
    void Enable(const DownstreamSettings& settings)
    {
        if (m_isRunning.exchange(true))
            return;

        m_networkThread = std::thread([this, settings]()
        {
            m_downstream->Post([this, settings]()
            {
                m_downstream->Enable(settings);
            });
            m_downstream->Run();
        });
    }

    /// Stop the IO loop and join the thread. Safe to call multiple times.
    void Stop()
    {
        if (!m_isRunning.exchange(false))
            return;
        m_downstream->Stop();
        if (m_networkThread.joinable())
            m_networkThread.join();
    }

    // ---- Sending messages ----
    // These must be called from the IO thread (e.g. from within a callback,
    // or posted via Post()). Calling from an external thread is safe only
    // if done via Post().

    /// Send a message to the connected upstream machine.
    template<typename T>
    void Signal(unsigned sessionId, const T& data) { m_downstream->Signal(sessionId, data); }

    /// Post a callable into the IO thread. Use this to call Signal() safely
    /// from outside the IO thread.
    template<typename F>
    void Post(F&& f) { m_downstream->Post(std::forward<F>(f)); }

    /// Reset all sessions with a notification (e.g. on shutdown or config change).
    void Reset(const NotificationData& data) { m_downstream->Reset(data); }

    /// Disable the port gracefully with a notification.
    void Disable(const NotificationData& data) { m_downstream->Disable(data); }

private:
    // Bridges Hermes::IDownstreamCallback pure virtuals to our std::function members.
    class CallbackWrapper : public Hermes::IDownstreamCallback
    {
    public:
        explicit CallbackWrapper(Downstream* parent) : m_parent(parent) {}

        void OnConnected(unsigned sessionId, EState state, const ConnectionInfo& info) override
        {
            if (m_parent->m_onConnected) m_parent->m_onConnected(sessionId, state, info);
        }
        void OnDisconnected(unsigned sessionId, EState state, const Error& error) override
        {
            if (m_parent->m_onDisconnected) m_parent->m_onDisconnected(sessionId, state, error);
        }
        void OnState(unsigned sessionId, EState state) override
        {
            if (m_parent->m_onState) m_parent->m_onState(sessionId, state);
        }
        void OnTrace(unsigned sessionId, ETraceType type, StringView trace) override
        {
            if (m_parent->m_onTrace) m_parent->m_onTrace(sessionId, type, trace);
        }

        // Core messages received from the upstream machine:
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

        // Optional messages (non-abstract in IDownstreamCallback):
        void On(unsigned sessionId, const QueryBoardInfoData& data) override
        {
            if (m_parent->m_onQueryBoardInfo) m_parent->m_onQueryBoardInfo(sessionId, data);
        }
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

    private:
        Downstream* m_parent;
    };

    unsigned                           m_laneId;
    std::atomic<bool>                  m_isRunning{false};
    std::thread                        m_networkThread;
    std::unique_ptr<CallbackWrapper>   m_callbackWrapper;
    std::unique_ptr<Hermes::Downstream> m_downstream;

    ConnectedCallback           m_onConnected;
    DisconnectedCallback        m_onDisconnected;
    StateCallback               m_onState;
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
// Upstream
//
// Wraps Hermes::Upstream (horizontal lane, upstream port).
// The upstream side *receives* messages sent by the downstream machine:
//   ServiceDescriptionData, BoardAvailableData, RevokeBoardAvailableData,
//   TransportFinishedData, BoardForecastData (optional), SendBoardInfoData (optional),
//   NotificationData, CheckAliveData (optional), CommandData.
//
// The upstream side *sends*:
//   ServiceDescriptionData, MachineReadyData, RevokeMachineReadyData,
//   StartTransportData, StopTransportData, QueryBoardInfoData,
//   NotificationData, CheckAliveData, CommandData.
// =============================================================================
class Upstream
{
public:
    // ---- Connection lifecycle callbacks ----

    /// Called when a TCP connection is established with the downstream machine.
    using ConnectedCallback    = std::function<void(unsigned sessionId, EState, const ConnectionInfo&)>;
    /// Called when the TCP connection is dropped.
    using DisconnectedCallback = std::function<void(unsigned sessionId, EState, const Error&)>;
    /// Called on every protocol state machine transition.
    using StateCallback        = std::function<void(unsigned sessionId, EState)>;
    /// Called for every internal trace/log event.
    using TraceCallback        = std::function<void(unsigned sessionId, ETraceType, StringView)>;

    // ---- Received message callbacks (sent by the downstream machine) ----

    /// Received: downstream declares its service capabilities.
    using ServiceDescriptionCallback    = std::function<void(unsigned sessionId, EState, const ServiceDescriptionData&)>;
    /// Received: downstream signals a board is available for transfer.
    using BoardAvailableCallback        = std::function<void(unsigned sessionId, EState, const BoardAvailableData&)>;
    /// Received: downstream revokes a previously sent BoardAvailable.
    using RevokeBoardAvailableCallback  = std::function<void(unsigned sessionId, EState, const RevokeBoardAvailableData&)>;
    /// Received: downstream signals the board transfer is complete.
    using TransportFinishedCallback     = std::function<void(unsigned sessionId, EState, const TransportFinishedData&)>;
    /// Received: downstream forecasts an upcoming board (optional message).
    using BoardForecastCallback         = std::function<void(unsigned sessionId, EState, const BoardForecastData&)>;
    /// Received: downstream sends board information in response to QueryBoardInfo (optional message).
    using SendBoardInfoCallback         = std::function<void(unsigned sessionId, const SendBoardInfoData&)>;
    /// Received: downstream sends a notification.
    using NotificationCallback          = std::function<void(unsigned sessionId, const NotificationData&)>;
    /// Received: downstream sends a check-alive ping/pong (optional message).
    using CheckAliveCallback            = std::function<void(unsigned sessionId, const CheckAliveData&)>;
    /// Received: downstream sends a command.
    using CommandCallback               = std::function<void(unsigned sessionId, const CommandData&)>;

    /// @param laneId  The lane number this upstream port belongs to (1-based).
    explicit Upstream(unsigned laneId)
        : m_laneId(laneId)
    {
        m_callbackWrapper = std::make_unique<CallbackWrapper>(this);
        m_upstream        = std::make_unique<Hermes::Upstream>(laneId, *m_callbackWrapper);
    }

    ~Upstream() { Stop(); }

    Upstream(const Upstream&)            = delete;
    Upstream& operator=(const Upstream&) = delete;

    // ---- Callback registration ----
    void RegisterConnectedCallback(ConnectedCallback cb)                       { m_onConnected = std::move(cb); }
    void RegisterDisconnectedCallback(DisconnectedCallback cb)                 { m_onDisconnected = std::move(cb); }
    void RegisterStateCallback(StateCallback cb)                               { m_onState = std::move(cb); }
    void RegisterTraceCallback(TraceCallback cb)                               { m_onTrace = std::move(cb); }
    void RegisterServiceDescriptionCallback(ServiceDescriptionCallback cb)     { m_onServiceDescription = std::move(cb); }
    void RegisterBoardAvailableCallback(BoardAvailableCallback cb)             { m_onBoardAvailable = std::move(cb); }
    void RegisterRevokeBoardAvailableCallback(RevokeBoardAvailableCallback cb) { m_onRevokeBoardAvailable = std::move(cb); }
    void RegisterTransportFinishedCallback(TransportFinishedCallback cb)       { m_onTransportFinished = std::move(cb); }
    void RegisterBoardForecastCallback(BoardForecastCallback cb)               { m_onBoardForecast = std::move(cb); }
    void RegisterSendBoardInfoCallback(SendBoardInfoCallback cb)               { m_onSendBoardInfo = std::move(cb); }
    void RegisterNotificationCallback(NotificationCallback cb)                 { m_onNotification = std::move(cb); }
    void RegisterCheckAliveCallback(CheckAliveCallback cb)                     { m_onCheckAlive = std::move(cb); }
    void RegisterCommandCallback(CommandCallback cb)                           { m_onCommand = std::move(cb); }

    // ---- Lifecycle ----

    /// Start the IO loop and enable the upstream port.
    /// Enable() is posted into the IO thread to guarantee it executes after Run() starts.
    void Enable(const UpstreamSettings& settings)
    {
        if (m_isRunning.exchange(true))
            return;

        m_networkThread = std::thread([this, settings]()
        {
            m_upstream->Post([this, settings]()
            {
                m_upstream->Enable(settings);
            });
            m_upstream->Run();
        });
    }

    /// Stop the IO loop and join the thread. Safe to call multiple times.
    void Stop()
    {
        if (!m_isRunning.exchange(false))
            return;
        m_upstream->Stop();
        if (m_networkThread.joinable())
            m_networkThread.join();
    }

    // ---- Sending messages ----

    /// Send a message to the connected downstream machine.
    template<typename T>
    void Signal(unsigned sessionId, const T& data) { m_upstream->Signal(sessionId, data); }

    /// Post a callable into the IO thread.
    template<typename F>
    void Post(F&& f) { m_upstream->Post(std::forward<F>(f)); }

    /// Reset all sessions with a notification.
    void Reset(const NotificationData& data) { m_upstream->Reset(data); }

    /// Disable the port gracefully.
    void Disable(const NotificationData& data) { m_upstream->Disable(data); }

private:
    class CallbackWrapper : public Hermes::IUpstreamCallback
    {
    public:
        explicit CallbackWrapper(Upstream* parent) : m_parent(parent) {}

        void OnConnected(unsigned sessionId, EState state, const ConnectionInfo& info) override
        {
            if (m_parent->m_onConnected) m_parent->m_onConnected(sessionId, state, info);
        }
        void OnDisconnected(unsigned sessionId, EState state, const Error& error) override
        {
            if (m_parent->m_onDisconnected) m_parent->m_onDisconnected(sessionId, state, error);
        }
        void OnState(unsigned sessionId, EState state) override
        {
            if (m_parent->m_onState) m_parent->m_onState(sessionId, state);
        }
        void OnTrace(unsigned sessionId, ETraceType type, StringView trace) override
        {
            if (m_parent->m_onTrace) m_parent->m_onTrace(sessionId, type, trace);
        }

        // Core messages received from the downstream machine:
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

        // Optional messages (non-abstract in IUpstreamCallback):
        void On(unsigned sessionId, EState state, const BoardForecastData& data) override
        {
            if (m_parent->m_onBoardForecast) m_parent->m_onBoardForecast(sessionId, state, data);
        }
        void On(unsigned sessionId, const SendBoardInfoData& data) override
        {
            if (m_parent->m_onSendBoardInfo) m_parent->m_onSendBoardInfo(sessionId, data);
        }
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

    private:
        Upstream* m_parent;
    };

    unsigned                          m_laneId;
    std::atomic<bool>                 m_isRunning{false};
    std::thread                       m_networkThread;
    std::unique_ptr<CallbackWrapper>  m_callbackWrapper;
    std::unique_ptr<Hermes::Upstream> m_upstream;

    ConnectedCallback           m_onConnected;
    DisconnectedCallback        m_onDisconnected;
    StateCallback               m_onState;
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


// =============================================================================
// VerticalService
//
// Wraps Hermes::VerticalService (vertical supervisory interface, server side).
// Receives from the vertical client:
//   SupervisoryServiceDescriptionData, GetConfigurationData, SetConfigurationData,
//   SendWorkOrderInfoData (optional), QueryHermesCapabilitiesData,
//   NotificationData, CheckAliveData (optional).
//
// Sends to the vertical client:
//   SupervisoryServiceDescriptionData, BoardArrivedData, BoardDepartedData,
//   QueryWorkOrderInfoData, ReplyWorkOrderInfoData, SendHermesCapabilitiesData,
//   CurrentConfigurationData, NotificationData, CheckAliveData, CommandData.
// =============================================================================
class VerticalService
{
public:
    // ---- Connection lifecycle callbacks ----
    using ConnectedCallback    = std::function<void(unsigned sessionId, EVerticalState, const ConnectionInfo&)>;
    using DisconnectedCallback = std::function<void(unsigned sessionId, EVerticalState, const Error&)>;
    using TraceCallback        = std::function<void(unsigned sessionId, ETraceType, StringView)>;

    // ---- Received message callbacks ----

    /// Received: client declares its supervisory service capabilities.
    using ServiceDescriptionCallback       = std::function<void(unsigned sessionId, EVerticalState, const SupervisoryServiceDescriptionData&)>;
    /// Received: client requests the current configuration.
    using GetConfigurationCallback         = std::function<void(unsigned sessionId, const GetConfigurationData&, const ConnectionInfo&)>;
    /// Received: client sets a new configuration.
    using SetConfigurationCallback         = std::function<void(unsigned sessionId, const SetConfigurationData&, const ConnectionInfo&)>;
    /// Received: client sends work order information (optional message).
    using SendWorkOrderInfoCallback        = std::function<void(unsigned sessionId, const SendWorkOrderInfoData&)>;
    /// Received: client sends a notification.
    using NotificationCallback             = std::function<void(unsigned sessionId, const NotificationData&)>;
    /// Received: client queries Hermes capabilities.
    using QueryHermesCapabilitiesCallback  = std::function<void(unsigned sessionId, const QueryHermesCapabilitiesData&)>;
    /// Received: client sends a check-alive (optional message).
    using CheckAliveCallback               = std::function<void(unsigned sessionId, const CheckAliveData&)>;

    explicit VerticalService()
    {
        m_callbackWrapper = std::make_unique<CallbackWrapper>(this);
        m_service         = std::make_unique<Hermes::VerticalService>(*m_callbackWrapper);
    }

    ~VerticalService() { Stop(); }

    VerticalService(const VerticalService&)            = delete;
    VerticalService& operator=(const VerticalService&) = delete;

    // ---- Callback registration ----
    void RegisterConnectedCallback(ConnectedCallback cb)                             { m_onConnected = std::move(cb); }
    void RegisterDisconnectedCallback(DisconnectedCallback cb)                       { m_onDisconnected = std::move(cb); }
    void RegisterTraceCallback(TraceCallback cb)                                     { m_onTrace = std::move(cb); }
    void RegisterServiceDescriptionCallback(ServiceDescriptionCallback cb)           { m_onServiceDescription = std::move(cb); }
    void RegisterGetConfigurationCallback(GetConfigurationCallback cb)               { m_onGetConfiguration = std::move(cb); }
    void RegisterSetConfigurationCallback(SetConfigurationCallback cb)               { m_onSetConfiguration = std::move(cb); }
    void RegisterSendWorkOrderInfoCallback(SendWorkOrderInfoCallback cb)             { m_onSendWorkOrderInfo = std::move(cb); }
    void RegisterNotificationCallback(NotificationCallback cb)                       { m_onNotification = std::move(cb); }
    void RegisterQueryHermesCapabilitiesCallback(QueryHermesCapabilitiesCallback cb) { m_onQueryHermesCapabilities = std::move(cb); }
    void RegisterCheckAliveCallback(CheckAliveCallback cb)                           { m_onCheckAlive = std::move(cb); }

    // ---- Lifecycle ----

    /// Start the IO loop and enable the vertical service.
    void Enable(const VerticalServiceSettings& settings)
    {
        if (m_isRunning.exchange(true))
            return;

        m_networkThread = std::thread([this, settings]()
        {
            m_service->Post([this, settings]()
            {
                m_service->Enable(settings);
            });
            m_service->Run();
        });
    }

    /// Stop the IO loop and join the thread. Safe to call multiple times.
    void Stop()
    {
        if (!m_isRunning.exchange(false))
            return;
        m_service->Stop();
        if (m_networkThread.joinable())
            m_networkThread.join();
    }

    // ---- Sending messages ----

    template<typename T>
    void Signal(unsigned sessionId, const T& data) { m_service->Signal(sessionId, data); }

    /// Broadcast BoardArrivedData to all clients that requested FeatureBoardTracking.
    void Signal(const BoardArrivedData& data)  { m_service->Signal(data); }
    /// Broadcast BoardDepartedData to all clients that requested FeatureBoardTracking.
    void Signal(const BoardDepartedData& data) { m_service->Signal(data); }

    template<typename F>
    void Post(F&& f) { m_service->Post(std::forward<F>(f)); }

    void ResetSession(unsigned sessionId, const NotificationData& data) { m_service->ResetSession(sessionId, data); }
    void Disable(const NotificationData& data) { m_service->Disable(data); }

private:
    class CallbackWrapper : public Hermes::IVerticalServiceCallback
    {
    public:
        explicit CallbackWrapper(VerticalService* parent) : m_parent(parent) {}

        void OnConnected(unsigned sessionId, EVerticalState state, const ConnectionInfo& info) override
        {
            if (m_parent->m_onConnected) m_parent->m_onConnected(sessionId, state, info);
        }
        void OnDisconnected(unsigned sessionId, EVerticalState state, const Error& error) override
        {
            if (m_parent->m_onDisconnected) m_parent->m_onDisconnected(sessionId, state, error);
        }
        void OnTrace(unsigned sessionId, ETraceType type, StringView trace) override
        {
            if (m_parent->m_onTrace) m_parent->m_onTrace(sessionId, type, trace);
        }
        void On(unsigned sessionId, EVerticalState state, const SupervisoryServiceDescriptionData& data) override
        {
            if (m_parent->m_onServiceDescription) m_parent->m_onServiceDescription(sessionId, state, data);
        }
        void On(unsigned sessionId, const GetConfigurationData& data, const ConnectionInfo& info) override
        {
            if (m_parent->m_onGetConfiguration) m_parent->m_onGetConfiguration(sessionId, data, info);
        }
        void On(unsigned sessionId, const SetConfigurationData& data, const ConnectionInfo& info) override
        {
            if (m_parent->m_onSetConfiguration) m_parent->m_onSetConfiguration(sessionId, data, info);
        }
        void On(unsigned sessionId, const SendWorkOrderInfoData& data) override
        {
            if (m_parent->m_onSendWorkOrderInfo) m_parent->m_onSendWorkOrderInfo(sessionId, data);
        }
        void On(unsigned sessionId, const NotificationData& data) override
        {
            if (m_parent->m_onNotification) m_parent->m_onNotification(sessionId, data);
        }
        void On(unsigned sessionId, const QueryHermesCapabilitiesData& data) override
        {
            if (m_parent->m_onQueryHermesCapabilities) m_parent->m_onQueryHermesCapabilities(sessionId, data);
        }
        void On(unsigned sessionId, const CheckAliveData& data) override
        {
            if (m_parent->m_onCheckAlive) m_parent->m_onCheckAlive(sessionId, data);
        }

    private:
        VerticalService* m_parent;
    };

    std::atomic<bool>                        m_isRunning{false};
    std::thread                              m_networkThread;
    std::unique_ptr<CallbackWrapper>         m_callbackWrapper;
    std::unique_ptr<Hermes::VerticalService> m_service;

    ConnectedCallback                  m_onConnected;
    DisconnectedCallback               m_onDisconnected;
    TraceCallback                      m_onTrace;
    ServiceDescriptionCallback         m_onServiceDescription;
    GetConfigurationCallback           m_onGetConfiguration;
    SetConfigurationCallback           m_onSetConfiguration;
    SendWorkOrderInfoCallback          m_onSendWorkOrderInfo;
    NotificationCallback               m_onNotification;
    QueryHermesCapabilitiesCallback    m_onQueryHermesCapabilities;
    CheckAliveCallback                 m_onCheckAlive;
};


// =============================================================================
// VerticalClient
//
// Wraps Hermes::VerticalClient (vertical supervisory interface, client side).
// Receives from the vertical service:
//   SupervisoryServiceDescriptionData, BoardArrivedData (optional),
//   BoardDepartedData (optional), QueryWorkOrderInfoData, ReplyWorkOrderInfoData,
//   CurrentConfigurationData (optional), SendHermesCapabilitiesData (optional),
//   NotificationData, CheckAliveData (optional).
//
// Sends to the vertical service:
//   SupervisoryServiceDescriptionData, GetConfigurationData, SetConfigurationData,
//   QueryHermesCapabilitiesData, SendWorkOrderInfoData,
//   NotificationData, CheckAliveData.
// =============================================================================
class VerticalClient
{
public:
    // ---- Connection lifecycle callbacks ----
    using ConnectedCallback    = std::function<void(unsigned sessionId, EVerticalState, const ConnectionInfo&)>;
    using DisconnectedCallback = std::function<void(unsigned sessionId, EVerticalState, const Error&)>;
    using TraceCallback        = std::function<void(unsigned sessionId, ETraceType, StringView)>;

    // ---- Received message callbacks ----

    /// Received: service declares its supervisory service capabilities.
    using ServiceDescriptionCallback    = std::function<void(unsigned sessionId, EVerticalState, const SupervisoryServiceDescriptionData&)>;
    /// Received: service notifies a board has arrived at a machine (optional message).
    using BoardArrivedCallback          = std::function<void(unsigned sessionId, const BoardArrivedData&)>;
    /// Received: service notifies a board has departed a machine (optional message).
    using BoardDepartedCallback         = std::function<void(unsigned sessionId, const BoardDepartedData&)>;
    /// Received: service queries work order information.
    using QueryWorkOrderInfoCallback    = std::function<void(unsigned sessionId, const QueryWorkOrderInfoData&)>;
    /// Received: service replies to a SendWorkOrderInfo request.
    using ReplyWorkOrderInfoCallback    = std::function<void(unsigned sessionId, const ReplyWorkOrderInfoData&)>;
    /// Received: service sends the current configuration (optional message).
    using CurrentConfigurationCallback  = std::function<void(unsigned sessionId, const CurrentConfigurationData&)>;
    /// Received: service sends its Hermes capabilities (optional message).
    using SendHermesCapabilitiesCallback = std::function<void(unsigned sessionId, const SendHermesCapabilitiesData&)>;
    /// Received: service sends a notification.
    using NotificationCallback          = std::function<void(unsigned sessionId, const NotificationData&)>;
    /// Received: service sends a check-alive (optional message).
    using CheckAliveCallback            = std::function<void(unsigned sessionId, const CheckAliveData&)>;

    explicit VerticalClient()
    {
        m_callbackWrapper = std::make_unique<CallbackWrapper>(this);
        m_client          = std::make_unique<Hermes::VerticalClient>(*m_callbackWrapper);
    }

    ~VerticalClient() { Stop(); }

    VerticalClient(const VerticalClient&)            = delete;
    VerticalClient& operator=(const VerticalClient&) = delete;

    // ---- Callback registration ----
    void RegisterConnectedCallback(ConnectedCallback cb)                              { m_onConnected = std::move(cb); }
    void RegisterDisconnectedCallback(DisconnectedCallback cb)                        { m_onDisconnected = std::move(cb); }
    void RegisterTraceCallback(TraceCallback cb)                                      { m_onTrace = std::move(cb); }
    void RegisterServiceDescriptionCallback(ServiceDescriptionCallback cb)            { m_onServiceDescription = std::move(cb); }
    void RegisterBoardArrivedCallback(BoardArrivedCallback cb)                        { m_onBoardArrived = std::move(cb); }
    void RegisterBoardDepartedCallback(BoardDepartedCallback cb)                      { m_onBoardDeparted = std::move(cb); }
    void RegisterQueryWorkOrderInfoCallback(QueryWorkOrderInfoCallback cb)            { m_onQueryWorkOrderInfo = std::move(cb); }
    void RegisterReplyWorkOrderInfoCallback(ReplyWorkOrderInfoCallback cb)            { m_onReplyWorkOrderInfo = std::move(cb); }
    void RegisterCurrentConfigurationCallback(CurrentConfigurationCallback cb)        { m_onCurrentConfiguration = std::move(cb); }
    void RegisterSendHermesCapabilitiesCallback(SendHermesCapabilitiesCallback cb)    { m_onSendHermesCapabilities = std::move(cb); }
    void RegisterNotificationCallback(NotificationCallback cb)                        { m_onNotification = std::move(cb); }
    void RegisterCheckAliveCallback(CheckAliveCallback cb)                            { m_onCheckAlive = std::move(cb); }

    // ---- Lifecycle ----

    /// Start the IO loop and enable the vertical client.
    void Enable(const VerticalClientSettings& settings)
    {
        if (m_isRunning.exchange(true))
            return;

        m_networkThread = std::thread([this, settings]()
        {
            m_client->Post([this, settings]()
            {
                m_client->Enable(settings);
            });
            m_client->Run();
        });
    }

    /// Stop the IO loop and join the thread. Safe to call multiple times.
    void Stop()
    {
        if (!m_isRunning.exchange(false))
            return;
        m_client->Stop();
        if (m_networkThread.joinable())
            m_networkThread.join();
    }

    // ---- Sending messages ----

    template<typename T>
    void Signal(unsigned sessionId, const T& data) { m_client->Signal(sessionId, data); }

    template<typename F>
    void Post(F&& f) { m_client->Post(std::forward<F>(f)); }

    void Reset(const NotificationData& data) { m_client->Reset(data); }
    void Disable(const NotificationData& data) { m_client->Disable(data); }

private:
    class CallbackWrapper : public Hermes::IVerticalClientCallback
    {
    public:
        explicit CallbackWrapper(VerticalClient* parent) : m_parent(parent) {}

        void OnConnected(unsigned sessionId, EVerticalState state, const ConnectionInfo& info) override
        {
            if (m_parent->m_onConnected) m_parent->m_onConnected(sessionId, state, info);
        }
        void OnDisconnected(unsigned sessionId, EVerticalState state, const Error& error) override
        {
            if (m_parent->m_onDisconnected) m_parent->m_onDisconnected(sessionId, state, error);
        }
        void OnTrace(unsigned sessionId, ETraceType type, StringView trace) override
        {
            if (m_parent->m_onTrace) m_parent->m_onTrace(sessionId, type, trace);
        }
        void On(unsigned sessionId, EVerticalState state, const SupervisoryServiceDescriptionData& data) override
        {
            if (m_parent->m_onServiceDescription) m_parent->m_onServiceDescription(sessionId, state, data);
        }
        void On(unsigned sessionId, const BoardArrivedData& data) override
        {
            if (m_parent->m_onBoardArrived) m_parent->m_onBoardArrived(sessionId, data);
        }
        void On(unsigned sessionId, const BoardDepartedData& data) override
        {
            if (m_parent->m_onBoardDeparted) m_parent->m_onBoardDeparted(sessionId, data);
        }
        void On(unsigned sessionId, const QueryWorkOrderInfoData& data) override
        {
            if (m_parent->m_onQueryWorkOrderInfo) m_parent->m_onQueryWorkOrderInfo(sessionId, data);
        }
        void On(unsigned sessionId, const ReplyWorkOrderInfoData& data) override
        {
            if (m_parent->m_onReplyWorkOrderInfo) m_parent->m_onReplyWorkOrderInfo(sessionId, data);
        }
        void On(unsigned sessionId, const CurrentConfigurationData& data) override
        {
            if (m_parent->m_onCurrentConfiguration) m_parent->m_onCurrentConfiguration(sessionId, data);
        }
        void On(unsigned sessionId, const SendHermesCapabilitiesData& data) override
        {
            if (m_parent->m_onSendHermesCapabilities) m_parent->m_onSendHermesCapabilities(sessionId, data);
        }
        void On(unsigned sessionId, const NotificationData& data) override
        {
            if (m_parent->m_onNotification) m_parent->m_onNotification(sessionId, data);
        }
        void On(unsigned sessionId, const CheckAliveData& data) override
        {
            if (m_parent->m_onCheckAlive) m_parent->m_onCheckAlive(sessionId, data);
        }

    private:
        VerticalClient* m_parent;
    };

    std::atomic<bool>                       m_isRunning{false};
    std::thread                             m_networkThread;
    std::unique_ptr<CallbackWrapper>        m_callbackWrapper;
    std::unique_ptr<Hermes::VerticalClient> m_client;

    ConnectedCallback               m_onConnected;
    DisconnectedCallback            m_onDisconnected;
    TraceCallback                   m_onTrace;
    ServiceDescriptionCallback      m_onServiceDescription;
    BoardArrivedCallback            m_onBoardArrived;
    BoardDepartedCallback           m_onBoardDeparted;
    QueryWorkOrderInfoCallback      m_onQueryWorkOrderInfo;
    ReplyWorkOrderInfoCallback      m_onReplyWorkOrderInfo;
    CurrentConfigurationCallback    m_onCurrentConfiguration;
    SendHermesCapabilitiesCallback  m_onSendHermesCapabilities;
    NotificationCallback            m_onNotification;
    CheckAliveCallback              m_onCheckAlive;
};


// =============================================================================
// ConfigurationService
//
// Wraps Hermes::ConfigurationService.
// Handles GetConfiguration and SetConfiguration requests from remote clients.
//
// Note: Unlike the horizontal/vertical wrappers, OnGetConfiguration and
// OnSetConfiguration have return values (they are query/response patterns),
// so they use std::function with return types rather than void callbacks.
// =============================================================================
class ConfigurationService
{
public:
    using ConnectedCallback    = std::function<void(unsigned sessionId, const ConnectionInfo&)>;
    using DisconnectedCallback = std::function<void(unsigned sessionId, const Error&)>;
    using TraceCallback        = std::function<void(unsigned sessionId, ETraceType, StringView)>;

    /// Called when a remote client requests the current configuration.
    /// Must return a CurrentConfigurationData. Required.
    using GetConfigurationCallback = std::function<CurrentConfigurationData(unsigned sessionId, const ConnectionInfo&)>;

    /// Called when a remote client pushes a new configuration.
    /// Must return an Error (return Error{} for success). Required.
    using SetConfigurationCallback = std::function<Error(unsigned sessionId, const ConnectionInfo&, const SetConfigurationData&)>;

    explicit ConfigurationService()
    {
        m_callbackWrapper = std::make_unique<CallbackWrapper>(this);
        m_service         = std::make_unique<Hermes::ConfigurationService>(*m_callbackWrapper);
    }

    ~ConfigurationService() { Stop(); }

    ConfigurationService(const ConfigurationService&)            = delete;
    ConfigurationService& operator=(const ConfigurationService&) = delete;

    // ---- Callback registration ----
    void RegisterConnectedCallback(ConnectedCallback cb)               { m_onConnected = std::move(cb); }
    void RegisterDisconnectedCallback(DisconnectedCallback cb)         { m_onDisconnected = std::move(cb); }
    void RegisterTraceCallback(TraceCallback cb)                       { m_onTrace = std::move(cb); }

    /// Register the get-configuration handler. This is required; the service
    /// cannot respond to GetConfiguration requests without it.
    void RegisterGetConfigurationCallback(GetConfigurationCallback cb) { m_onGetConfiguration = std::move(cb); }

    /// Register the set-configuration handler. This is required; the service
    /// cannot process SetConfiguration requests without it.
    void RegisterSetConfigurationCallback(SetConfigurationCallback cb) { m_onSetConfiguration = std::move(cb); }

    // ---- Lifecycle ----

    void Enable(const ConfigurationServiceSettings& settings)
    {
        if (m_isRunning.exchange(true))
            return;

        m_networkThread = std::thread([this, settings]()
        {
            m_service->Post([this, settings]()
            {
                m_service->Enable(settings);
            });
            m_service->Run();
        });
    }

    void Stop()
    {
        if (!m_isRunning.exchange(false))
            return;
        m_service->Stop();
        if (m_networkThread.joinable())
            m_networkThread.join();
    }

    template<typename F>
    void Post(F&& f) { m_service->Post(std::forward<F>(f)); }

    void Disable(const NotificationData& data) { m_service->Disable(data); }

private:
    class CallbackWrapper : public Hermes::IConfigurationServiceCallback
    {
    public:
        explicit CallbackWrapper(ConfigurationService* parent) : m_parent(parent) {}

        void OnConnected(unsigned sessionId, const ConnectionInfo& info) override
        {
            if (m_parent->m_onConnected) m_parent->m_onConnected(sessionId, info);
        }
        void OnDisconnected(unsigned sessionId, const Error& error) override
        {
            if (m_parent->m_onDisconnected) m_parent->m_onDisconnected(sessionId, error);
        }
        void OnTrace(unsigned sessionId, ETraceType type, StringView trace) override
        {
            if (m_parent->m_onTrace) m_parent->m_onTrace(sessionId, type, trace);
        }
        CurrentConfigurationData OnGetConfiguration(unsigned sessionId, const ConnectionInfo& info) override
        {
            if (m_parent->m_onGetConfiguration)
                return m_parent->m_onGetConfiguration(sessionId, info);
            return CurrentConfigurationData{};
        }
        Error OnSetConfiguration(unsigned sessionId, const ConnectionInfo& info, const SetConfigurationData& data) override
        {
            if (m_parent->m_onSetConfiguration)
                return m_parent->m_onSetConfiguration(sessionId, info, data);
            return Error{};
        }

    private:
        ConfigurationService* m_parent;
    };

    std::atomic<bool>                              m_isRunning{false};
    std::thread                                    m_networkThread;
    std::unique_ptr<CallbackWrapper>               m_callbackWrapper;
    std::unique_ptr<Hermes::ConfigurationService>  m_service;

    ConnectedCallback           m_onConnected;
    DisconnectedCallback        m_onDisconnected;
    TraceCallback               m_onTrace;
    GetConfigurationCallback    m_onGetConfiguration;
    SetConfigurationCallback    m_onSetConfiguration;
};

} // namespace Modern
} // namespace Hermes