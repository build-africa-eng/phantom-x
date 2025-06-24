cmake_minimum_required(VERSION 3.5.0)
project(phantomjs)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Qt
find_package(Qt5 COMPONENTS Core Network WebKitWidgets REQUIRED)
find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

message("Using Qt version ${Qt5Core_VERSION}")
if (Qt5Core_VERSION VERSION_LESS 5.5.0)
  message(FATAL_ERROR "This version of Qt is not supported. Please use Qt 5.5 or later")
endif()

# Sources
file(GLOB_RECURSE PHANTOMJS_SOURCES src/*.cpp)
# Optional: filter out test files
# list(FILTER PHANTOMJS_SOURCES EXCLUDE REGEX ".*test/.*")

# Third-party
set(THIRDPARTY_SOURCES
  src/mongoose/mongoose.c
  src/qcommandline/qcommandline.cpp
  src/linenoise/src/linenoise.c
)
include_directories(
  src
  src/linenoise/src
  src/mongoose
  src/qcommandline
)

# Platform-specific
if (WIN32)
  set(EXTRA_LIBS ws2_32)
  add_definitions(-DQCOMMANDLINE_STATIC)
else()
  set(EXTRA_LIBS dl)
endif()

# Target
add_executable(${PROJECT_NAME}
  src/phantomjs.qrc
  ${PHANTOMJS_SOURCES}
  ${THIRDPARTY_SOURCES}
)
target_link_libraries(${PROJECT_NAME}
  ${EXTRA_LIBS}
  Qt5::Core Qt5::Network Qt5::WebKitWidgets
  Threads::Threads
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Test target
add_custom_target(check
  COMMAND ${Python3_EXECUTABLE} test/run-tests.py -v
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Running PhantomJS tests..."
)
