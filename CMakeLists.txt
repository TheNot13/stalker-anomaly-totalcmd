cmake_minimum_required(VERSION 3.15)
project(AnomalyDbExplorer VERSION 1.0.0 LANGUAGES CXX)

# Создаем динамическую библиотеку (DLL) из нашего кода
add_library(anomaly_db SHARED src/main.cpp)

# Убираем префикс "lib" и задаем правильные расширения для Total Commander
set_target_properties(anomaly_db PROPERTIES PREFIX "")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(anomaly_db PROPERTIES SUFFIX ".wfx64")
else()
    set_target_properties(anomaly_db PROPERTIES SUFFIX ".wfx")
endif()
