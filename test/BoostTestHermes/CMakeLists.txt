# ==============================================================================
# test/BoostTestHermes/CMakeLists.txt
# ==============================================================================

# The test suite specifically requires the Boost Unit Test Framework
find_package(Boost 1.66 REQUIRED COMPONENTS unit_test_framework)

# Gather all C++ test files
file(GLOB TEST_SOURCES "*.cpp")

# Create the test executable
add_executable(BoostTestHermes ${TEST_SOURCES})

# Include local references for tests if they need direct access to pugixml or Boost headers
target_include_directories(BoostTestHermes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../References)

# Link the test executable against our newly built hermes library AND Boost Test
target_link_libraries(BoostTestHermes 
    PRIVATE 
        hermes 
        Boost::unit_test_framework
)

# Register the executable with CTest so the `ctest` command works automatically
add_test(NAME BoostTestHermes COMMAND BoostTestHermes)