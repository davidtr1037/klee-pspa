#include "klee/Internal/Analysis/Reachability.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>

#include <stack>

using namespace llvm;
using namespace klee;
using namespace std;


void klee::computeReachableFunctions(Function *entry,
                                     FunctionSet &results) {
    stack<Function *> worklist;
    FunctionSet pushed;

    worklist.push(entry);
    pushed.insert(entry);
    results.insert(entry);

    while (!worklist.empty()) {
        Function *f = worklist.top();
        worklist.pop();

        for (inst_iterator iter = inst_begin(f); iter != inst_end(f); iter++) {
            Instruction *inst = &*iter;
            if (inst->getOpcode() != Instruction::Call) {
                continue;
            }

            CallInst *callInst = dyn_cast<CallInst>(inst);
            Function *target = callInst->getCalledFunction();
            if (!target) {
                continue;
            }

            results.insert(target);
            if (target->isDeclaration()) {
                continue;
            }

            if (pushed.find(target) == pushed.end()) {
                worklist.push(target);
                pushed.insert(target);
            }
        }
    }
}
