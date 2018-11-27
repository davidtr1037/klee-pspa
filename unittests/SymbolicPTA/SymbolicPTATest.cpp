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
llvm::DataLayout dl("e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128");
TimingSolver* solver = nullptr;


ObjectState* initializeObject(MemoryObject &mo, ExecutionState &state, int size = 8, std::string name = "unnamed") {
    mo.refCount = 2; //to prevent delition
    mo.size = size;
    mo.name = name; 

    ObjectState *os = new ObjectState(&mo);
    state.addressSpace.bindObject(&mo, os);
    return os;
}

TEST(SymbolicPTATest, BasicTarget) {
    if(!Context::isInit())
      Context::initialize(true, 64);
    llvm::LLVMContext ctx;
    ExecutionState state;
    klee::ref<Expr> zero = ConstantExpr::create(0, 64);
    const Array *array = ac.CreateArray("arrSym", 1);
    klee::ref<Expr> sym = Expr::createTempRead(array, 8);

    if(!solver)
        solver = new TimingSolver(klee::createCoreSolver(CoreSolverToUse));

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
              
    SymbolicPTA sPTA(*solver, state, dl);
    llvm::Type* shrtTy = llvm::IntegerType::getInt16Ty(ctx)->getPointerTo();
    sPTA.giveMemoryObjectType(&pointerMo, shrtTy->getPointerTo());
    sPTA.giveMemoryObjectType(&pointeeMo, shrtTy->getPointerTo());

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

    //test iteration
    std::vector<std::pair<Pointer*, Pointer*>> returnedPointers;
    for(auto e : sPTA.traverse(ptr)) { 
        returnedPointers.push_back(e);
    }
    ASSERT_EQ(returnedPointers.size(), (uint64_t)3);
    ASSERT_TRUE(IN_CONTAINER(returnedPointers, std::make_pair(ptr,pointeePtr)));
    ASSERT_TRUE(IN_CONTAINER(returnedPointers, std::make_pair(pointeePtr,pointeePtr)));
    ASSERT_TRUE(IN_CONTAINER(returnedPointers, std::make_pair(pointeePtr,ptr)));
}

TEST(SymbolicPTATest, ColocatedArray) {
    if(!Context::isInit())
      Context::initialize(true, 64);
    ExecutionState state;
    klee::ref<Expr> zero = ConstantExpr::create(0, 64);

    llvm::LLVMContext ctx;
    llvm::Type* shrtTy = llvm::IntegerType::getInt16Ty(ctx)->getPointerTo();
    llvm::Type* intTy = llvm::IntegerType::getInt32Ty(ctx)->getPointerTo();
    llvm::Type* charTy = llvm::IntegerType::getInt8Ty(ctx)->getPointerTo();
    llvm::Type* primitiveType = llvm::IntegerType::getInt16Ty(ctx);

    llvm::ArrayRef<llvm::Type*> types({shrtTy, intTy, primitiveType, charTy});

    llvm::Type* st = llvm::StructType::create(types);

    if(!solver)
        solver = new TimingSolver(klee::createCoreSolver(CoreSolverToUse));

    const auto size = dl.getTypeAllocSize(st)*2;
    MemoryObject pointerMo((uint64_t)malloc(size));
    pointerMo.size = size;

              
    SymbolicPTA sPTA(*solver, state, dl);
    sPTA.giveMemoryObjectType(&pointerMo, st->getPointerTo());

    Pointer* ptr0_0 = sPTA.getPointer(&pointerMo, ConstantExpr::create(0,64));
    Pointer* ptr8_0 = sPTA.getPointer(&pointerMo, ConstantExpr::create(8,64));
    Pointer* ptr24_0 = sPTA.getPointer(&pointerMo, ConstantExpr::create(24,64));

    Pointer* ptr0_1 = sPTA.getPointer(&pointerMo, ConstantExpr::create(32,64));
    Pointer* ptr8_1 = sPTA.getPointer(&pointerMo, ConstantExpr::create(40,64));
    Pointer* ptr24_1 = sPTA.getPointer(&pointerMo, ConstantExpr::create(56,64));

    std::vector<Pointer*> ptrs = sPTA.getColocatedPointers(*ptr0_0);
    ASSERT_EQ(ptrs.size(), (uint64_t)6);
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr8_0));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr0_0));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr24_0));

    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr8_1));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr0_1));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr24_1));

    ptrs = sPTA.getColocatedPointers(*ptr8_0);
    ASSERT_EQ(ptrs.size(), (uint64_t)6);
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr8_0));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr0_0));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr24_0));

    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr8_1));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr0_1));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr24_1));

    ptrs = sPTA.getColocatedPointers(*ptr24_0);
    ASSERT_EQ(ptrs.size(), (uint64_t)6);
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr8_0));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr0_0));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr24_0));

    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr8_1));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr0_1));
    ASSERT_TRUE(IN_CONTAINER(ptrs, ptr24_1));
}
