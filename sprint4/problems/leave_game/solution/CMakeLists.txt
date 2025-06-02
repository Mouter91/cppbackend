cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

get_property(importTargets DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY IMPORTED_TARGETS)

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Создаем статическую библиотеку с игровой моделью
add_library(game_model STATIC
    src/tagged.h
    src/boost_json.cpp
    src/log.h
    src/log.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/move_info.h
    src/collision_detector.h
    src/collision_detector.cpp
    src/geom.h
)

# Основной исполняемый файл
add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
    src/json_loader.h
    src/json_loader.cpp
    src/request_handler.cpp
    src/request_handler.h
    src/request_logger.h
    src/command_line.h
    src/command_line.cpp
    src/ticker.h
    src/ticker.cpp
    src/serialization.h
    src/serialization.cpp
    src/application.h
    src/application.cpp
    src/database.h
    src/database.cpp
    src/connection_pool.h
    src/model.h
    src/model.cpp
)

# Линкуем библиотеку game_model с необходимыми зависимостями
target_link_libraries(game_model PRIVATE CONAN_PKG::boost)

# Линкуем основной исполняемый файл
target_link_libraries(game_server PRIVATE 
    Threads::Threads 
    CONAN_PKG::boost
    CONAN_PKG::libpqxx
    game_model
)

# Исполняемый файл для тестов
add_executable(game_server_tests
    tests/loot_generator_tests.cpp
    tests/collision-detector-tests.cpp
)

# Настройка тестов
target_include_directories(game_server_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src  # Для доступа к заголовочным файлам
)

target_link_libraries(game_server_tests PRIVATE 
    CONAN_PKG::catch2 
    CONAN_PKG::boost
    game_model  # Используем нашу библиотеку модели
)

# Регистрация тестов для CTest
include(CTest)
enable_testing()
add_test(NAME game_server_tests COMMAND game_server_tests)
