cmake_minimum_required(VERSION 3.23)

project(zinc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-fcoroutines)

# ///

set(ZINC_SOURCE core/async/task.cpp
core/async/thread_pool.cpp)

add_library(zinc STATIC ${ZINC_SOURCE})

add_executable(test_exec)
target_sources(test_exec PRIVATE main.cpp)
target_link_libraries(test_exec zinc)


