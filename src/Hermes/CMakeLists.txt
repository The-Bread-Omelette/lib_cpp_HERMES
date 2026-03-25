# ==============================================================================
# src/Hermes/CMakeLists.txt (Universal Cross-Platform)
# ==============================================================================
file(GLOB HERMES_SOURCES "*.cpp")

# Pugixml is compiled directly into the library
list(APPEND HERMES_SOURCES "../../References/pugixml/pugixml.cpp")

# Windows explicitly requires the .def file for DLL exports; Linux/Mac ignore it
if(WIN32)
    add_library(hermes SHARED ${HERMES_SOURCES} Hermes.def)
else()
    add_library(hermes SHARED ${HERMES_SOURCES})
endif()

# Public Includes (What users of the library need to see)
target_include_directories(hermes PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)

# Private Includes (Internal dependencies)
target_include_directories(hermes PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../References
    ${CMAKE_CURRENT_SOURCE_DIR}/../../References/pugixml
)

# Link the Boost Headers and Cross-Platform Threads
target_link_libraries(hermes PRIVATE Boost::boost Threads::Threads)

# Core Definitions
target_compile_definitions(hermes PRIVATE 
    HERMES_EXPORTS
    BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT
)

# OS-Specific Linker Flags
if(WIN32)
    # All Windows compilers need the socket libraries
    target_link_libraries(hermes PRIVATE ws2_32 mswsock iphlpapi)
    
    # ONLY MinGW (GCC on Windows) needs the static standard library flags
    if(MINGW)
        target_link_options(hermes PRIVATE "-static-libgcc" "-static-libstdc++")
    endif()
endif()