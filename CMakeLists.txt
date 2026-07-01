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

add_library(zinc STATIC ${ZINC_SOURCE})
include_directories("include")

option(ENABLE_SAN "Enable sanitizers")

if(ENABLE_SAN)
    set(SANITIZERS_FLAGS "-fsanitize=address -fsanitize=undefined -fno-sanitize-recover=all -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-sanitize=null -fno-sanitize=alignment")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZERS_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZERS_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZERS_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${SANITIZERS_FLAGS}")
endif()

enable_testing()

add_subdirectory(external/googletest)

add_subdirectory(test)

option(ENABLE_EXAMPLE "Enable build of examples" ON)

if (ENABLE_EXAMPLE)
    add_subdirectory(example)
endif()


