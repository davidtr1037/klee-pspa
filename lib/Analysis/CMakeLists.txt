#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#

klee_add_component(kleeAnalysis
    PTAStats.cpp
    PTAUtils.cpp
    Reachability.cpp
)

# TODO: Work out what the correct LLVM components are for kleeCore.
set(LLVM_COMPONENTS
  core
  support
)

if ("${LLVM_PACKAGE_VERSION}" VERSION_EQUAL "3.6" OR
    "${LLVM_PACKAGE_VERSION}" VERSION_GREATER "3.6")
  list(APPEND LLVM_COMPONENTS mcjit executionengine native)
else()
  list(APPEND LLVM_COMPONENTS jit engine)
endif()

find_library(SVF_LIB NAMES LLVMSvf HINTS ${SVF_PATH}/build/lib)
find_library(CUDD_LIB NAMES LLVMCudd HINTS ${SVF_PATH}/build/lib/CUDD)

klee_get_llvm_libs(LLVM_LIBS ${LLVM_COMPONENTS})
target_link_libraries(kleeAnalysis PUBLIC
    ${SVF_LIB}
    ${CUDD_LIB}
)
target_include_directories(kleeAnalysis PUBLIC ${SVF_PATH}/include)
