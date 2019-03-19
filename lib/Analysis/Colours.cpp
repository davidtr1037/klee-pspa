#include "klee/Internal/Analysis/Colours.h"

#include <MemoryModel/PointerAnalysis.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/DebugInfo.h>

using namespace llvm;
using namespace klee;


void ColourCollector::visitStore(PointerAnalysis *pta,
                                 Function *f,
                                 StoreInst *inst) {
  NodeID id = pta->getPAG()->getValueNode(inst->getPointerOperand());
  PointsTo pts = pta->getPts(id);
  ptsSets.push_back(pts);
}

void ColourCollector::computeColours(PointerAnalysis* pta) {
    int changes = 0;
    
    //Set all nodes to FI, so they will intersect
    for(auto pts: ptsSets) {
        for(auto nodeId : pts) {
            pts.set(pta->getFIObjNode(nodeId));
            pts.set(pta->getBaseObjNode(nodeId));
        }
    }
    
    do {
       changes = 0;
       for(auto it = ptsSets.begin(); it != ptsSets.end(); it++) {
           for(auto it_other = it+1; it_other != ptsSets.end(); ) {
               if(it->intersects(*it_other)) {
                   changes++;
                   *it |= *it_other;
                   it_other = ptsSets.erase(it_other);
               } else it_other++;
           }
       }
      errs() << "Completed loop with " << changes << " changes\n";
    } while(changes > 0);


//  for(auto pts : ptsSets) {
//      llvm::dump(pts, errs());
//  }
   int colour = 1;
   for(auto pts : ptsSets) {
       colour++;
       for(auto nodeId : pts) {
          PAGNode *pagNode = pta->getPAG()->getPAGNode(nodeId);
          ObjPN *obj = dyn_cast<ObjPN>(pagNode);
          if(!obj) continue;
          auto inst = dyn_cast<Instruction>(obj->getMemObj()->getRefVal());
          if(!inst) continue;
          MDNode *n = inst->getMetadata("dbg");
          if(!n) continue;
          DILocation *loc = cast<DILocation>(n);
          outputFile << loc->getFilename();
          outputFile << ":" << loc->getLine();
          outputFile << " " << colour << "\n";
       }
    }
   errs() << "There are " << ptsSets.size() << " colours\n";
    
}
