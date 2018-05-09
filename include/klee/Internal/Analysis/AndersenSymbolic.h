#ifndef KLEE_ANDERSENSYMBOLIC_H
#define KLEE_ANDERSENSYMBOLIC_H

#include <WPA/Andersen.h>
#include <WPA/AndersenDynamic.h>

#include <llvm/IR/Module.h>

namespace klee {

class AndersenSymbolic : public AndersenDynamic {
public:

    AndersenSymbolic(llvm::Module &module) : module(module) {

    }

    void setup();

private:

    llvm::Module &module;
};

}

#endif
