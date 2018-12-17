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

  llvm::Type* types[4] = {shrtTy, intTy, primitiveType, charTy};

  llvm::Type* st = llvm::StructType::create(types);
  
  llvm::DataLayout dl("e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128");



  OffsetFinder of(dl);
  auto offsets = of.visit(st);
  ASSERT_EQ(offsets.size(), (uint64_t)3);
  ASSERT_EQ(offsets[0].offset, (uint64_t)0);
  ASSERT_EQ(offsets[1].offset, (uint64_t)8);
  ASSERT_EQ(offsets[2].offset, (uint64_t)24);

  llvm::Type* typesPacked[4] = {primitiveType, shrtTy, intTy, charTy};
  st = llvm::StructType::create(ctx, typesPacked, llvm::StringRef(), true);
  auto offsetsPacked = of.visit(st);
  ASSERT_EQ(offsetsPacked.size(), (uint64_t)3);
  ASSERT_EQ(offsetsPacked[0].offset, (uint64_t)2);
  ASSERT_EQ(offsetsPacked[1].offset, (uint64_t)10);
  ASSERT_EQ(offsetsPacked[2].offset, (uint64_t)18);

  llvm::Type* typesNested[5] = {primitiveType, shrtTy, st, primitiveType, charTy};
  st = llvm::StructType::create(typesNested);
  auto offsetsNested = of.visit(st);
  ASSERT_EQ(offsetsNested.size(), (uint64_t)5);
  ASSERT_EQ(offsetsNested[0].offset, (uint64_t)8);
  ASSERT_EQ(offsetsNested[1].offset, (uint64_t)18);
  ASSERT_EQ(offsetsNested[2].offset, (uint64_t)26);
  ASSERT_EQ(offsetsNested[3].offset, (uint64_t)34);
  ASSERT_EQ(offsetsNested[4].offset, (uint64_t)48);


}

TEST(OffsetFinderTest, Array) {
  llvm::LLVMContext ctx;
  llvm::Type* shrtTy = llvm::IntegerType::getInt16Ty(ctx)->getPointerTo();
  llvm::Type* primitiveType = llvm::IntegerType::getInt16Ty(ctx);
  
  llvm::DataLayout dl("e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128");

  auto shrtArray = llvm::ArrayType::get(shrtTy, 4);
  auto primitiveArray = llvm::ArrayType::get(primitiveType, 12);



  OffsetFinder of(dl);
  auto offsets = of.visit(shrtArray);
  ASSERT_EQ(offsets.size(), (uint64_t)4);
  ASSERT_EQ(offsets[0].offset, (uint64_t)0);
  ASSERT_EQ(offsets[0].isWeak, false);
  ASSERT_EQ(offsets[1].offset, (uint64_t)8);
  ASSERT_EQ(offsets[1].isWeak, true);
  ASSERT_EQ(offsets[2].offset, (uint64_t)16);
  ASSERT_EQ(offsets[2].isWeak, true);
  ASSERT_EQ(offsets[3].offset, (uint64_t)24);
  ASSERT_EQ(offsets[3].isWeak, true);

  auto offsetsPacked = of.visit(primitiveArray);
  ASSERT_EQ(offsetsPacked.size(), (uint64_t)0);

}

TEST(OffsetFinderTest, StructArray) {
  llvm::LLVMContext ctx;
  llvm::Type* shrtTy = llvm::IntegerType::getInt16Ty(ctx)->getPointerTo();
  llvm::Type* intTy = llvm::IntegerType::getInt32Ty(ctx)->getPointerTo();
  llvm::Type* charTy = llvm::IntegerType::getInt8Ty(ctx)->getPointerTo();
  llvm::Type* primitiveType = llvm::IntegerType::getInt16Ty(ctx);

  auto shrtArray = llvm::ArrayType::get(shrtTy, 2);
  llvm::Type* types[4] = {intTy, primitiveType, shrtArray, charTy};
  
  llvm::DataLayout dl("e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128");

  llvm::Type* st = llvm::StructType::create(ctx, types, llvm::StringRef(), false);
  OffsetFinder of(dl);
  auto offsets = of.visit(st);
  ASSERT_EQ(offsets.size(), (uint64_t)4);
  ASSERT_EQ(offsets[0].offset, (uint64_t)0);
  ASSERT_EQ(offsets[1].offset, (uint64_t)16);
  ASSERT_EQ(offsets[1].isWeak, false);
  ASSERT_EQ(offsets[2].offset, (uint64_t)24);
  ASSERT_EQ(offsets[2].isWeak, true);
  ASSERT_EQ(offsets[3].offset, (uint64_t)32);

  llvm::Type* typesPacked[4] = {primitiveType, shrtTy, intTy, charTy};
  st = llvm::StructType::create(ctx, typesPacked, llvm::StringRef(), true);
  auto packedSturctsArray = llvm::ArrayType::get(st, 2);
  OffsetFinder ofPacked(dl);
  auto offsetsPacked = ofPacked.visit(packedSturctsArray);
  ASSERT_EQ(offsetsPacked.size(), (uint64_t)6);
  ASSERT_EQ(offsetsPacked[0].offset, (uint64_t)2);
  ASSERT_EQ(offsetsPacked[1].offset, (uint64_t)10);
  ASSERT_EQ(offsetsPacked[2].offset, (uint64_t)18);
  ASSERT_EQ(offsetsPacked[3].offset, (uint64_t)28);
  ASSERT_EQ(offsetsPacked[4].offset, (uint64_t)36);
  ASSERT_EQ(offsetsPacked[5].offset, (uint64_t)44);
}
