cmake_minimum_required(VERSION 3.16)
project(SysLab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  message(FATAL_ERROR "SysLab requires Clang or GCC")
endif()

include(CheckIncludeFileCXX)
set(CMAKE_REQUIRED_FLAGS "-std=c++20")
check_include_file_cxx(coroutine SYSLAB_HAS_STD_COROUTINE)
if(NOT SYSLAB_HAS_STD_COROUTINE)
  include_directories(BEFORE "${CMAKE_SOURCE_DIR}/third_party/polyfill/include")
  set(SYSLAB_POLYFILL_DIR "${CMAKE_SOURCE_DIR}/third_party/polyfill/include")
else()
  set(SYSLAB_POLYFILL_DIR "")
endif()

add_library(syslab
  src/core/error.cpp
  src/core/fs.cpp
  src/core/time.cpp
  src/core/sha256.cpp
  src/core/json.cpp
  src/core/yaml.cpp
  src/experiment/model.cpp
  src/experiment/manifest.cpp
  src/experiment/lifecycle.cpp
  src/experiment/registry.cpp
  src/experiment/session.cpp
  src/build/build_manager.cpp
  src/observation/elf.cpp
  src/observation/report.cpp
  src/platform/linux/environment.cpp
  src/platform/linux/capability.cpp
  src/platform/linux/toolchain.cpp
  src/platform/x86_64/cpu.cpp
  src/runner/process.cpp
  src/memory/virtual_memory.cpp
  src/memory/arena_allocator.cpp
  src/memory/pool_allocator.cpp
  src/memory/freelist_allocator.cpp
  src/memory/buddy_allocator.cpp
  src/memory/size_class_allocator.cpp
  src/memory/thread_cache_allocator.cpp
  src/atomic/hazard_pointer.cpp
  src/atomic/epoch.cpp
  src/concurrency/scheduler.cpp
  src/concurrency/work_stealing.cpp
  src/runtime/coroutine.cpp
)

target_include_directories(syslab
  PUBLIC "${CMAKE_SOURCE_DIR}/include"
  PUBLIC "${CMAKE_SOURCE_DIR}"
)
target_compile_options(syslab PRIVATE -Wall -Wextra -Wno-deprecated-declarations -pthread)
target_link_libraries(syslab PUBLIC pthread)

add_executable(syslab_cli src/cli/main.cpp src/cli/learn.cpp)
target_link_libraries(syslab_cli PRIVATE syslab)
target_compile_definitions(syslab_cli PRIVATE
  SYSLAB_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  SYSLAB_COMPILER="${CMAKE_CXX_COMPILER}"
  SYSLAB_POLYFILL_DIR="${SYSLAB_POLYFILL_DIR}"
)
set_target_properties(syslab_cli PROPERTIES OUTPUT_NAME syslab)

enable_testing()
add_executable(syslab_tests tests/unit/test_main.cpp)
target_link_libraries(syslab_tests PRIVATE syslab)
target_compile_options(syslab_tests PRIVATE -Wall -Wextra -Wno-deprecated-declarations)
add_test(NAME unit COMMAND syslab_tests)
add_test(NAME cpu001 COMMAND syslab_cli run CPU-001)
set_tests_properties(cpu001 PROPERTIES TIMEOUT 120)
