# Hermes

"The Hermes Standard for vendor-independent machine-to-machine communication in SMT assembly" is a new, non-proprietary open protocol, based on TCP/IP- and XML, that takes exchange of PCB related data between the different machines in electronics assembly lines to the next level. It was initiated and developed by a group of leading equipment suppliers, bundling their expertise in order to achieve a great step towards advanced process integration. And the story continues: The Hermes Standard Initiative is open to all equipment vendors who want to actively participate in bringing the benefits of Industry 4.0 to their customers.
---

## Quick Start

### Prerequisites

**No Boost.Test library needed for using Hermes!** Only headers required.

| Component | Required For | Notes |
|-----------|--------------|-------|
| **Boost headers** (1.66-1.78) | Building library | Header-only, no compilation |
| **Pugixml** | Building library | Lightweight XML parser |
| **MinGW/GCC** | Compilation | g++ 7.0+ with C++17 support |
| Boost.Test library | Official test suite | **Optional** - for library developers|

### Installation

**Windows (MinGW):**
```batch
# Clone repository
git clone https://github.com/hermes-org/lib_cpp
cd lib_cpp

# Run setup (checks dependencies, builds library, runs tests)
setup_windows.bat
```

**Linux/macOS:**
```bash
# Clone repository
git clone https://github.com/hermes-org/lib_cpp
cd lib_cpp

# Run setup
chmod +x setup.sh
./setup.sh
```

### Build Output

- **Windows:** `Hermes.dll` + `Hermes.lib` (import library)
- **Linux:** `libhermes.so.3.1.0` (shared library)
- **Headers:** `src/include/Hermes.h` (main header)

---

## Using Hermes in Your Application

### 1. Minimal Example

```cpp
#include <Hermes.h>
#include <iostream>

int main() {
    // Create upstream connection (machine → line)
    Hermes::Upstream upstream;
    
    // Configure connection
    Hermes::UpstreamSettings settings;
    settings._laneId = 1;
    settings._hostAddress = "192.168.1.100";  // Downstream machine IP
    settings._port = 50101;
    
    // Connect to line
    upstream.Connect(1, settings);
    
    std::cout << "Connected to Hermes line" << std::endl;
    
    // Signal board available
    Hermes::BoardAvailableData board;
    board._boardId = "PCB-12345";
    board._topBarcode = "TOP-BARCODE-001";
    board._lengthInMM = 250.0;
    board._widthInMM = 150.0;
    
    upstream.Signal(1, board);
    
    // Cleanup
    upstream.Disconnect(1);
    return 0;
}
```

### 2. Compile Your Application

**Windows:**
```batch
g++ -std=c++17 -I src/include -L . -o my_app.exe my_app.cpp -lHermes -lws2_32 -lmswsock -static-libgcc -static-libstdc++
```

**Linux:**
```bash
g++ -std=c++17 -I src/include -L . -o my_app my_app.cpp -lhermes -lboost_system -lboost_thread -lpugixml -lpthread -Wl,-rpath,'$ORIGIN'
```

### 3. Deploy

Include with your application:
- **Windows:** `my_app.exe` + `Hermes.dll`
- **Linux:** `my_app` + `libhermes.so*` (set `LD_LIBRARY_PATH` or use rpath)

---

## Documentation

- **[Getting Started Guide](docs/GETTING_STARTED.md)** - Step-by-step tutorial
- **[API Reference](docs/API_REFERENCE.md)** - Complete API documentation
- **[Building from Source](docs/BUILDING.md)** - Advanced build instructions

---

## Architecture

### Communication Patterns

```
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│  Machine A  │────────▶│  Machine B  │────────▶│  Machine C  │
│  (Upstream) │  Board  │(Up+Down)    │  Board  │ (Downstream)│
└─────────────┘  Data   └─────────────┘  Data   └─────────────┘
      ▲                        ▲                        ▲
      │                        │                        │
      └────────────────────────┴────────────────────────┘
                         IPC-CFX (Vertical)
                    MES / ERP / Cloud Systems
```

### Message Types

- **Board handling:** BoardAvailable, BoardForecast, TransportFinished
- **Machine state:** MachineReady, NotificationData, CheckAlive
- **Work order:** GetWorkOrderData, SetWorkOrderData
- **Configuration:** SupervisoryServiceDescription, QueryBoardInfo

---

## Project Structure

```
hermes/
├── src/
│   ├── Hermes/              # Library source code
│   │   ├── Makefile         # Linux/macOS build
│   │   ├── Makefile.mingw   # Windows (MinGW) build
│   │   └── *.cpp            # Implementation files
│   └── include/             # Public headers
│       └── Hermes.h
├── test/
│   └── BoostTestHermes/     # Official test suite (optional)
│       ├── Makefile
│       └── Makefile.mingw
├── References/              # Dependencies (not in repo)
│   ├── boost/               # Boost headers
│   └── pugixml/             # Pugixml source
├── docs/                    # Documentation
├── setup_windows.bat        # Windows build script
├── setup.sh                 # Linux/macOS build script
└── README.md
```

---

## Dependencies Setup

### Boost (Headers Only)

**Download:** [Boost 1.78.0](https://archives.boost.io/release/1.78.0/source/boost_1_78_0.zip)

```
# Extract and copy:
boost_1_78_0/boost/  →  References/boost/
```

### Pugixml

**Download:** [Pugixml Latest](https://github.com/zeux/pugixml/releases)

```
# Copy source files:
pugixml-*/src/*.hpp  →  References/pugixml/
pugixml-*/src/*.cpp  →  References/pugixml/
```

---

## Testing

### Simple Test (Recommended)

```batch
# Creates basic connectivity test
g++ -std=c++17 -I src/include -L . -o simple_test.exe simple_test.cpp -lHermes -lws2_32 -lmswsock
simple_test.exe
```

### Official Test Suite (Optional)

Requires Boost.Test library. See [BUILDING.md](docs/BUILDING.md) for details.

```batch
cd test/BoostTestHermes
make -f Makefile.mingw
./BoostTestHermes.exe
```

---

## Compatibility

### Hermes Standard Versions

-  IPC-HERMES-9852 v1.0 - v1.5
-  SMEMA backward compatibility mode

### Platforms

| Platform | Compiler | Status |
|----------|----------|--------|
| Windows 10/11 | MinGW-w64 (GCC 7+) |  Tested |
| Windows 10/11 | MSVC 2019+ | ⚠️ Community contributions welcome |
| Linux (Ubuntu/Debian) | GCC 7+ |  Tested |
| Linux (Fedora/RHEL) | GCC 7+ |  Tested |

### Boost Version Compatibility

⚠️ **IMPORTANT:** Use Boost 1.66 - 1.78 only

- [NO] Boost 1.87+ breaks compatibility (removed `io_service`)
- [OK] Boost 1.78 recommended (stable, well-tested)

---

## IPC Standards Integration

### Hermes + CFX Together

The Hermes Standard drives horizontal integration along the SMT line. IPC-CFX complements this by providing a powerful standard for connecting vertically from the SMT line to an MES.

**Horizontal (Line):** IPC-HERMES-9852 (this library)  
**Vertical (MES/ERP):** IPC-2591 (CFX) - separate implementation

---

## Support & Contributing

- **Issues:** [GitHub Issues](../../issues)
- **Documentation:** [Wiki](../../wiki)
- **Official Standard:** [IPC-HERMES-9852](https://www.the-hermes-standard.info/)

### Contributing

Contributions welcome! Please:
1. Follow existing code style
2. Add tests for new features
3. Update documentation
4. Submit pull requests
