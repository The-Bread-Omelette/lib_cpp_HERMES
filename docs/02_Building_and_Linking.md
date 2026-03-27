# 02. Building and Linking

The Hermes library uses **CMake (3.15+)** as its universal build system, ensuring seamless, cross-platform compilation on Windows, Linux, and macOS.

---

## 1. System Requirements

Before you begin, verify your environment has the following:
* **C++17 Compiler:** MSVC 2017+, GCC 7+, or Clang 5+.
* **Boost Libraries:** Version 1.66 through 1.78 is recommended. The library specifically requires `boost_system` and `boost_thread`.
  * *Note on Boost 1.87+: If using a newer version of Boost where `io_service` is deprecated, our CMake setup automatically applies the `-DBOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT` flag to ensure it still compiles.*
* **pugixml:** No installation required. The required files are bundled locally in the `References/` folder.

---

## 2. Compiling the Library

Open a terminal in the root of the cloned repository and execute a standard out-of-source build:

```bash
mkdir build && cd build

# 1. Configure the project (CMake locates Boost and your compiler)
cmake ..

# 2. Compile the core library
cmake --build . --config Release
Build Outputs:

Windows: build/Release/hermes.dll and hermes.lib

Linux: build/libhermes.so

3. Linking Hermes to Your Project
The cleanest way to integrate Hermes into your own C++ application is by using CMake's add_subdirectory command.

Assuming you cloned this repository into a folder named third_party/lib_cpp inside your project, your application's CMakeLists.txt would look like this:

CMake
cmake_minimum_required(VERSION 3.15)
project(MySmtMachine)

# 1. Include the Hermes library directory
add_subdirectory(third_party/lib_cpp)

# 2. Define your application executable
add_executable(MyApp main.cpp)

# 3. Link Hermes to your application
# This automatically configures the include paths for Hermes.hpp
target_link_libraries(MyApp PRIVATE hermes)