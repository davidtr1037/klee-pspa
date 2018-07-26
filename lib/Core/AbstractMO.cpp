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

NodeID klee::computeAbstractMO(PointerAnalysis *pta,
                               DynamicMemoryLocation &location,
                               bool *canStronglyUpdate) {
  bool isArrayElement = false;

  if (canStronglyUpdate != NULL) {
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

  NodeID nodeId = pta->getPAG()->getObjectNode(location.value);
  if (SymbolTableInfo::isConstantObj(nodeId)) {
    /* strings, ... */
    return nodeId;
  }

  if (SymbolTableInfo::isBlkObj(nodeId)) {
    /* TODO: handle... */
    assert(false);
  }

  if (location.isSymbolicOffset) {
    /* if the offset is symbolic, we don't have much to do... */
    NodeID objId = pta->getFIObjNode(nodeId);
    pta->setObjFieldInsensitive(objId);
    return objId;
  }

  uint32_t offset = location.offset;
  Type *elementType;
  uint32_t abstractOffset;

  const MemObj *mem = pta->getPAG()->getObject(nodeId);
  if (!mem) {
    assert(false);
  }

  if (mem->isFunction()) {
    return pta->getFIObjNode(nodeId);
  }

  if (mem->isHeap()) {
    if (canStronglyUpdate != NULL) {
      *canStronglyUpdate = false;
    }
    if (!location.hint) {
      /* handle field-insensitively */
      NodeID objId = pta->getFIObjNode(nodeId);
      pta->setObjFieldInsensitive(objId);
      return objId;
    }

    /* we have a type hint... */
    elementType = location.hint->getElementType();
    StInfo *stInfo = SymbolTableInfo::SymbolInfo()->getStructInfo(elementType);
    offset = offset % stInfo->getSize();
    abstractOffset = computeAbstractFieldOffset(offset, elementType, isArrayElement);
    return pta->getGepObjNode(nodeId, LocationSet(abstractOffset));
  }

  elementType = moType->getElementType();
  if (elementType->isSingleValueType()) {
    return pta->getFIObjNode(nodeId);
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
