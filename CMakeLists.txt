# ==============================================================================
# Root CMakeLists.txt (Universal Cross-Platform)
# ==============================================================================
cmake_minimum_required(VERSION 3.15)
project(HermesLib VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Find Cross-Platform Threading (Resolves to pthread on Linux/Mac, native on Win)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# 2. Find Boost HEADERS ONLY (No 'COMPONENTS system thread' required!)
if(WIN32)
    # Adjust the folder name if your boost version is different (e.g., boost_1_78_0)
    set(BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/References/boost_1_78_0" CACHE PATH "Path to local Boost")
endif()

find_package(Boost 1.66 REQUIRED)

add_subdirectory(src/Hermes)
