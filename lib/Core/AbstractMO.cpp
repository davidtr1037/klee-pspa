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
                               PointerType *hint) {
    /* get the type from the allocation site */
    PointerType *moType = dyn_cast<PointerType>(location.value->getType());
    if (!moType) {
        const CallInst *callInst = dyn_cast<CallInst>(location.value);
        if (callInst) {
            Function *callee = callInst->getCalledFunction();
            if (callee->getName() == "__uClibc_main") {
                return 0;
            }
            if (callee->getName() == "version_etc") {
                return 0;
            }
        }

        /* TODO: check the __uClibc_main wierd case... */
        llvm::report_fatal_error("Unexpected type of allocation site");
        return 0;
    }

    NodeID nodeID = pta->getPAG()->getObjectNode(location.value);
    if (SymbolTableInfo::isConstantObj(nodeID)) {
        /* strings, ... */
        return nodeID;
    }

    if (SymbolTableInfo::isBlkObj(nodeID)) {
        /* TODO: handle... */
        assert(false);
    }

    uint32_t offset = location.offset;
    Type *elementType;
    uint32_t abstractOffset;

    if (pta->isHeapMemObj(nodeID)) {
        if (!hint) {
            /* handle similarly to arrays */
            return pta->getGepObjNode(nodeID, LocationSet(0));
        }

        /* we have a type hint... */
        elementType = hint->getElementType();
        StInfo *stInfo = SymbolTableInfo::SymbolInfo()->getStructInfo(elementType);
        offset = offset % stInfo->getSize();
        abstractOffset = computeAbstractFieldOffset(offset, elementType);
        return pta->getGepObjNode(nodeID, LocationSet(abstractOffset));
    }

    elementType = moType->getElementType();
    if (elementType->isSingleValueType()) {
        return pta->getFIObjNode(nodeID);
    }

    abstractOffset = computeAbstractFieldOffset(offset, elementType);
    return pta->getGepObjNode(nodeID, LocationSet(abstractOffset));
}

uint32_t klee::computeAbstractFieldOffset(uint32_t offset,
                                          const Type *moType) {
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
        FieldLayout &fl = stInfo->getFieldLayoutVec().front();
        uint32_t modOffset = offset % fl.size;
        return computeAbstractFieldOffset(modOffset, fl.type);
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
                return flattenOffset + computeAbstractFieldOffset(nextOffset, fl.type);
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
