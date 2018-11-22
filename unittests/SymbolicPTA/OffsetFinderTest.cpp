#include "Core/Memory.h"
#include "Analysis/SymbolicPTA.h"
#include "llvm/IR/DerivedTypes.h"


#include <vector>
#include <cstring>
#include <algorithm>

#include "gtest/gtest.h"
#define IN_CONTAINER(c,item) (std::find(c.begin(), c.end(), item) != c.end())

using namespace klee;
TEST(OffsetFinderTest, Struct) {
  llvm::LLVMContext ctx;
  llvm::Type* shrtTy = llvm::IntegerType::getInt16Ty(ctx)->getPointerTo();
  llvm::Type* intTy = llvm::IntegerType::getInt32Ty(ctx)->getPointerTo();
  llvm::Type* charTy = llvm::IntegerType::getInt8Ty(ctx)->getPointerTo();
  llvm::Type* primitiveType = llvm::IntegerType::getInt16Ty(ctx);

  llvm::ArrayRef<llvm::Type*> types({shrtTy, intTy, primitiveType, charTy});

  llvm::Type* st = llvm::StructType::create(types);
  
  llvm::DataLayout dl("e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128");



  OffsetFinder of(dl);
  std::vector<unsigned> offsets = of.visit(st);
  ASSERT_EQ(offsets.size(), (uint64_t)3);
  ASSERT_EQ(offsets[0], (uint64_t)0);
  ASSERT_EQ(offsets[1], (uint64_t)8);
  ASSERT_EQ(offsets[2], (uint64_t)24);

  llvm::ArrayRef<llvm::Type*> typesPacked({primitiveType, shrtTy, intTy, charTy});
  OffsetFinder ofPacked(dl);
  st = llvm::StructType::create(ctx, typesPacked, llvm::StringRef(), true);
  auto offsetsPacked = ofPacked.visit(st);
  ASSERT_EQ(offsetsPacked.size(), (uint64_t)3);
  ASSERT_EQ(offsetsPacked[0], (uint64_t)2);
  ASSERT_EQ(offsetsPacked[1], (uint64_t)10);
  ASSERT_EQ(offsetsPacked[2], (uint64_t)18);

  llvm::ArrayRef<llvm::Type*> typesNested({primitiveType, shrtTy, st, primitiveType, charTy});
  OffsetFinder ofNested(dl);
  st = llvm::StructType::create(typesNested);
  auto offsetsNested = ofNested.visit(st);
  ASSERT_EQ(offsetsNested.size(), (uint64_t)5);
  ASSERT_EQ(offsetsNested[0], (uint64_t)8);
  ASSERT_EQ(offsetsNested[1], (uint64_t)18);
  ASSERT_EQ(offsetsNested[2], (uint64_t)26);
  ASSERT_EQ(offsetsNested[3], (uint64_t)34);
  ASSERT_EQ(offsetsNested[4], (uint64_t)48);


}

