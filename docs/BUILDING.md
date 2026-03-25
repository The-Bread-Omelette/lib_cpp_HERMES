# Building the Hermes C++ Library

The Hermes library uses **CMake** as its universal build system. The library has been modernized to use **Header-Only Boost** for its core networking, making it incredibly lightweight and easy to compile natively on Windows, Linux, and macOS without complex binary linking.

---

## 1. Prerequisites

Before building, ensure your environment has the following installed:

* **C++17 Compiler**: 
    * **Linux**: GCC 7+ or Clang 5+
    * **Windows**: MSYS2/MinGW-w64 (GCC) or MSVC (Visual Studio 2017+)
    * **macOS**: Apple Clang
* **CMake**: Version 3.15 or higher.
* **Boost Libraries**: Version 1.66 or newer. *(Note: The core library only requires Boost headers. No compiled `.lib` or `.so` Boost binaries are required unless you are building the Test Suite).*
* **pugixml**: Included locally in the `References/` directory, so no external installation is required.

---

## 2. Environment Setup (From Scratch)

Depending on your operating system, follow the steps below to ensure you have all required build tools present before running CMake.

### 🐧 Linux (Ubuntu / Debian)
Linux is the easiest platform to build on. Simply install the standard build essentials and Boost headers from your package manager:
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y g++ cmake libboost-all-dev git
🪟 Windows (via MSYS2 / MinGW - Recommended)
To build a lightweight .dll using GCC on Windows, use MSYS2 (UCRT64 environment). Open your MSYS2 terminal and install the toolchain:

Bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-boost
Crucial: If you build from a standard Windows Command Prompt (cmd) instead of the MSYS2 terminal, you must temporarily add the MSYS2 binaries to your path before running CMake:

DOS
set PATH=C:\msys64\ucrt64\bin;%PATH%
🪟 Windows (via Visual Studio / MSVC)
If you prefer Microsoft Visual Studio, ensure the "Desktop development with C++" workload is installed. You will also need to download Boost headers (e.g., from boost.org) and extract them to a local directory (e.g., C:\local\boost_1_84_0).

3. Standard Build Process
The CMake workflow is generally identical regardless of your operating system. Open a terminal in the root of the cloned repository.

Step 1: Create a Build Directory
Always perform an "out-of-source" build to keep your source tree clean.

Bash
mkdir build
cd build
Step 2: Configure the Project
Run CMake to generate the build files.

For Linux / macOS:

Bash
cmake ..
make -j4
For Windows (MSYS2 / MinGW):

DOS
cmake .. -G "MinGW Makefiles"
For Windows (Visual Studio):

DOS
cmake .. -DBOOST_ROOT="C:/local/boost_1_84_0"
Step 3: Compile the Library
Execute the build command to compile the library.

Bash
cmake --build . --config Release
Build Outputs
Once the build completes successfully, the compiled binaries will be located in the build/src/Hermes/ directory:

Windows: hermes.dll

Linux: libhermes.so

macOS: libhermes.dylib

4. Building and Running the Tests
The repository includes a test suite (BoostTestHermes) to verify protocol compliance.

Note: While the core library is header-only, building the tests requires the compiled boost_unit_test_framework binary.

To run the tests from inside your build directory:

Bash
ctest --output-on-failure -C Release
Alternatively, you can run the executable directly:

Windows: .\test\BoostTestHermes\Release\BoostTestHermes.exe

Linux: ./test/BoostTestHermes/BoostTestHermes

To compile any file run
g++ -std=c++17 -o example.exe example.cpp \
    -I~/lib_cpp/src/include \
    -L~/lib_cpp/build -lhermes \
    -lboost_system -lpthread