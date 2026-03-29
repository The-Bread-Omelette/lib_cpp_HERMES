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
# Building the Hermes C++ Library

The Hermes library uses **CMake** as its universal build system. The library has been modernized to use **Header-Only Boost** for its core networking, making it incredibly lightweight and easy to compile natively on Windows, Linux, and macOS without complex binary linking.

---

## 1. Prerequisites

Before building, ensure your environment has the following:

* **C++17 Compiler**: GCC 7+ / Clang 5+ (Linux) or MSVC 2017+ / MinGW-w64 (Windows).
* **CMake**: Version 3.15 or higher.
* **Boost Libraries**: Version 1.66 to 1.78.
* **pugixml**: XML parsing library.

---

### 2. Environment Setup

### Linux (Ubuntu / Debian)
On Linux, dependencies are easily managed via the system package manager. Install the standard build essentials, Boost, and pugixml directly:

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y g++ cmake libboost-all-dev libpugixml-dev git
```
### Windows
On Windows, dependencies are managed manually.

- Boost (1.66 - 1.78): Download the Boost source or precompiled headers. Extract the contents into a local References/ directory within your project root (e.g., References/boost).

- pugixml: Download the pugixml source and place it in the References/pugixml/ directory.

(CMake will be configured to point to these local reference folders in the next steps).

### 3. Standard Build Process
The CMake workflow is generally identical regardless of your operating system. Open a terminal in the root of the cloned repository.

#### Step 1: Create a Build Directory
Always perform an "out-of-source" build to keep your source tree clean.

```Bash
mkdir build
cd build
```

#### Step 2: Configure the Project

For windows run this command to set MSYS2 binaries into the path for this session
```Bash
set PATH=C:\msys64\ucrt64\bin;%PATH%
```

Now, Run CMake to generate the build files.

```Bash
cmake ..
cmake --build . --config Release --parallel 4
```
(You can remove parallel flag or adjust the number based on your number of cores. I was working on RPi so i used all 4 cores)

#### Build Outputs
Once the build completes successfully, the compiled binaries will be located in the build/src/Hermes/ directory:

- Linux: libhermes.so

- Windows: hermes.dll / hermes.lib

- macOS: libhermes.dylib

### Step 3: Building and Running the Tests
The repository includes a test suite (BoostTestHermes) to verify protocol compliance.

Note: While the core library is header-only, building the tests requires the compiled boost_unit_test_framework binary.

To run the tests from inside your build directory:

```Bash
ctest --output-on-failure -C Release
```
Alternatively, you can run the executable directly:

Windows: `.\test\BoostTestHermes\Release\BoostTestHermes.exe`

Linux: `./test/BoostTestHermes/BoostTestHermes`

## Step 4: Compiling Your Application

To compile a standalone C++ application that links against the Hermes library, you must point your compiler to the Hermes include directory and the folder containing your newly built binaries. 

Replace `<path-to-hermes>` with the actual path to your cloned repository.

**For Linux:**
```bash
g++ -std=c++17 -o my_app my_app.cpp \
    -I/<path-to-hermes>/src/include \
    -L/<path-to-hermes>/build/src/Hermes -lhermes \
    -lboost_system -lpthread
```
**For Windows:**
```bash
g++ -std=c++17 -o my_app.exe my_app.cpp ^
    -IC:\<path-to-hermes>\src\include ^
    -LC:\<path-to-hermes>\build\src\Hermes -lhermes ^
    -lws2_32 -lmswsock -liphlpapi
```

#### Running the compiled application:

- Linux: If you receive a "shared object file not found" error, ensure the linker knows where libhermes.so is located:

```Bash
LD_LIBRARY_PATH=/<path-to-hermes>/build/src/Hermes ./my_app 
```

- Windows: Ensure that hermes.dll is either in the same directory as my_app.exe, or that its folder is added to your system's PATH.

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