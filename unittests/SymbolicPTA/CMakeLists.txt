add_klee_unit_test(SymbolicPTATest
  SymbolicPTATest.cpp)
target_link_libraries(SymbolicPTATest PUBLIC kleeBasic kleaverSolver kleaverExpr kleeCore kleeAnalysis)

target_include_directories(SymbolicPTATest BEFORE PUBLIC "../../lib")
