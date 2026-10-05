cmake_minimum_required(VERSION 3.15)
project(AnomalyDbExplorer VERSION 1.0.0 LANGUAGES CXX C)

# Скачиваем minilzo автоматически при сборке
include(FetchContent)
FetchContent_Declare(
  minilzo
  URL https://www.oberhumer.com/opensource/lzo/download/minilzo-2.10.tar.gz
)
FetchContent_MakeAvailable(minilzo)

# Наш плагин
add_library(anomaly_db SHARED src/main.cpp)

# Подключаем скачанный LZO к плагину
target_include_directories(anomaly_db PRIVATE ${minilzo_SOURCE_DIR})
target_sources(anomaly_db PRIVATE ${minilzo_SOURCE_DIR}/minilzo.c)

# Убираем префикс "lib" и задаем расширения .wfx
set_target_properties(anomaly_db PROPERTIES PREFIX "")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(anomaly_db PROPERTIES SUFFIX ".wfx64")
else()
    set_target_properties(anomaly_db PROPERTIES SUFFIX ".wfx")
endif()
