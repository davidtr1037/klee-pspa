//===-- KModule.cpp -------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "KModule"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/Support/ErrorHandling.h"

#include "Passes.h"

#include "klee/Config/Version.h"
#include "klee/Interpreter.h"
#include "klee/Internal/Module/Cell.h"
#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Support/Debug.h"
#include "klee/Internal/Support/ModuleUtil.h"

#include "Util/BreakConstantExpr.h"

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/DataLayout.h"
#include <llvm/IR/Dominators.h>
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"

#if LLVM_VERSION_CODE < LLVM_VERSION(3, 5)
#include "llvm/Support/CallSite.h"
#else
#include "llvm/IR/CallSite.h"
#endif

#include "klee/Internal/Module/LLVMPassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/Path.h"
#include "llvm/Transforms/Scalar.h"

#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Analysis/LoopInfo.h>

#include <sstream>
#include <stack>
#include <set>

using namespace llvm;
using namespace klee;

namespace {
  enum SwitchImplType {
    eSwitchTypeSimple,
    eSwitchTypeLLVM,
    eSwitchTypeInternal
  };
    
  cl::opt<bool>
  NoTruncateSourceLines("no-truncate-source-lines",
                        cl::desc("Don't truncate long lines in the output source"));

  cl::opt<bool>
  OutputSource("output-source",
               cl::desc("Write the assembly for the final transformed source"),
               cl::init(true));

  cl::opt<bool>
  OutputModule("output-module",
               cl::desc("Write the bitcode for the final transformed module"),
               cl::init(false));

  cl::opt<SwitchImplType>
  SwitchType("switch-type", cl::desc("Select the implementation of switch"),
             cl::values(clEnumValN(eSwitchTypeSimple, "simple", 
                                   "lower to ordered branches"),
                        clEnumValN(eSwitchTypeLLVM, "llvm", 
                                   "lower using LLVM"),
                        clEnumValN(eSwitchTypeInternal, "internal", 
                                   "execute switch internally")
                        KLEE_LLVM_CL_VAL_END),
             cl::init(eSwitchTypeInternal));
  
  cl::opt<bool>
  DebugPrintEscapingFunctions("debug-print-escaping-functions", 
                              cl::desc("Print functions whose address is taken."));
}

KModule::KModule(Module *_module) 
  : module(_module),
    targetData(new DataLayout(module)),
    infos(0),
    constantTable(0) {
}

KModule::~KModule() {
  delete[] constantTable;
  delete infos;

  for (std::vector<KFunction*>::iterator it = functions.begin(), 
         ie = functions.end(); it != ie; ++it)
    delete *it;

  for (std::map<const llvm::Constant*, KConstant*>::iterator it=constantMap.begin(),
      itE=constantMap.end(); it!=itE;++it)
    delete it->second;

  delete targetData;
  delete module;
}

/***/

namespace llvm {
extern void Optimize(Module *, const std::string &EntryPoint);
}

// what a hack
static Function *getStubFunctionForCtorList(Module *m,
                                            GlobalVariable *gv, 
                                            std::string name) {
  assert(!gv->isDeclaration() && !gv->hasInternalLinkage() &&
         "do not support old LLVM style constructor/destructor lists");

  std::vector<Type *> nullary;

  Function *fn = Function::Create(FunctionType::get(Type::getVoidTy(m->getContext()),
						    nullary, false),
				  GlobalVariable::InternalLinkage, 
				  name,
                              m);
  BasicBlock *bb = BasicBlock::Create(m->getContext(), "entry", fn);
  
  // From lli:
  // Should be an array of '{ int, void ()* }' structs.  The first value is
  // the init priority, which we ignore.
  ConstantArray *arr = dyn_cast<ConstantArray>(gv->getInitializer());
  if (arr) {
    for (unsigned i=0; i<arr->getNumOperands(); i++) {
      ConstantStruct *cs = cast<ConstantStruct>(arr->getOperand(i));
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 5)
      // There is a third *optional* element in global_ctor elements (``i8
      // @data``).
      assert((cs->getNumOperands() == 2 || cs->getNumOperands() == 3) &&
             "unexpected element in ctor initializer list");
#else
      assert(cs->getNumOperands()==2 && "unexpected element in ctor initializer list");
#endif
      Constant *fp = cs->getOperand(1);      
      if (!fp->isNullValue()) {
        if (llvm::ConstantExpr *ce = dyn_cast<llvm::ConstantExpr>(fp))
          fp = ce->getOperand(0);

        if (Function *f = dyn_cast<Function>(fp)) {
	  CallInst::Create(f, "", bb);
        } else {
          assert(0 && "unable to get function pointer from ctor initializer list");
        }
      }
    }
  }
  
  ReturnInst::Create(m->getContext(), bb);

  return fn;
}

static void injectStaticConstructorsAndDestructors(Module *m) {
  GlobalVariable *ctors = m->getNamedGlobal("llvm.global_ctors");
  GlobalVariable *dtors = m->getNamedGlobal("llvm.global_dtors");
  
  if (ctors || dtors) {
    Function *mainFn = m->getFunction("main");
    if (!mainFn)
      klee_error("Could not find main() function.");

    if (ctors)
    CallInst::Create(getStubFunctionForCtorList(m, ctors, "klee.ctor_stub"),
		     "", &*(mainFn->begin()->begin()));
    if (dtors) {
      Function *dtorStub = getStubFunctionForCtorList(m, dtors, "klee.dtor_stub");
      for (Function::iterator it = mainFn->begin(), ie = mainFn->end();
           it != ie; ++it) {
        if (isa<ReturnInst>(it->getTerminator()))
	  CallInst::Create(dtorStub, "", it->getTerminator());
      }
    }
  }
}

void KModule::addInternalFunction(const char* functionName){
  Function* internalFunction = module->getFunction(functionName);
  if (!internalFunction) {
    KLEE_DEBUG(klee_warning(
        "Failed to add internal function %s. Not found.", functionName));
    return ;
  }
  KLEE_DEBUG(klee_message("Added function %s.",functionName));
  internalFunctions.insert(internalFunction);
}

void KModule::prepare(const Interpreter::ModuleOptions &opts,
                      InterpreterHandler *ih) {
  // Inject checks prior to optimization... we also perform the
  // invariant transformations that we will end up doing later so that
  // optimize is seeing what is as close as possible to the final
  // module.
  LegacyLLVMPassManagerTy pm;
  pm.add(new RaiseAsmPass());
  // This pass will scalarize as much code as possible so that the Executor
  // does not need to handle operands of vector type for most instructions
  // other than InsertElementInst and ExtractElementInst.
  //
  // NOTE: Must come before division/overshift checks because those passes
  // don't know how to handle vector instructions.
  if (opts.CheckDivZero) pm.add(new DivCheckPass());
  if (opts.CheckOvershift) pm.add(new OvershiftCheckPass());
  // FIXME: This false here is to work around a bug in
  // IntrinsicLowering which caches values which may eventually be
  // deleted (via RAUW). This can be removed once LLVM fixes this
  // issue.
  pm.add(new IntrinsicCleanerPass(*targetData, false));
  pm.run(*module);

  if (opts.Optimize)
    Optimize(module, opts.EntryPoint);

  // FIXME: Missing force import for various math functions.

  // FIXME: Find a way that we can test programs without requiring
  // this to be linked in, it makes low level debugging much more
  // annoying.

  SmallString<128> LibPath(opts.LibraryDir);
  llvm::sys::path::append(LibPath,
      "kleeRuntimeIntrinsic.bc"
    );
  module = linkWithLibrary(module, LibPath.str());

  // Add internal functions which are not used to check if instructions
  // have been already visited
  if (opts.CheckDivZero)
    addInternalFunction("klee_div_zero_check");
  if (opts.CheckOvershift)
    addInternalFunction("klee_overshift_check");


  // Needs to happen after linking (since ctors/dtors can be modified)
  // and optimization (since global optimization can rewrite lists).
  injectStaticConstructorsAndDestructors(module);

  // Finally, run the passes that maintain invariants we expect during
  // interpretation. We run the intrinsic cleaner just in case we
  // linked in something with intrinsics but any external calls are
  // going to be unresolved. We really need to handle the intrinsics
  // directly I think?
  LegacyLLVMPassManagerTy pm3;
  pm3.add(createCFGSimplificationPass());
  pm3.add(createScalarizerPass());
  switch(SwitchType) {
  case eSwitchTypeInternal: break;
  case eSwitchTypeSimple: pm3.add(new LowerSwitchPass()); break;
  case eSwitchTypeLLVM:  pm3.add(createLowerSwitchPass()); break;
  default: klee_error("invalid --switch-type");
  }
  InstructionOperandTypeCheckPass *operandTypeCheckPass =
      new InstructionOperandTypeCheckPass();
  pm3.add(new IntrinsicCleanerPass(*targetData));
  pm3.add(new PhiCleanerPass());
  pm3.add(operandTypeCheckPass);
  pm3.run(*module);

  // Enforce the operand type invariants that the Executor expects.  This
  // implicitly depends on the "Scalarizer" pass to be run in order to succeed
  // in the presence of vector instructions.
  if (!operandTypeCheckPass->checkPassed()) {
    klee_error("Unexpected instruction operand types detected");
  }

  // Write out the .ll assembly file. We truncate long lines to work
  // around a kcachegrind parsing bug (it puts them on new lines), so
  // that source browsing works.
  if (OutputSource) {
    llvm::raw_fd_ostream *os = ih->openOutputFile("assembly.ll");
    assert(os && !os->has_error() && "unable to open source output");

    // We have an option for this in case the user wants a .ll they
    // can compile.
    if (NoTruncateSourceLines) {
      *os << *module;
    } else {
      std::string string;
      llvm::raw_string_ostream rss(string);
      rss << *module;
      rss.flush();
      const char *position = string.c_str();

      for (;;) {
        const char *end = index(position, '\n');
        if (!end) {
          *os << position;
          break;
        } else {
          unsigned count = (end - position) + 1;
          if (count<255) {
            os->write(position, count);
          } else {
            os->write(position, 254);
            *os << "\n";
          }
          position = end+1;
        }
      }
    }
    delete os;
  }

  /* running the SVF passes explicitly,
     in order to avoid missing instructions */
  BreakConstantGEPs p1;
  p1.runOnModule(*module);

  UnifyFunctionExitNodes p2;
  for (Function &f : *module) {
    if (!f.isDeclaration()) {
      p2.runOnFunction(f);
    }
  }

  LegacyLLVMPassManagerTy passManager;
  passManager.add(new UnusedValuesRemovalPass(opts.EntryPoint));
  passManager.run(*module);

  if (OutputModule) {
    llvm::raw_fd_ostream *f = ih->openOutputFile("final.bc");
    WriteBitcodeToFile(module, *f);
    delete f;
  }

  /* Build shadow structures */

  infos = new InstructionInfoTable(module);  
  
  for (Module::iterator it = module->begin(), ie = module->end();
       it != ie; ++it) {
    if (it->isDeclaration())
      continue;

    Function *fn = &*it;
    KFunction *kf = new KFunction(fn, this);
    
    for (unsigned i=0; i<kf->numInstructions; ++i) {
      KInstruction *ki = kf->instructions[i];
      ki->info = &infos->getInfo(ki->inst);
    }

    functions.push_back(kf);
    functionMap.insert(std::make_pair(fn, kf));
  }

  /* Compute various interesting properties */

  for (std::vector<KFunction*>::iterator it = functions.begin(), 
         ie = functions.end(); it != ie; ++it) {
    KFunction *kf = *it;
    if (functionEscapes(kf->function))
      escapingFunctions.insert(kf->function);
  }

  /* TODO: add docs */
  for (KFunction *kf : functions) {
    markNonRelevantStores(kf);
  }

  if (DebugPrintEscapingFunctions && !escapingFunctions.empty()) {
    llvm::errs() << "KLEE: escaping functions: [";
    for (std::set<Function*>::iterator it = escapingFunctions.begin(), 
         ie = escapingFunctions.end(); it != ie; ++it) {
      llvm::errs() << (*it)->getName() << ", ";
    }
    llvm::errs() << "]\n";
  }
}

void KModule::markNonRelevantStores(KFunction *kf) {
  bool isCallingFunction = false;

  for (unsigned i = 0; i < kf->numInstructions; ++i) {
    KInstruction *ki = kf->instructions[i];
    CallInst *callInst = dyn_cast<CallInst>(ki->inst);
    if (callInst) {
      isCallingFunction = true;
      break;
    }
  }

  if (!isCallingFunction) {
    markStoresInNonCallingFunction(kf);
  }
}

void KModule::markStoresInCallingFunction(KFunction *kf) {
  for (unsigned i = 0; i < kf->numInstructions; ++i) {
    KInstruction *ki = kf->instructions[i];
    StoreInst *storeInst = dyn_cast<StoreInst>(ki->inst);
    if (!storeInst) {
      continue;
    }

    Value *pointer = storeInst->getPointerOperand();
    AllocaInst *allocaInst = dyn_cast<AllocaInst>(pointer);
    if (!allocaInst) {
      continue;
    }

    if (!isEscapingVariable(allocaInst)) {
      ki->isRelevant = false;
    }
  }
}

bool KModule::isEscapingVariable(Instruction *allocaInst) {
  std::stack<Value *> worklist;
  std::set<Value *> visited;

  /* initialize */
  worklist.push(allocaInst);

  while (!worklist.empty()) {
    Value *inst = worklist.top();
    worklist.pop();

    if (visited.find(inst) != visited.end()) {
      continue;
    }

    for (auto i = inst->user_begin(); i != inst->user_end(); i++) {
      Value *user = *i;
      if (!isa<Instruction>(user)) {
        assert(false);
      }

      if (isa<CallInst>(user)) {
        return true;
      }

      if (isa<StoreInst>(user)) {
        StoreInst *userStore = dyn_cast<StoreInst>(user);
        if (userStore->getValueOperand() == inst) {
          return true;
        }
      }

      worklist.push(user);
    }

    visited.insert(inst);
  }

  return false;
}

void KModule::markStoresInNonCallingFunction(KFunction *kf) {
  for (unsigned i = 0; i < kf->numInstructions; ++i) {
    KInstruction *ki = kf->instructions[i];
    StoreInst *storeInst = dyn_cast<StoreInst>(ki->inst);
    if (!storeInst) {
      continue;
    }

    Value *pointer = storeInst->getPointerOperand();
    AllocaInst *allocaInst = dyn_cast<AllocaInst>(pointer);
    if (allocaInst) {
      ki->isRelevant = false;
    }
  }
}

KConstant* KModule::getKConstant(const Constant *c) {
  std::map<const llvm::Constant*, KConstant*>::iterator it = constantMap.find(c);
  if (it != constantMap.end())
    return it->second;
  return NULL;
}

unsigned KModule::getConstantID(Constant *c, KInstruction* ki) {
  KConstant *kc = getKConstant(c);
  if (kc)
    return kc->id;  

  unsigned id = constants.size();
  kc = new KConstant(c, id, ki);
  constantMap.insert(std::make_pair(c, kc));
  constants.push_back(c);
  return id;
}

/***/

KConstant::KConstant(llvm::Constant* _ct, unsigned _id, KInstruction* _ki) {
  ct = _ct;
  id = _id;
  ki = _ki;
}

/***/

static int getOperandNum(Value *v,
                         std::map<Instruction*, unsigned> &registerMap,
                         KModule *km,
                         KInstruction *ki) {
  if (Instruction *inst = dyn_cast<Instruction>(v)) {
    return registerMap[inst];
  } else if (Argument *a = dyn_cast<Argument>(v)) {
    return a->getArgNo();
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 6)
    // Metadata is no longer a Value
  } else if (isa<BasicBlock>(v) || isa<InlineAsm>(v)) {
#else
  } else if (isa<BasicBlock>(v) || isa<InlineAsm>(v) ||
             isa<MDNode>(v)) {
#endif
    return -1;
  } else {
    assert(isa<Constant>(v));
    Constant *c = cast<Constant>(v);
    return -(km->getConstantID(c, ki) + 2);
  }
}

KFunction::KFunction(llvm::Function *_function,
                     KModule *km) 
  : function(_function),
    numArgs(function->arg_size()),
    numInstructions(0),
    trackCoverage(true) {
  for (llvm::Function::iterator bbit = function->begin(), 
         bbie = function->end(); bbit != bbie; ++bbit) {
    BasicBlock *bb = &*bbit;
    basicBlockEntry[bb] = numInstructions;
    numInstructions += bb->size();
  }

  instructions = new KInstruction*[numInstructions];

  std::map<Instruction*, unsigned> registerMap;

  // The first arg_size() registers are reserved for formals.
  unsigned rnum = numArgs;
  for (llvm::Function::iterator bbit = function->begin(), 
         bbie = function->end(); bbit != bbie; ++bbit) {
    for (llvm::BasicBlock::iterator it = bbit->begin(), ie = bbit->end();
         it != ie; ++it)
      registerMap[&*it] = rnum++;
  }
  numRegisters = rnum;
  
  unsigned i = 0;
  for (llvm::Function::iterator bbit = function->begin(), 
         bbie = function->end(); bbit != bbie; ++bbit) {
    for (llvm::BasicBlock::iterator it = bbit->begin(), ie = bbit->end();
         it != ie; ++it) {
      KInstruction *ki;

      switch(it->getOpcode()) {
      case Instruction::GetElementPtr:
      case Instruction::InsertValue:
      case Instruction::ExtractValue:
        ki = new KGEPInstruction(); break;
      default:
        ki = new KInstruction(); break;
      }

      Instruction *inst = &*it;
      ki->inst = inst;
      ki->dest = registerMap[inst];

      if (isa<CallInst>(it) || isa<InvokeInst>(it)) {
        CallSite cs(inst);
        unsigned numArgs = cs.arg_size();
        ki->operands = new int[numArgs+1];
        ki->operands[0] = getOperandNum(cs.getCalledValue(), registerMap, km,
                                        ki);
        for (unsigned j=0; j<numArgs; j++) {
          Value *v = cs.getArgument(j);
          ki->operands[j+1] = getOperandNum(v, registerMap, km, ki);
        }
      } else {
        unsigned numOperands = it->getNumOperands();
        ki->operands = new int[numOperands];
        for (unsigned j=0; j<numOperands; j++) {
          Value *v = it->getOperand(j);
          ki->operands[j] = getOperandNum(v, registerMap, km, ki);
        }
      }

      instructions[i++] = ki;
    }
  }

  std::set<Instruction *> result;
  collectLoopInfo(function, result);
}

void KFunction::collectLoopInfo(Function *f,
                                std::set<Instruction *> &result) {
  DominatorTree dtree(*f);
  LoopInfo loopInfo(dtree);

  for (Loop *loop : loopInfo) {
    BasicBlock *header = loop->getHeader();
    Instruction *inst = &*header->begin();
    for (Loop *subLoop : loop->getSubLoops()) {
      BasicBlock *subHeader = subLoop->getHeader();
      Instruction *subInst = &*subHeader->begin();
      loops[inst].insert(subInst);
    }
  }
}

KFunction::~KFunction() {
  for (unsigned i=0; i<numInstructions; ++i)
    delete instructions[i];
  delete[] instructions;
}
