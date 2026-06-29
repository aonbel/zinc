cmake_minimum_required(VERSION 3.23)

project(zinc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ZINC_SOURCE 
src/core/async/task.cpp
src/core/async/thread_pool.cpp
src/core/node/node_manager.cpp
)

set(ZINC_HEADER 
include/core/async/awaiters/duration.hpp
include/core/async/task.hpp
include/core/async/thread_pool.hpp
include/core/async/thread_pool_context.hpp
include/core/node/node.hpp
include/core/node/node_manager.hpp
include/core/utility/meta.hpp
include/core/defaults.hpp

include/nodes/epoll_node/epoll_node.hpp
)

include_directories("include")

add_library(zinc STATIC ${ZINC_SOURCE})

enable_testing()

add_subdirectory(external/googletest)

option(ENABLE_TSAN "Enable ThreadSanitizer" ON)

if (ENABLE_TSAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
endif()

add_subdirectory(test)


