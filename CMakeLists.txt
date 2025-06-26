cmake_minimum_required(VERSION 3.5)
project(phantomjs)

# C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Output binary to /bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find required packages
# IMPORTANT: Removed WebKitWidgets and added Gui, Widgets
find_package(Qt5 5.5 REQUIRED COMPONENTS Core Network Gui Widgets) # <--- UPDATED LINE 11
find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

message(STATUS "Using Qt version ${Qt5Core_VERSION}")

# Source files
file(GLOB_RECURSE SRC_FILES src/*.cpp)
file(GLOB_RECURSE HDR_FILES src/*.h)

# Optional: exclude test files
list(FILTER SRC_FILES EXCLUDE REGEX ".*/tests?/.*")

# Third-party sources
set(THIRDPARTY_SOURCES
    src/mongoose/mongoose.c
    src/qcommandline/qcommandline.cpp
    src/linenoise/src/linenoise.c
)

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/engines
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/linenoise/src
    ${PROJECT_SOURCE_DIR}/src/mongoose
    ${PROJECT_SOURCE_DIR}/src/qcommandline
)

# Platform-specific definitions and libs
if(WIN32)
    set(EXTRA_LIBS ws2_32)
    add_definitions(-DQCOMMANDLINE_STATIC)
else()
    set(EXTRA_LIBS dl)
endif()

# PhantomJS target
add_executable(${PROJECT_NAME}
    src/phantomjs.qrc
    ${SRC_FILES}
    ${THIRDPARTY_SOURCES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Network
    Qt5::Gui     # <--- ADDED Qt5::Gui
    Qt5::Widgets # <--- ADDED Qt5::Widgets
    # Qt5::WebKitWidgets # <--- REMOVED THIS LINE
    Threads::Threads
    ${EXTRA_LIBS}
)

# Install target
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Test target
add_custom_target(check
    COMMAND ${Python3_EXECUTABLE} test/run-tests.py -v
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running PhantomJS tests..."
)
