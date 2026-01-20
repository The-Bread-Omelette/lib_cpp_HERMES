# Building Hermes C++ Library

Comprehensive build instructions for all supported platforms.

## Table of Contents

- [System Requirements](#system-requirements)
- [Dependencies](#dependencies)
- [Windows Build](#windows-build)
- [MinGW/MSYS2](#mingwmsys2)
- [Visual Studio](#visual-studio)
- [Linux Build](#linux-build)
- [Test Suite](#test-suite)
- [Troubleshooting](#troubleshooting)

## System Requirements

- **C++17 compatible compiler**
- **500 MB disk space** for source + dependencies

## Dependencies

### Required Dependencies

#### Boost (1.66-1.78)

**⚠️ IMPORTANT**: Hermes uses Boost ASIO with `io_service`, which was removed in Boost 1.87+. You MUST use Boost 1.66-1.78.

**Windows (MinGW):**
```batch
# Download Boost 1.78:
https://archives.boost.io/release/1.78.0/source/boost_1_78_0.zip

# Extract and copy boost/ folder to:
References\boost\
```

**Linux:**
```bash
# Ubuntu/Debian
sudo apt-get install libboost-dev libboost-system-dev libboost-thread-dev

# Fedora/RHEL
sudo dnf install boost-devel

# Arch
sudo pacman -S boost

```

#### Pugixml

**Windows (MinGW):**
```batch
# Download from:
https://github.com/zeux/pugixml/releases/latest

# Extract and copy src/*.cpp and src/*.hppto:
References\pugixml\
```

**Linux:**
```bash
# Ubuntu/Debian
sudo apt-get install libpugixml-dev

# Fedora/RHEL
sudo dnf install pugixml-devel

# Arch
sudo pacman -S pugixml

```

## Windows Build

### MinGW/MSYS2

#### 1. Install MSYS2

Download and install from: https://www.msys2.org/

#### 2. Install Build Tools

Open MSYS2 MinGW 64-bit terminal:
```bash
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make
```

#### 3. Set Up Dependencies

Follow the [Dependencies](#dependencies) section to install Boost and Pugixml.

Directory structure should be:
```
lib_cpp/
├── References/
│   ├── boost/
│   │   ├── asio.hpp
│   │   ├── system/
│   │   └── ... (all boost headers)
│   └── pugixml/
│       ├── pugixml.hpp
│       ├── pugiconfig.hpp
│       └── pugixml.cpp
```

#### 4. Build
```batch
# Run the build script:
setup_windows.bat

# Or build manually:
cd src\Hermes
g++ -std=c++17 -O2 -Wall -DHERMES_EXPORTS ^
    -I..\include -I..\..\References -I..\..\References\pugixml ^
    -c *.cpp

g++ -shared -o Hermes.dll *.o Hermes.def ^
    -lws2_32 -lmswsock -liphlpapi ^
    -static-libgcc -static-libstdc++
```

### Visual Studio

#### 1. Install Visual Studio

- Visual Studio
- MSVC v143 C++ Build Tools
- C++ Desktop Development workload

#### 2. Set Up Dependencies

Create directory structure:
```
lib_cpp/
├── References/
│   ├── boost/           # Boost headers
│   ├── pugixml/         # Pugixml headers and source
│   ├── lib32/           # 32-bit Boost libraries (optional)
│   └── lib64/           # 64-bit Boost libraries (optional)
```

#### 3. Build

1. Open `Hermes.sln` in Visual Studio
2. Select configuration (Debug/Release)
3. Select platform (Win32/x64)
4. Build Solution (Ctrl+Shift+B)

Output will be in:
```
bin\$(Platform)\$(Configuration)\Hermes.dll
```


## Linux Build

### 1. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install build-essential g++ libboost-all-dev libpugixml-dev
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc-c++ boost-devel pugixml-devel
```

**Arch:**
```bash
sudo pacman -S base-devel boost pugixml
```

### 2. Build
```bash
# Run build script:
chmod +x setup.sh
./setup.sh

# Or build manually:
cd src/Hermes
g++ -fPIC -std=c++17 -O2 \
    -DBOOST_ERROR_CODE_HEADER_ONLY=0 \
    -DBOOST_SYSTEM_NO_DEPRECATED \
    -I../include \
    -c *.cpp

g++ -shared -Wl,-soname,libhermes.so.3 \
    -o libhermes.so.3.1.0 *.o \
    -lboost_system -lboost_thread -lpugixml -lpthread

ln -sf libhermes.so.3.1.0 libhermes.so
```

## Test Suite

**Simple Test:**

### Simple Test (No Boost.Test Required)

Create `simple_test.cpp`:
```cpp
#include <iostream>
#include "Hermes.h"

int main() {
    std::cout << "Testing Hermes library..." << std::endl;
    
    // Create minimal downstream
    HermesDownstreamCallbacks callbacks = {};
    HermesDownstream* p = CreateHermesDownstream(1, &callbacks);
    
    if (p) {
        std::cout << "SUCCESS: Library loaded and functional" << std::endl;
        DeleteHermesDownstream(p);
        return 0;
    }
    
    std::cerr << "FAILED: Could not create Hermes object" << std::endl;
    return 1;
}
``
no Boost compilation needed.
```batch
# Build and run simple test:
g++ -std=c++17 -I src/include -L . ^
    -o simple_test.exe simple_test.cpp ^
    -lHermes -lws2_32 -lmswsock ^
    -static-libgcc -static-libstdc++

simple_test.exe
```

You can use the test.cpp for testing every fucntion.


**Official Test Suite (Advanced):**
Download pre-compiled Boost libraries from:
https://sourceforge.net/projects/boost/files/boost-binaries/

Building Boost from source is not required.
Run Makefile in test\BoostTestHermes
