#include "klee/Internal/Analysis/Colours.h"
#include "klee/Internal/Support/ErrorHandling.h"
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
  PointsTo pts;
  for (NodeID n : pta->getPts(id)) {
    /* I think we can do it without disabling unique AS */
    //pts.set(stripUniqueAS(pta, n));
    pts.set(n);
  }
  ptsSets.push_back(pts);
}

bool ColourCollector::intersects(PointerAnalysis* pta,
                                 PointsTo &pts1,
                                 PointsTo &pts2) {
  for (NodeID n1 : pts1) {
    for (NodeID n2 : pts2) {
      PAGNode *pagNode;
      pagNode = pta->getPAG()->getPAGNode(n1);
      ObjPN *obj1 = dyn_cast<ObjPN>(pagNode);
      pagNode = pta->getPAG()->getPAGNode(n2);
      ObjPN *obj2 = dyn_cast<ObjPN>(pagNode);
      bool fiCheck = isa<FIObjPN>(obj1) || isa<FIObjPN>(obj2);
      if (fiCheck) {
        if (obj1->getMemObj()->getSymId() == obj2->getMemObj()->getSymId()) {
          return true;
        }
      } else {
        if (n1 == n2) {
          return true;
        }
      }
    }
  }

  return false;
}

std::vector<int> ColourCollector::getColour(llvm::Instruction *inst) {
  NodeID objnodeId = pta->getPAG()->getObjectNode(inst);
  //llvm::errs() << " object node Id" << objnodeId << "\n";
  const MemObj *memobj = pta->getPAG()->getBaseObj(objnodeId);
  assert(memobj && "Passed instructions is not an allocation");
  NodeBS &allFields = pta->getPAG()->getAllFieldsObjNode(memobj);
//  llvm::errs() << objnodeId << ":\n";
  std::vector<std::pair<NodeID, GepObjPN*>> fields;
  for(auto nId : allFields) {
      PAGNode* pn = pta->getPAG()->getPAGNode(nId);
 //     llvm::errs() << "\t" << nId  << "\n";
      if(allFields.count() == 1) { //We don't skip field insenstive nodes if there is only 1
          fields.emplace_back(nId,nullptr);
      } else { //Otherwise we skip FI nodes, (they shouldn't have a pts set anyway)
          GepObjPN* gep = dyn_cast<GepObjPN>(pn);
          if(gep) {
 //          llvm::errs() << "offset: " << gep->getLocationSet().getOffset();
 //          llvm::errs() << "\n";
            fields.emplace_back(nId,gep);
          }
      }
  }

  int colour = 0;
//  llvm::BitVector returnColour(ptsSets.size() + 1);
  //A vector of offset sorted colours
  std::vector<int> returnColours(fields.size(), 0);
  for (auto pts : ptsSets) {
    colour++;
    int idx = 0;
    for(const auto& fIdGep : fields) {
      if(pts.test(fIdGep.first)) {
//          llvm::errs() << fIdGep.first << " has colour " << colour;
          if(fIdGep.second) {
//              llvm::errs() << " at offset: " << fIdGep.second->getLocationSet().getOffset(); 
          }
//          llvm::errs() << "\n";
          returnColours[idx] = colour;
      }
      idx++;
    }
  }
  return returnColours;
}

void ColourCollector::computeColours(PointerAnalysis* pta, 
                                     llvm::raw_ostream &outputFile) {
  
  int changes = 0;
  this->pta = pta;
  witMode = true;

  do {
    changes = 0;
    for (auto i = ptsSets.begin(); i != ptsSets.end(); i++) {
      for (auto j = i + 1; j != ptsSets.end(); ) {
        PointsTo &pts1 = *i;
        PointsTo &pts2 = *j;
        if (intersects(pta, pts1, pts2)) {
          changes++;
          pts1 |= pts2;
          j = ptsSets.erase(j);
        } else {
          j++;
        }
      }
    }
    errs() << "Completed loop with " << changes << " changes\n";
  } while(changes > 0);
  int colour = 1;
  for(auto pts : ptsSets) {
      colour++;
      for(auto nodeId : pts) {
         PAGNode *pagNode = pta->getPAG()->getPAGNode(nodeId);
         ObjPN *obj = dyn_cast<ObjPN>(pagNode);
         if(!obj) continue;
         //if(!obj || isa<GepObjPN>(obj)) continue;
         if(obj->getMemObj()->getRefVal() == nullptr) continue;
         auto inst = dyn_cast<Instruction>(obj->getMemObj()->getRefVal());
         if(!inst) continue;
         MDNode *n = inst->getMetadata("dbg");
         if(!n) continue;
         DILocation *loc = cast<DILocation>(n);
         outputFile << loc->getFilename();
         outputFile << ":" << loc->getLine();
         outputFile << " " << colour << " " << nodeId << "\n";
      }
	}

  klee_message("There are %lu colours", ptsSets.size());
}
