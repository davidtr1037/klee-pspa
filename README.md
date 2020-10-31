Pointer Analysis Framework for Symbolic Execution
=============================
A pointer analysis framework for the KLEE symbolic virtual machine.

## Build
Build SVF fork:
* https://github.com/davidtr1037/SVF-dynamic

Build KLEE:
```
git checkout master
mkdir klee_build
cd klee_build
CXXFLAGS="-fno-rtti" cmake \
    -DENABLE_SOLVER_STP=ON \
    -DENABLE_POSIX_RUNTIME=ON \
    -DENABLE_KLEE_UCLIBC=ON \
    -DKLEE_UCLIBC_PATH=<UCLIBC_PATH>
    -DLLVM_CONFIG_BINARY=<LLVM_CONFIG> \
    -DLLVMCC=<LLVMCC> \
    -DLLVMCXX=<LLVMCXX> \
    -DENABLE_UNIT_TESTS=OFF \
    -DKLEE_RUNTIME_BUILD_TYPE=Release+Asserts \
    -DENABLE_SYSTEM_TESTS=ON \
    -DENABLE_TCMALLOC=ON \
    -DSVF_PATH=<SVF_ROOT_DIR> \
    <KLEE_ROOT_DIR>
make
```

## Options

### Pointer Analysis Modes
Use the following option to perform _static_ analysis:
```
-use-pta-mode=static
```

Use the following option to perform _past-sensitive_ analysis:
```
-use-pta-mode=symbolic
```

### Target Functions
The target functions to analyze are set using the following option:
```
-pta-target=<function1>[:line1/line2/...],<function2>[:line1/line2/...],...
```

### Allocation Sites
Use the following option to create a unique allocation sites for each dynamic allocation (_on_ by default):
```
-create-unique-as
```

### Debugging
Use the following option to dump basic points-to information:
```
-collect-pta-stats
```
Use the following option to dump detailed points-to information:
```
-collect-pta-results
```
Use the following option to dump the mod set for the target function:
```
-collect-modref
```
