Hermes C++ Library (lib_cpp)
The Hermes Standard (IPC-HERMES-9852) is the modern, non-proprietary TCP/IP and XML-based successor to the legacy SMEMA standard. It enables vendor-independent machine-to-machine communication in SMT assembly lines. This repository provides the official, high-performance C++ implementation of the protocol.

1. Core Repository Map
To navigate the library effectively, refer to the following directory structure:

src/include/: The Public API. This directory contains the headers required to integrate Hermes into your application.

Hermes.hpp: The primary Object-Oriented C++ wrapper.

Hermes.h: The low-level C-style API.

src/Hermes/: The Implementation. Contains the core logic, including the ASIO networking stack, XML serialization, and the standard-compliant state machines.

test/BoostTestHermes/: Verification Suite. Contains comprehensive unit and integration tests. This is the source of truth for protocol-compliant behavior.

References/: External Headers. Contains local versions of pugixml and boost headers necessary for compilation in restricted environments.

2. Technical Requirements
Dependencies
C++ Standard: C++17 or higher.

Networking: Boost.ASIO (v1.66 through v1.78 recommended).

Note: Versions 1.87+ require the -DBOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT flag.

XML Processing: pugixml (included in References/).

Platform Independence
The library is designed to be fully platform-independent. It supports:

Windows: MSVC (2017+) and MinGW-w64.

Linux: GCC (7+) and Clang.

Build System: The project utilizes CMake (3.15+) to generate native build files (Visual Studio Solutions or Makefiles) for your specific platform.

3. Unified Build Process (CMake)
To build the library on any platform, use the following standard workflow:

Generate: mkdir build && cd build && cmake ..

Build: cmake --build . --config Release

Outputs:

hermes.dll / hermes.lib (Windows)

libhermes.so (Linux)

4. API Architecture & Design
The Callback Model
The C++ API follows a strict Interface-Based Callback pattern.

Users must implement classes inheriting from IDownstreamCallback (Receivers) or IUpstreamCallback (Senders).

All pure virtual methods in these interfaces must be overridden to handle protocol events (Connection, Disconnection, Board Available, Machine Ready, etc.).

Execution Model
Blocking Event Loop: The core processing occurs within the Run() method.

Threading: Because Run() blocks the calling thread to process network I/O, it must be executed in a dedicated background thread to maintain application responsiveness.

State Management: The library handles all internal state transitions (e.g., Not Connected -> Service Description -> Not Ready -> Ready) automatically based on the Enable() configuration.

5. Supported Communication Channels
The library implements the four primary channels defined by IPC-HERMES-9852:

Upstream: Machine-to-Machine (Sending).

Downstream: Machine-to-Machine (Receiving).

Configuration: Exchange of machine capabilities and line settings.

Vertical: Communication with factory-level MES/ERP systems.

6. Deployment Topologies
The library supports standard IPv4 networking across various hardware setups:

Point-to-Point: Direct Ethernet connection between two machines.

Switched Fabric: Deployment via factory-wide network switches. This is the preferred method for modern "Smart Factory" environments, as it allows a single network interface to handle both horizontal (machine) and vertical (MES) data simultaneously.

7. Quality Assurance
All protocol features are verified against the BoostTestHermes suite.

Unit Tests: Verify individual XML serialization and data structures.

Integration Tests: Simulate full handshakes between virtual Upstream and Downstream sessions.

Automation: Compatible with ctest for continuous integration workflows.

License: Copyright (c) ASM Assembly Systems GmbH & Co. KG. Licensed under the Apache License, Version 2.0. See COPYRIGHT.txt for details.