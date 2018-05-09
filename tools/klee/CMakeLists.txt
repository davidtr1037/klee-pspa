#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
add_executable(klee
  main.cpp
)

set(KLEE_LIBS
  kleeCore
)

find_library(SVF_LIB NAMES LLVMSvf HINTS ${SVF_PATH}/build/lib)
find_library(CUDD_LIB NAMES LLVMCudd HINTS ${SVF_PATH}/build/lib/CUDD)

target_link_libraries(klee ${SVF_LIB} ${CUDD_LIB} ${KLEE_LIBS})

install(TARGETS klee RUNTIME DESTINATION bin)

# The KLEE binary depends on the runtimes
add_dependencies(klee BuildKLEERuntimes)
