cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# Подключение Conan
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

# Потоки
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Статическая библиотека с игровой моделью
add_library(game_model STATIC
    src/model.h
    src/model.cpp
    src/tagged.h
    src/boost_json.cpp
    src/log.h
    src/log.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/move_info.h
)

target_include_directories(game_model PUBLIC src)
target_link_libraries(game_model PRIVATE CONAN_PKG::boost)

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
    src/ticker.h
    src/ticker.cpp
)

target_include_directories(game_server PRIVATE src)
target_link_libraries(game_server PRIVATE 
    Threads::Threads 
    CONAN_PKG::boost
    game_model
)

# Исполняемый файл для тестов
add_executable(game_server_tests
    tests/loot_generator_tests.cpp
)

target_include_directories(game_server_tests PRIVATE tests src)
target_link_libraries(game_server_tests PRIVATE 
    CONAN_PKG::catch2 
    CONAN_PKG::boost
    game_model
)

# Подключение системы тестирования
include(CTest)

# Поиск Catch.cmake, установленного Conan-ом
find_path(CATCH2_CMAKE_PATH Catch.cmake PATHS ${CMAKE_BINARY_DIR} PATH_SUFFIXES Catch2 share/cmake)
if (CATCH2_CMAKE_PATH)
    include(${CATCH2_CMAKE_PATH}/Catch.cmake)
    catch_discover_tests(game_server_tests)
else()
    message(WARNING "Catch.cmake not found. Автоматическое добавление тестов отключено.")
    # В качестве fallback — добавить все тесты одной командой:
    add_test(NAME all_tests COMMAND game_server_tests)
endif()

