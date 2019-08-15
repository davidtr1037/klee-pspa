#include "AbstractMO.h"

#include <llvm/IR/Type.h>
#include <llvm/IR/Instruction.h>

#include <MemoryModel/MemModel.h>
#include <MemoryModel/LocationSet.h>
#include <MemoryModel/PointerAnalysis.h>

#include <stdint.h>

using namespace klee;
using namespace llvm;
using namespace std;

/* TODO: try to avoid as many lookups as possible */
NodeID klee::computeAbstractMO(PointerAnalysis *pta,
                               DynamicMemoryLocation &location,
                               bool enforceFI,
                               bool *canStronglyUpdate) {
  if (canStronglyUpdate) {
    *canStronglyUpdate = true;
  }

  if (isa<ConstantPointerNull>(location.value)) {
    return pta->getPAG()->getNullPtr();
  }

  /* get the type from the allocation site */
  PointerType *moType = dyn_cast<PointerType>(location.value->getType());
  if (!moType) {
    const CallInst *callInst = dyn_cast<CallInst>(location.value);
    if (callInst) {
      /* TODO: better solution for varargs? */
      return pta->getPAG()->getBlackHoleNode();
    }

    /* TODO: check the __uClibc_main wierd case... */
    llvm::report_fatal_error("Unexpected type of allocation site");
  }

  if (!pta->getPAG()->hasObjectNode(location.value)) {
    return pta->getPAG()->getBlackHoleNode();
  }

  NodeID nodeId = pta->getPAG()->getObjectNode(location.value);
  const MemObj *mem = pta->getPAG()->getObject(nodeId);
  if (!mem) {
    /* this shouldn't happen */
    assert(false);
  }

  if (mem->isConstantObj()) {
    /* strings, ... */
    return nodeId;
  }

  if (mem->isBlackHoleObj()) {
    /* TODO: handle... */
    assert(false);
  }

  if (location.isSymbolicOffset) {
    /* if the offset is symbolic, we don't have much to do... */
    if (canStronglyUpdate) {
      *canStronglyUpdate = false;
    }
    if (enforceFI) {
      makeFieldInsensitive(const_cast<MemObj *>(mem));
    }
    return pta->getPAG()->getFIObjNode(mem);
  }

  bool isArrayElement = false;
  uint32_t offset = location.offset;
  Type *elementType;
  uint32_t abstractOffset;

  if (mem->isFieldInsensitive()) {
    if (canStronglyUpdate) {
      *canStronglyUpdate = false;
    }
    return pta->getPAG()->getFIObjNode(mem);
  }

  if (mem->isFunction()) {
    return pta->getPAG()->getFIObjNode(mem);
  }

  if (mem->isHeap()) {
    if (!location.hint) {
      /* handle field-insensitively */
      if (canStronglyUpdate) {
        *canStronglyUpdate = false;
      }
      if (enforceFI) {
        makeFieldInsensitive(const_cast<MemObj *>(mem));
      }
      return pta->getPAG()->getFIObjNode(mem);
    }

    /* we have a type hint... */
    elementType = location.hint->getElementType();
    StInfo *stInfo = SymbolTableInfo::SymbolInfo()->getStructInfo(elementType);
    offset = offset % stInfo->getSize();
    abstractOffset = computeAbstractFieldOffset(offset, elementType, isArrayElement);
    if (location.size > stInfo->getSize() || isArrayElement) {
      /* that should be an array, then we can't perform strong update */
      if (canStronglyUpdate) {
          *canStronglyUpdate = false;
      }
    }
    return pta->getGepObjNode(nodeId, LocationSet(abstractOffset));
  }

  elementType = moType->getElementType();
  if (elementType->isSingleValueType()) {
    return pta->getPAG()->getFIObjNode(mem);
  }

  abstractOffset = computeAbstractFieldOffset(offset, elementType, isArrayElement);
  if (canStronglyUpdate && isArrayElement) {
    *canStronglyUpdate = false;
  }
  return pta->getGepObjNode(nodeId, LocationSet(abstractOffset));
}

uint32_t klee::computeAbstractFieldOffset(uint32_t offset,
                                          const Type *moType,
                                          bool &isArrayElement) {
  if (moType->isSingleValueType()) {
    /* there are 2 options in this case:
       - a pointer to a primitive type
       - a pointer to an array (which is probably dynamically allocated)
       so we should be field insensitive here */
    return 0;
  }

  StInfo *stInfo = SymbolTableInfo::SymbolInfo()->getStructInfo(moType);
  if (offset > stInfo->getSize()) {
    assert(false);
  }

  if (isa<ArrayType>(moType)) {
    isArrayElement = true;
    FieldLayout &fl = stInfo->getFieldLayoutVec().front();
    uint32_t modOffset = offset % fl.size;
    return computeAbstractFieldOffset(modOffset, fl.type, isArrayElement);
  }

  if (isa<StructType>(moType)) {
    uint32_t flattenOffset = 0;
    for (FieldLayout &fl : stInfo->getFieldLayoutVec()) {
      if (offset < fl.offset) {
        /* something went wrong... */
        assert(false);
      }

      uint32_t unalignedSize, nextOffset;
      if (offset < fl.offset + fl.size) {
        unalignedSize = SymbolTableInfo::SymbolInfo()->getTypeSizeInBytes(fl.type);
        nextOffset = min(unalignedSize - 1, offset - fl.offset);
        return flattenOffset + computeAbstractFieldOffset(nextOffset,
                                                          fl.type,
                                                          isArrayElement);
      }

      StInfo *fieldStInfo = SymbolTableInfo::SymbolInfo()->getStructInfo(fl.type);
      flattenOffset += fieldStInfo->getFlattenFieldInfoVec().size();
    }

    /* we should not get here... */
    assert(false);
  }

  if (isa<FunctionType>(moType)) {
    /* TODO: is it correct? */
    return 0;
  }

  assert(false);
  return 0;
}

void klee::makeFieldInsensitive(MemObj *mem) {
  mem->setFieldInsensitive();
  mem->setPermanentlyFI();
}
