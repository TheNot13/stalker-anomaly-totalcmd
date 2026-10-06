cmake_minimum_required(VERSION 3.15)
project(AnomalyDbExplorer VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Скачиваем minilzo для распаковки файлов (.ltx, .dds и т.д.)
include(FetchContent)
FetchContent_Declare(
  minilzo
  URL https://www.oberhumer.com/opensource/lzo/download/minilzo-2.10.tar.gz
)
FetchContent_MakeAvailable(minilzo)

add_library(anomaly_db SHARED src/main.cpp)

target_include_directories(anomaly_db PRIVATE ${minilzo_SOURCE_DIR})
target_sources(anomaly_db PRIVATE ${minilzo_SOURCE_DIR}/minilzo.c)

set_target_properties(anomaly_db PROPERTIES PREFIX "")
set_target_properties(anomaly_db PROPERTIES SUFFIX ".wfx64")
