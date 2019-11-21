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
//  inst->dump();
  for (NodeID n : pta->getPts(id)) {
    /* I think we can do it without disabling unique AS */
    //pts.set(stripUniqueAS(pta, n));
    pts.set(n);
    PAGNode *pagNode = pta->getPAG()->getPAGNode(n);
    ObjPN *obj = dyn_cast<ObjPN>(pagNode);
    if(!obj) continue;
    if(obj->getMemObj()->getRefVal() == nullptr) continue;
//    obj->getMemObj()->getRefVal()->dump();
     
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

static uint64_t freshColours = 10000000;

std::vector<int> ColourCollector::getColour(const llvm::Value *inst, Type *hint) {
  NodeID objnodeId = pta->getPAG()->getObjectNode(inst);
  const MemObj *memobj = pta->getPAG()->getBaseObj(objnodeId);
  assert(memobj && "Passed instructions is not an allocation");
  std::vector<std::pair<NodeID, GepObjPN*>> fields;

  if (memobj->isFieldInsensitive()) {
    fields.emplace_back(pta->getPAG()->getFIObjNode(memobj), nullptr);
  } else {
    if (hint) {
      StInfo *stInfo = SymbolTableInfo::SymbolInfo()->getStructInfo(hint);
      for (unsigned int i = 0; i < stInfo->getSize(); i++) {
        NodeID id = pta->getGepObjNode(memobj->getSymId(), LocationSet(i));
        PAGNode *node = pta->getPAG()->getPAGNode(id);
        GepObjPN *gep = dyn_cast<GepObjPN>(node);
        fields.emplace_back(id, gep);
      }
    } else {
      NodeBS &allFields = pta->getPAG()->getAllFieldsObjNode(memobj);
      for (auto nId : allFields) {
        PAGNode* pn = pta->getPAG()->getPAGNode(nId);
        GepObjPN* gep = dyn_cast<GepObjPN>(pn);
        if (gep) {
          fields.emplace_back(nId,gep);
        }
      }
    }
  }

  int idx = 0;
  //A vector of offset sorted colours
  std::vector<int> returnColours(fields.size(), 0);
  for (const auto& fIdGep : fields) {
    int colour = 0;
    bool hasColor = false;
    for (auto pts : ptsSets) {
      if (pts.test(fIdGep.first)) {
        returnColours[idx] = colour;
        hasColor = true;
      }
      colour++;
    }
    if (!hasColor) {
      /* TODO: add fresh color? */
      returnColours[idx] = freshColours++;
    }
    idx++;
  }

  //for (auto pts : ptsSets) {
  //  colour++;
  //  int idx = 0;
  //  bool hasColor = false;
  //  for (const auto& fIdGep : fields) {
  //    if (pts.test(fIdGep.first)) {
  //      returnColours[idx] = colour;
  //      hasColor = true;
  //    }
  //    idx++;
  //  }
  //  if (!hasColor) {
  //    errs() << "MISS\n";
  //  }
  //}

  //filter out fields that don't have a colour. For example FI node, if FS nodes are present
  std::vector<int> returnCols;
  for (auto& c : returnColours) {
    if (c > 0) {
      returnCols.emplace_back(c);
    }
  }

  //if there are no return colours, it means there were no stores in the
  //analyzed function to this object therefore we assign it a new fresh colour.
  if(returnCols.size() == 0) {
      returnCols.emplace_back(freshColours++);
  }

  return returnCols;
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
