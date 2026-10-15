cmake_minimum_required(VERSION 3.15)
project(AnomalyExplorer VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Скачиваем minilzo для распаковки файлов
include(FetchContent)
FetchContent_Declare(
  minilzo
  URL https://www.oberhumer.com/opensource/lzo/download/minilzo-2.10.tar.gz
)
FetchContent_MakeAvailable(minilzo)

# 1. Плагин для Total Commander (WFX)
add_library(anomaly_db SHARED src/main.cpp ${minilzo_SOURCE_DIR}/minilzo.c)
target_include_directories(anomaly_db PRIVATE ${minilzo_SOURCE_DIR} src)
set_target_properties(anomaly_db PROPERTIES PREFIX "" SUFFIX ".wfx64")

# 2. Плагин для 7-Zip (Formats DLL)
add_library(anomaly_7z SHARED src/7z_main.cpp ${minilzo_SOURCE_DIR}/minilzo.c)
target_include_directories(anomaly_7z PRIVATE ${minilzo_SOURCE_DIR} src)
target_link_libraries(anomaly_7z PRIVATE ole32 oleaut32)
set_target_properties(anomaly_7z PROPERTIES PREFIX "" SUFFIX ".dll")
