cmake_minimum_required(VERSION 3.15)
project(AnomalyDbExplorer VERSION 1.0.0 LANGUAGES CXX C)

# Включаем современный C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(anomaly_db SHARED src/main.cpp)

set_target_properties(anomaly_db PROPERTIES PREFIX "")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(anomaly_db PROPERTIES SUFFIX ".wfx64")
else()
    set_target_properties(anomaly_db PROPERTIES SUFFIX ".wfx")
endif()
