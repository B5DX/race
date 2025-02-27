cmake_minimum_required(VERSION 3.5)
project(avn_project)

set(LLVM_DIR "/home/yuanliang/.local/lib/cmake/llvm")
set(CLANG_DIR "/home/yuanliang/.local/lib/cmake/clang")
set(CMAKE_CXX_COMPILER "/home/yuanliang/.local/bin/clang++")

find_package(LLVM REQUIRED CONFIG HINTS ${LLVM_DIR} NO_DEFAULT_PATH)
message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION}")

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIR})

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

find_package(Clang REQUIRED CONFIG HINTS ${LLVM_DIR} ${CLANG_DIR} NO_DEFAULT_PATH)
include_directories(${CLANG_INCLUDE_DIRS})

#list(APPEND CMAKE_MODULE_PATH "${CLANG_CMAKE_DIR}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
add_definitions(${LLVM_DEFINITIONS})
if(NOT ${LLVM_ENABLE_RTTI})
  message(STATUS "Disabling RTTI")
  # For non-MSVC compilers
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# add -g to enable debug
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

include_directories(${PROJECT_SOURCE_DIR}/src
                    ${PROJECT_SOURCE_DIR}/src/**)
                    
file(GLOB SOURCE 
    ${PROJECT_SOURCE_DIR}/src/*.cpp 
    ${PROJECT_SOURCE_DIR}/src/**/*.cpp)

llvm_map_components_to_libnames(LLVM_LIBS
    x86asmparser
    x86codegen
    x86desc
    x86disassembler
    x86info
)

add_executable(race ${SOURCE})

set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )

target_link_libraries(race
  clangAST
  clangASTMatchers
  clangBasic
  clangLex
  clangFrontend
  clangSerialization
  clangTooling
  LLVMSupport
  LLVMCore
  # ${LLVM_LIBS}
  )

install(TARGETS race
  RUNTIME DESTINATION bin)
