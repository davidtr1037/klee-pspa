#include "klee/Internal/ADT/TreeStream.h"
#include "klee/ExecutionState.h"
#include "klee/Solver.h"
#include "klee/util/ArrayCache.h"

#include "Core/TimingSolver.h"
#include "Core/Memory.h"
#include "Analysis/SymbolicPTA.h"

#include <vector>
#include <cstring>
#include <algorithm>

#include "gtest/gtest.h"
#define IN_CONTAINER(c,item) (std::find(c.begin(), c.end(), item) != c.end())

using namespace klee;
ArrayCache ac;

ObjectState* initializeObject(MemoryObject &mo, ExecutionState &state, int size = 8, std::string name = "unnamed") {
    mo.refCount = 2; //to prevent delition
    mo.size = size;
    mo.name = name; 

    ObjectState *os = new ObjectState(&mo);
    state.addressSpace.bindObject(&mo, os);
    return os;
}

TEST(SymbolicPTATest, BasicTarget) {
    Context::initialize(true, 64);
    ExecutionState state;
    klee::ref<Expr> zero = ConstantExpr::create(0, 64);
    const Array *array = ac.CreateArray("arrSym", 1);
    klee::ref<Expr> sym = Expr::createTempRead(array, 8);

    Solver *core = klee::createCoreSolver(CoreSolverToUse);
    TimingSolver* solver = new TimingSolver(core);

    MemoryObject pointerMo((uint64_t)malloc(8));
    MemoryObject pointeeMo((uint64_t)malloc(8));

    ObjectState* pointerOs = initializeObject(pointerMo, state, 8, "pointerMo");
    ObjectState* pointeeOs = initializeObject(pointeeMo, state, 8, "pointeeMo");
    pointerOs->write64(0, pointeeMo.address);
    pointeeOs->write(0, 
        SelectExpr::create(
            EqExpr::create(sym, ConstantExpr::create(0,8)), 
              ConstantExpr::create(pointerMo.address, Context::get().getPointerWidth()),
              ConstantExpr::create(pointeeMo.address, Context::get().getPointerWidth())
            ));
              
    SymbolicPTA sPTA(*solver, state);

    Pointer* ptr = sPTA.getPointer(&pointerMo, zero);
    Pointer* ptr1 = sPTA.getPointer(&pointerMo, zero);
    Pointer* pointeePtr = sPTA.getPointer(&pointeeMo, zero);
    ASSERT_EQ(ptr, ptr1);
    ASSERT_NE(ptr, pointeePtr);

    std::vector<Pointer*> ptrs = sPTA.getPointerTarget(*ptr);
    ASSERT_EQ(ptrs.size(), (uint64_t)1);
    ASSERT_EQ(ptrs[0], pointeePtr);

    std::vector<Pointer*> ptrss = sPTA.getPointerTarget(*pointeePtr);
    ASSERT_EQ(ptrss.size(), (uint64_t)2);
    ASSERT_TRUE(IN_CONTAINER(ptrss, pointeePtr));
    ASSERT_TRUE(IN_CONTAINER(ptrss, ptr));
}

