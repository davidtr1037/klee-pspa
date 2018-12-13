Static Analysis Framework for Symbolic Execution
=============================
A static analysis framework for the KLEE symbolic virtual machine.

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
### Target Functions
The target functions are set using the following option:
```
-pta-target=<function1>[:line1/line2/...],<function2>[:line1/line2/...],...
```
### Update Mode
Use the following option to enforce strong points-tp updates (on by default):
```
-use-strong-updates
```

### Allocation Sites
Use the following option to create a unique allocation site (name)
for each dynamic allocation (on by default):
```
-create-unique-as
```

### Standard Static Analysis
Use the following option to perfrom standard static analysis (instead of dynamic):
```
-run-static-pta
```

### Debugging
Use the following option to dump basic pointer analysis statistics:
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

## Statistics
Some basic statistics can be dumped to a `.csv` log using the following command:
```
gen_pta_stats.py <command> <functions> <csv_file>
```
The _command_ file should contain the invocation of the application:
```
<bc_file> <arg1> <arg2> ...
```
The _functions_ file contains a list of target functions:
```
<f1>[:<line1>]
<f2>[:<line2>]
...
```
