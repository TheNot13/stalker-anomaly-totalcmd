cmake_minimum_required(VERSION 3.15)
project(AnomalyDbExplorer VERSION 1.0.0 LANGUAGES CXX C)

# Скачиваем библиотеку LZ4
file(DOWNLOAD "https://raw.githubusercontent.com/lz4/lz4/v1.9.4/lib/lz4.h" "${CMAKE_BINARY_DIR}/lz4.h")
file(DOWNLOAD "https://raw.githubusercontent.com/lz4/lz4/v1.9.4/lib/lz4.c" "${CMAKE_BINARY_DIR}/lz4.c")

# Собираем плагин вместе с LZ4
add_library(anomaly_db SHARED src/main.cpp ${CMAKE_BINARY_DIR}/lz4.c)

target_include_directories(anomaly_db PRIVATE ${CMAKE_BINARY_DIR})

set_target_properties(anomaly_db PROPERTIES PREFIX "")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(anomaly_db PROPERTIES SUFFIX ".wfx64")
else()
    set_target_properties(anomaly_db PROPERTIES SUFFIX ".wfx")
endif()
