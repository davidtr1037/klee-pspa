add_klee_unit_test(SymbolicPTATest
  SymbolicPTATest.cpp)

add_klee_unit_test(OffsetFinderTest
  OffsetFinderTest.cpp)

target_link_libraries(SymbolicPTATest PUBLIC kleeBasic kleaverSolver kleaverExpr kleeCore kleeAnalysis)

target_include_directories(SymbolicPTATest BEFORE PUBLIC "../../lib")

target_link_libraries(OffsetFinderTest PUBLIC kleeAnalysis kleeCore)
target_include_directories(OffsetFinderTest BEFORE PUBLIC "../../lib")
