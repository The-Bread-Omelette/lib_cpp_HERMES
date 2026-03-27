# Hermes C++ Library — Complete Reference

**Protocol:** The Hermes Standard — vendor-independent machine-to-machine communication for SMT assembly lines  
**License:** Apache 2.0  
**Requires:** C++17, Boost 1.66+, CMake 3.15+

---

## Table of Contents

1. [What is Hermes](#1-what-is-hermes)
2. [Architecture overview](#2-architecture-overview)
3. [Building the library](#3-building-the-library)
4. [Header map — what to include](#4-header-map)
5. [Core types reference](#5-core-types-reference)
6. [Modern C++ API — HermesModern.hpp](#6-modern-c-api)
7. [Low-level C++ API — Hermes.hpp](#7-low-level-c-api)
8. [Serialization API — HermesSerialization.hpp](#8-serialization-api)
9. [Configuration service](#9-configuration-service)
10. [Vertical interface](#10-vertical-interface)
11. [Complete examples](#11-complete-examples)
12. [Bugs fixed in this version](#12-bugs-fixed)

---

## 1. What is Hermes

Hermes is an open TCP/IP + XML protocol that connects machines in an electronics assembly line. Each machine has an **Upstream** port (faces the previous machine) and a **Downstream** port (faces the next machine). PCB boards flow from Upstream to Downstream along the lane.

```
[Machine A] --downstream:50100--> [Machine B] --downstream:50101--> [Machine C]
             <--upstream:50100---              <--upstream:50101---
```

The Downstream machine **listens** on a port. The Upstream machine **connects** to it. So:

- `Hermes::Downstream` / `Modern::Downstream` — **server role**, listens for incoming connections
- `Hermes::Upstream` / `Modern::Upstream` — **client role**, connects to the downstream machine

---

## 2. Architecture overview

```
Your application
      |
      |--- HermesModern.hpp     (std::function callbacks — recommended)
      |--- Hermes.hpp           (virtual interface callbacks — advanced)
      |--- HermesSerialization  (XML serialize/deserialize)
      |
      +---> Hermes C API (Hermes.h / HermesData.h)
      |         compiled into libhermes.so
      |
      +---> Boost.Asio (networking)
      +---> pugixml (XML parsing)
```

### Message flow — horizontal (machine to machine)

```
Downstream machine                    Upstream machine
(Modern::Downstream)                  (Modern::Upstream)
        |                                     |
        |<-- TCP connect -------------------- |
        |                                     |
        |<-- ServiceDescription ------------- |  (Upstream identifies itself)
        |--> ServiceDescription -----------> |  (Downstream identifies itself)
        |                                     |
        |<-- MachineReady ------------------- |  (Upstream ready to receive)
        |--> BoardAvailable --------------> |  (Downstream has a board)
        |                                     |
        |<-- StartTransport ----------------- |  (Upstream says: send it)
        |  [board physically moves]           |
        |--> TransportFinished -----------> |  (Downstream confirms done)
        |                                     |
        |<-- StopTransport  ---------------- |  (Upstream confirms received)
```

### Who receives what

| You create        | You listen for (receive)                                          | You send                                              |
|-------------------|-------------------------------------------------------------------|-------------------------------------------------------|
| `Modern::Downstream` | `ServiceDescription`, `MachineReady`, `RevokeMachineReady`, `StartTransport`, `StopTransport`, `QueryBoardInfo` | `ServiceDescription`, `BoardAvailable`, `RevokeBoardAvailable`, `TransportFinished`, `BoardForecast`, `SendBoardInfo` |
| `Modern::Upstream`   | `ServiceDescription`, `BoardAvailable`, `RevokeBoardAvailable`, `TransportFinished`, `BoardForecast`, `SendBoardInfo` | `ServiceDescription`, `MachineReady`, `RevokeMachineReady`, `StartTransport`, `StopTransport`, `QueryBoardInfo` |

---

## 3. Building the library

### Prerequisites

```bash
# Debian / Ubuntu / Raspberry Pi OS
sudo apt install -y g++ cmake libboost-all-dev

# macOS
brew install cmake boost

# Windows
# Install Boost via vcpkg: vcpkg install boost
```

### Build

```bash
git clone https://github.com/hermes-org/lib_cpp.git
cd lib_cpp
mkdir build && cd build
cmake ..
make -j4
```

This produces `build/src/Hermes/libhermes.so` (Linux/macOS) or `hermes.dll` (Windows).

### Compile your application

```bash
g++ -std=c++17 -o myapp myapp.cpp \
    -I./src/include \
    -L./build/src/Hermes -lhermes \
    -lboost_system -lpthread
#change the commands according to your OS

# Run (Linux — tell the linker where to find the .so)
LD_LIBRARY_PATH=./build/src/Hermes ./myapp
```

---

## 4. Header map

| Header | What it gives you | When to include it |
|--------|------------------|--------------------|
| `HermesModern.hpp` | `Modern::Downstream`, `Modern::Upstream` with std::function callbacks | **Start here. Recommended for all new code.** |
| `Hermes.hpp` | `Hermes::Downstream`, `Hermes::Upstream`, virtual callback interfaces | When you need session IDs, raw XML, or fine-grained control |
| `HermesData.hpp` | All C++ data structs, enums, settings structs | Included automatically by the above |
| `HermesSerialization.hpp` | `ToXml()`, `FromXml<T>()` | When you need to inspect or log raw XML messages |
| `HermesOptional.hpp` | `Hermes::Optional<T>` = `std::optional<T>` | Included automatically |
| `HermesStringView.hpp` | `Hermes::StringView` = `std::string_view` | Included automatically |

---