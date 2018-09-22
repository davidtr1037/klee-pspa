#include <stdio.h>
#include <iostream>
#include <vector>
#include <set>
#include <map>

#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Support/raw_ostream.h>

#include "MemoryModel/PointerAnalysis.h"

#include "klee/Internal/Analysis/ModRefAnalysis.h"

using namespace std;
using namespace llvm;

/* ModRefAnalysis class */

ModRefAnalysis::ModRefAnalysis(
    llvm::Module *module,
    ReachabilityAnalysis *ra,
    PointerAnalysis *pta,
    string entry,
    vector<string> targets,
    llvm::raw_ostream &debugs
) :
    module(module),
    ra(ra),
    pta(pta),
    entry(entry),
    targets(targets),
    debugs(debugs)
{

}

Function *ModRefAnalysis::getEntry() {
    return entryFunction;
}

vector<Function *> ModRefAnalysis::getTargets() {
    return targetFunctions;
}

void ModRefAnalysis::run() {
    /* validation */
    entryFunction = module->getFunction(entry);
    if (!entryFunction) {
        errs() << "entry function '" << entry << "' is not found (or unreachable)\n";
        assert(false);
    }
    for (string &name : targets) {
        Function *f = module->getFunction(name);
        if (!f) {
            errs() << "function '" << name << "' is not found (or unreachable)\n";
            assert(false);
        }
        targetFunctions.push_back(f);
    }

    /* collect mod information for each target function */
    for (Function * f : targetFunctions) {
        collectModInfo(f);
    }

    /* collect ref information with respect to the relevant call sites */
    for (Function *f : targetFunctions) {
        collectRefInfo(f);
    }

    /* compute the side effects of each target function */
    computeModRefInfo();

    /* for each modified object compute the modifying store instructions */
    computeModInfoToStoreMap();

    /* debug */
    //dumpModSetMap();
    //dumpLoadToStoreMap();
    //dumpLoadToModInfoMap();
    //dumpModInfoToStoreMap();
    //dumpModInfoToIdMap();
    //dumpOverridingStores();
}

ModRefAnalysis::ModInfoToStoreMap &ModRefAnalysis::getModInfoToStoreMap() {
    return modInfoToStoreMap;
}

bool ModRefAnalysis::mayBlock(Instruction *load) {
    LoadToStoreMap::iterator i = loadToStoreMap.find(load);
    return i != loadToStoreMap.end();
}

bool ModRefAnalysis::mayOverride(Instruction *store) {
    InstructionSet::iterator i = find(overridingStores.begin(), overridingStores.end(), store);
    return i != overridingStores.end();
}

ModRefAnalysis::SideEffects &ModRefAnalysis::getSideEffects() {
    return sideEffects;
}

bool ModRefAnalysis::hasSideEffects(Function *f) {
    InstructionSet &modSet = modSetMap[f];
    return !modSet.empty();
}

bool ModRefAnalysis::getSideEffects(Function *f, InstructionSet &modSet) {
    ModSetMap::iterator i = modSetMap.find(f);
    if (i == modSetMap.end()) {
        return false;
    }

    modSet = i->second;
    return true;
}

ModRefAnalysis::InstructionSet &ModRefAnalysis::getOverridingStores() {
    return overridingStores;
}

ModRefAnalysis::ModInfoToIdMap &ModRefAnalysis::getModInfoToIdMap() {
    return modInfoToIdMap;
}

bool ModRefAnalysis::getRetSliceId(llvm::Function *f, uint32_t &id) {
    RetSliceIdMap::iterator i = retSliceIdMap.find(f);
    if (i == retSliceIdMap.end()) {
        return false;
    }

    id = i->second;
    return true;
}

void ModRefAnalysis::collectModInfo(Function *entry) {
    set<Function *> &reachable = ra->getReachableFunctions(entry);

    for (Function *f : reachable) {
        if (f->isDeclaration()) {
            continue;
        }

        for (inst_iterator j = inst_begin(f); j != inst_end(f); j++) {
            Instruction *inst = &*j;
            if (inst->getOpcode() == Instruction::Store) {
                addStore(entry, inst);
            } 
        }
    }

    /* we don't need it any more... */
    cache.clear();
}

void ModRefAnalysis::addStore(Function *f, Instruction *inst) {
    StoreInst *store = dyn_cast<StoreInst>(inst);
    NodeID id = pta->getPAG()->getValueNode(store->getPointerOperand());
    PointsTo &pts = pta->getPts(id);

    PointsTo &modPts = modPtsMap[f];

    for (NodeID nodeId : pts) {
        /* get allocation site */
        PAGNode *pagNode = pta->getPAG()->getPAGNode(nodeId);
        ObjPN *obj = dyn_cast<ObjPN>(pagNode);
        if (!obj) {
            /* TODO: handle */
            assert(false);
        }

        /* TODO: check static objects? */
        if (obj->getMemObj()->isStack()) {
            const Value *value = obj->getMemObj()->getRefVal();
            if (canIgnoreStackObject(f, value)) {
                continue;
            }
        }

        pair<Function *, NodeID> k = make_pair(f, nodeId);
        objToStoreMap[k].insert(store);
        modPts.set(nodeId);
    }
}

bool ModRefAnalysis::canIgnoreStackObject(
    Function *f,
    const Value *value
) {
    bool result;
    AllocaInst *alloca = dyn_cast<AllocaInst>((Value *)(value));
    if (!alloca) {
        return false;
    }

    /* get the allocating function */
    Function *allocatingFunction = dyn_cast<Function>(alloca->getParent()->getParent());

    ReachabilityCache::iterator i = cache.find(allocatingFunction);
    if (i == cache.end()) {
        /* check if the entry reachable from the allocating function */
        set<Function *> reachable;
        ra->computeReachableFunctions(allocatingFunction, pta, reachable);

        /* save result */
        result = reachable.find(f) != reachable.end();
        cache.insert(make_pair(allocatingFunction, result));
    } else {
        result = i->second;
    }

    /* if reachable, then the stack object can't be ignored */
    return !result;
}

void ModRefAnalysis::collectRefInfo(Function *entry) {
    vector<CallInst *> callSites;
    for (Value::user_iterator i = entry->user_begin(); i != entry->user_end(); i++) {
        User *user = *i;
        if (isa<CallInst>(user)) {
            CallInst *callInst = dyn_cast<CallInst>(user);

            /* check if the call site is relevant */
            set<Function *> targets;
            ra->getCallTargets(callInst, targets);
            if (targets.find(entry) != targets.end()) {
                callSites.push_back(dyn_cast<CallInst>(user));
            }
        }
    }

    /* get reachable instructions */
    set<Instruction *> reachable;
    ra->getReachableInstructions(callSites, reachable);

    for (Instruction *inst : reachable) {
        /* handle load */
        if (inst->getOpcode() == Instruction::Load) {
            addLoad(entry, inst);
        }

        /* handle store */
        if (inst->getOpcode() == Instruction::Store) {
            addOverridingStore(inst);
        }
    }
}

void ModRefAnalysis::addLoad(Function *f, Instruction *inst) {
    LoadInst *load = dyn_cast<LoadInst>(inst);
    NodeID id = pta->getPAG()->getValueNode(load->getPointerOperand());
    PointsTo &pts = pta->getPts(id);

    PointsTo &refPts = refPtsMap[f];
    refPts |= pts;

    for (NodeID nodeId : pts) {
        pair<Function *, NodeID> k = make_pair(f, nodeId);
        objToLoadMap[k].insert(load);
    }
}

void ModRefAnalysis::addOverridingStore(Instruction *inst) {
    StoreInst *store = dyn_cast<StoreInst>(inst);
    NodeID id = pta->getPAG()->getValueNode(store->getPointerOperand());
    PointsTo &pts = pta->getPts(id);

    for (NodeID nodeId : pts) {
        objToOverridingStoreMap[nodeId].insert(store);
    }
}

void ModRefAnalysis::computeModRefInfo() {
    for (auto i : modPtsMap) {
        Function *f = i.first;
        PointsTo &modPts = i.second;

        /* get the corresponding ref-set */
        PointsTo &refPts = refPtsMap[f];
        /* compute the intersection */
        PointsTo pts = modPts & refPts;
        /* get the corresponding modifies-set */
        InstructionSet &modSet = modSetMap[f];

        for (NodeID nodeId : pts) {
            /* set key */
            pair<Function *, NodeID> k = make_pair(f, nodeId);

            /* update modifies-set */
            InstructionSet &stores = objToStoreMap[k];
            modSet.insert(stores.begin(), stores.end());

            /* get allocation site */
            AllocSite allocSite = getAllocSite(nodeId);

            InstructionSet &loads = objToLoadMap[k];
            for (Instruction *load : loads) {
                /* update with store instructions */
                loadToStoreMap[load].insert(stores.begin(), stores.end());

                /* update with allocation site */
                ModInfo modInfo = make_pair(f, allocSite);
                loadToModInfoMap[load].insert(modInfo);
            }

            /* update overriding stores */
            InstructionSet &localOverridingStores = objToOverridingStoreMap[nodeId];
            overridingStores.insert(localOverridingStores.begin(), localOverridingStores.end());
        }
    }
}

void ModRefAnalysis::computeModInfoToStoreMap() {
    uint32_t sliceId = 1;

    for (Function *f : targetFunctions) {
        InstructionSet &modSet = modSetMap[f];

        uint32_t retSliceId = sliceId++;
        if (hasReturnValue(f)) {
            retSliceIdMap[f] = retSliceId;
            SideEffect sideEffect = {
                .type = ReturnValue,
                .id = retSliceId,
                .info = {
                    .f = f
                }
            };
            sideEffects.push_back(sideEffect);
        }

        for (Instruction *inst : modSet) {
            StoreInst *store = dyn_cast<StoreInst>(inst);
            NodeID id = pta->getPAG()->getValueNode(store->getPointerOperand());
            PointsTo &pts = pta->getPts(id);

            for (NodeID nodeId : pts) {
                /* update store instructions */
                AllocSite allocSite = getAllocSite(nodeId);
                ModInfo modInfo = make_pair(f, allocSite);
                modInfoToStoreMap[modInfo].insert(inst);

                if (modInfoToIdMap.find(modInfo) == modInfoToIdMap.end()) {
                    uint32_t modSliceId = sliceId++;
                    modInfoToIdMap[modInfo] = modSliceId;
                    SideEffect sideEffect = {
                        .type = Modifier,
                        .id = modSliceId,
                        .info = {
                            .modInfo = modInfo
                        }
                    };
                    sideEffects.push_back(sideEffect);
                }
            }
        }
    }
}

ModRefAnalysis::AllocSite ModRefAnalysis::getAllocSite(NodeID nodeId) {
    PAGNode *pagNode = pta->getPAG()->getPAGNode(nodeId);
    ObjPN *obj = dyn_cast<ObjPN>(pagNode);
    assert(obj);

    /* get allocation site value */
    const MemObj *mo = obj->getMemObj();
    const Value *allocSite = mo->getRefVal();

    /* get offset in bytes */
    uint64_t offset = 0;
    if (obj->getNodeKind() == PAGNode::GepObjNode) {
        GepObjPN *gepObj = dyn_cast<GepObjPN>(obj);
        /* TODO: do we need the offset? */
        offset = gepObj->getLocationSet().getOffset(); 
    }

    return make_pair(allocSite, offset);
}

bool ModRefAnalysis::hasReturnValue(Function *f) {
    return !f->getReturnType()->isVoidTy();
}

/* TODO: validate that a load can't have two ModInfo's with the same allocation site */
void ModRefAnalysis::getApproximateModInfos(Instruction *inst, AllocSite hint, set<ModInfo> &result) {
    assert(inst->getOpcode() == Instruction::Load);

    LoadToModInfoMap::iterator entry = loadToModInfoMap.find(inst);
    if (entry == loadToModInfoMap.end()) {
        /* TODO: this should not happen */
        assert(false);
    }

    set<ModInfo> &modifiers = entry->second;

    for (auto i : modifiers) {
        ModInfo modInfo = i;
        AllocSite allocSite = modInfo.second;

        /* compare only the allocation sites (values) */
        if (allocSite.first == hint.first) {
            result.insert(modInfo);
        }
    }

    return;
}

void ModRefAnalysis::dumpModSetMap() {
    debugs << "### ModSetMap ###\n";

    for (auto i : modSetMap) {
        Function *f = i.first;
        InstructionSet &modSet = i.second;

        debugs << "# " << f->getName() << " #\n";
        for (Instruction *inst : modSet) {
            dumpInst(inst);
        }
    }
    debugs << "\n";
}

void ModRefAnalysis::dumpLoadToStoreMap() {
    debugs << "### LoadToStoreMap ###\n";

    for (auto i : loadToStoreMap) {
        Instruction *load = i.first;
        InstructionSet &stores = i.second;

        dumpInst(load);
        for (Instruction *store : stores) {
            dumpInst(store, "\t");
        }
    }
    debugs << "\n";
}

void ModRefAnalysis::dumpLoadToModInfoMap() {
    debugs << "### LoadToModInfoMap ###\n";

    for (auto i : loadToModInfoMap) {
        Instruction *load = i.first;
        set<ModInfo> &modInfos = i.second;

        dumpInst(load);
        for (ModInfo modInfo : modInfos) {
            dumpModInfo(modInfo, "\t");
        }
    }
    debugs << "\n";
}

void ModRefAnalysis::dumpModInfoToStoreMap() {
    debugs << "### ModInfoToStoreMap ###\n";

    for (auto i : modInfoToStoreMap) {
        const ModInfo &modInfo = i.first;
        InstructionSet &stores = i.second;

        dumpModInfo(modInfo);
        for (Instruction *store : stores) {
            dumpInst(store, "\t");
        }
    }
    debugs << "\n";
}

void ModRefAnalysis::dumpModInfoToIdMap() {
    debugs << "### ModInfoToIdMap ###\n";

    for (auto i : modInfoToIdMap) {
        const ModInfo &modInfo = i.first;
        uint32_t id = i.second;
        
        dumpModInfo(modInfo);
        debugs << "id: " << id << "\n";
    }
    debugs << "\n";
}

void ModRefAnalysis::dumpOverridingStores() {
    debugs << "### Overriding Stores ###\n";

    for (Instruction *inst : overridingStores) {
        dumpInst(inst);
    }
    debugs << "\n";
}

void ModRefAnalysis::dumpInst(Instruction *inst, const char *prefix) {
    Function *f = inst->getParent()->getParent();

    debugs << prefix << "[" << f->getName() << "]";
    inst->print(debugs);
    debugs << "\n";
}

void ModRefAnalysis::dumpModInfo(const ModInfo &modInfo, const char *prefix) {
    Function *f = modInfo.first;
    AllocSite allocSite = modInfo.second;

    const Value *value = allocSite.first;
    uint64_t offset = allocSite.second;

    debugs << prefix << "function: " << f->getName() << "\n";
    debugs << prefix << "allocation site: "; value->print(debugs); debugs << "\n";
    debugs << prefix << "offset: " << offset << "\n";
}
