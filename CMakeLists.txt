cmake_minimum_required(VERSION 3.16)
project(RACE LANGUAGES C CXX)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
find_package(Boost REQUIRED)
option(RACE_DEBUG "Enable detailed extraction diagnostics" OFF)

add_executable(race
  src/main.cpp
  src/Frontend/Scanner.cpp src/Frontend/PragmaHandler.cpp
  src/IR/IR.cpp src/IR/ExprTree.cpp
  src/Solver/Extractor.cpp src/Solver/IVInfo.cpp src/Solver/MatchSolver.cpp)
target_compile_features(race PRIVATE cxx_std_17)
target_include_directories(race PRIVATE src)
target_include_directories(race SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(race PRIVATE -fno-rtti)
endif()
if(RACE_DEBUG)
  target_compile_definitions(race PRIVATE RACE_DEBUG)
endif()
# Discover the headers belonging to this LLVM installation, rather than a user's path.
find_program(RACE_CLANG NAMES clang HINTS "${LLVM_TOOLS_BINARY_DIR}" NO_DEFAULT_PATH)
if(RACE_CLANG)
  execute_process(COMMAND "${RACE_CLANG}" -print-resource-dir
    OUTPUT_VARIABLE RACE_RESOURCE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE RACE_RESOURCE_STATUS)
  if(RACE_RESOURCE_STATUS EQUAL 0)
    target_compile_definitions(race PRIVATE RACE_RESOURCE_DIR="${RACE_RESOURCE_DIR}")
  endif()
endif()
target_link_libraries(race PRIVATE clangTooling clangASTMatchers clangFrontend clangSerialization clangRewrite clangAST clangLex clangBasic LLVMCore LLVMSupport LLVMFrontendOpenMP)
install(TARGETS race RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND AND RACE_CLANG)
    if(RACE_DEBUG)
      set(RACE_TEST_ARGS --debug-build)
    endif()
    add_test(NAME regression COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/regression.py" $<TARGET_FILE:race> "${RACE_CLANG}" ${RACE_TEST_ARGS})
  endif()
endif()
