#include "AbstractMO.h"

#include <llvm/IR/Type.h>

#include <MemoryModel/MemModel.h>
#include <MemoryModel/LocationSet.h>

#include <stdint.h>

using namespace klee;
using namespace llvm;

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
        moType->dump();
        errs() << "offset: " << offset << "\n";
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
            if (offset < fl.offset + fl.size) {
                return flattenOffset + computeAbstractFieldOffset(offset - fl.offset,
                                                                  fl.type);
            }

            StInfo *fieldStInfo = SymbolTableInfo::SymbolInfo()->getStructInfo(fl.type);
            flattenOffset += fieldStInfo->getFlattenFieldInfoVec().size();
        }
    }

    if (isa<FunctionType>(moType)) {
        /* TODO: is it correct? */
        return 0;
    }

    errs() << "unexpected type: " << moType->getTypeID(); moType->dump();
    assert(false);
    return 0;
}
