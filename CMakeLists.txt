cmake_minimum_required(VERSION 3.0.0)

# Project configuration
project(FletTerminalValidator LANGUAGES C CXX)

# Define versioning variables
set(PROJECT_VERSION_MAJOR "1")
set(PROJECT_VERSION_MINOR "0")
set(PROJECT_VERSION_PATCH "0")
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

# Definition for version
string(TIMESTAMP BUILD_YEAR "%Y")
string(TIMESTAMP BUILD_MONTH "%m")
string(TIMESTAMP BUILD_DAY "%d")
set(FTV_MODULE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-${BUILD_YEAR}${BUILD_MONTH}${BUILD_DAY}")
add_definitions(-DFTV_MODULE_VERSION="${FTV_MODULE_VERSION}")

# Include necessary modules
include(FindPkgConfig)

# Set the default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Custom Definition
## Reader and Business
add_definitions(-D__USE_EMPTECH_READER)
add_definitions(-D__TRANSJAKARTA)
## Directory Tree
add_definitions(-D__DATA_DIRECTORY__="root.data/")

# Verbose compile option
option(VERBOSE "Enable verbose compile" OFF)
if(VERBOSE)
  set(CMAKE_VERBOSE_MAKEFILE ON)
endif()

# Specify the source files
set(SOURCE_FILES
  src/duration.cpp
  src/ui-helper.cpp
  src/controller.cpp
)

# Create object
add_library(${PROJECT_NAME}-obj OBJECT ${SOURCE_FILES})

# Create executable
add_executable(${PROJECT_NAME} main/main.cpp $<TARGET_OBJECTS:${PROJECT_NAME}-obj> $<TARGET_OBJECTS:tscdata-obj> $<TARGET_OBJECTS:utils-obj>)

# Include directories for the project
set(INCLUDE_DIRS
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dependency>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dependency/communication/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dependency/lzma/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dependency/sqlite3/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/internal>
  $<INSTALL_INTERFACE:include>
)
target_include_directories(${PROJECT_NAME}-obj PUBLIC ${INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS})
target_link_directories(${PROJECT_NAME} PUBLIC /work/AT91SAMA5/QT/qt5.6_target/lib)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/dependency/workflow/lib/libworkflow.so
    ${CMAKE_CURRENT_SOURCE_DIR}/dependency/epayment/lib/libepayment.so
    ${CMAKE_CURRENT_SOURCE_DIR}/dependency/bca/lib/libbcadllarmhf.so
    ${CMAKE_CURRENT_SOURCE_DIR}/dependency/epayment/lib/libsqlite3.a
    ${CMAKE_CURRENT_SOURCE_DIR}/dependency/reader/lib/libemptechreader.so
    ${CMAKE_CURRENT_SOURCE_DIR}/dependency/gui/lib/libgui.so
    ${CMAKE_CURRENT_SOURCE_DIR}/dependency/communication/lib/libcommunication.so
    ${CMAKE_CURRENT_SOURCE_DIR}/dependency/communication/lib/libcurl.so.4.6.0
    ${CMAKE_CURRENT_SOURCE_DIR}/dependency/lzma/lib/liblzma.a
)
target_link_libraries(${PROJECT_NAME}
  PUBLIC
    Qt5Widgets
    Qt5Gui
    Qt5Core
    crypto
    ssl
    pthread
    dl
)
if(ENABLE_BCA_PAYMENT)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/library/bca/libbcadllARM.so)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "/work/AT91SAMA5/QT/qt5.6_target/lib"
    SOVERSION 1
)

# Compiler and linker flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fPIC")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fPIC")

# Add sub project
add_subdirectory(external/utils EXCLUDE_FROM_ALL)
add_subdirectory(internal/tscdata EXCLUDE_FROM_ALL)

target_include_directories(tscdata-obj PUBLIC ${INCLUDE_DIRS})