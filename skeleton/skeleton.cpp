
#include <string> 

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

// adding to include IRBuilder
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/CFG.h"

// adding to include getFunction function
#include "llvm/IR/Value.h"
#include "llvm/IR/Instruction.h"
#include <chrono>

using namespace llvm;
using namespace std;
using namespace std::chrono;

microseconds __duration(0);

namespace {
    struct bbprinter : public FunctionPass {
        static char ID;
        bbprinter() : FunctionPass(ID) {}

        bool runOnFunction(Function &F) override {
            auto tik = high_resolution_clock::now();            
            LLVMContext& context = F.getContext();
            Module *m = F.getParent();
            Type *returnType = Type::getVoidTy(m->getContext());
            PointerType *argType = PointerType::get(Type::getInt8Ty(m->getContext()), 0);
            FunctionType *FuncType = FunctionType::get(returnType, argType, false);

            FunctionCallee funcCallee = m->getOrInsertFunction("printBBname", FuncType);
            Function *printFunc = cast<Function>(funcCallee.getCallee());

            IRBuilder<> Builder(context);
            unsigned funcBBnum = 0, count = 0;
            for(Function::iterator bb = F.begin(), e = F.end(); bb != e; ++bb){
                funcBBnum++;
                bb->setName(F.getName() + "_" + to_string(count++));
                /* errs() << "Basic Block name: " << bb->getName() << ", size: " << bb->size() << "\n"; */

                if(!bb->empty()) { // checking if bb has instructions
                    Builder.SetInsertPoint(&*bb, bb->getFirstInsertionPt());
                } else {
                    errs() << "empty bb found: " << bb->getName() << '\n';
                    continue;
                }

                string str = bb->getName().str();
                Value *strPtr = Builder.CreateGlobalStringPtr(str, "blockString");
                Builder.CreateCall(printFunc, strPtr);
            }
            auto tok = high_resolution_clock::now();
            __duration += duration_cast<microseconds>(tok - tik);
            errs() << "time: " << __duration.count() << "microseconds\n";
            return true;
        }
    }; // end of struct Hello
} // end anonymous namespace
char bbprinter::ID = 0;
static RegisterPass<bbprinter> X("bbprinter", "Basic Block Printer Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
