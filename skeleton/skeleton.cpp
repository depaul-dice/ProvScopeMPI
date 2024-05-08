
/* #include <cassert> */
#include <string> 
#include <chrono>
#include <fstream>

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

#include "cfg.h"
#include "loops.h"

using namespace llvm;
using namespace std;
using namespace std::chrono;

microseconds __duration(0);

#define ASSERT(cond) if(!(cond)) {errs() << "assertion failed: " << #cond << '\n';}

namespace {
    struct bbprinter : public FunctionPass {
        static char ID;
        bbprinter() : FunctionPass(ID) {}
        ofstream looptreefile; 

        bool isEntryBlock(const BasicBlock &bb) {
            /* return &bb == &bb.getParent()->getEntryBlock(); */
            return pred_begin(&bb) == pred_end(&bb);
        }

        bool isExitBlock(const BasicBlock &bb) {
            return succ_begin(&bb) == succ_end(&bb);
        }
         
        void inline insertBeginning(string str, Function *printFunc, Function::iterator& bb, IRBuilder<>& Builder) {
            /* Value *strPtr = Builder.CreateGlobalStringPtr(str, "blockString"); */
            /* Builder.CreateCall(printFunc, strPtr); */
            ASSERT(!bb->empty());
            Builder.SetInsertPoint(&*bb, bb->getFirstInsertionPt());

            Value *strPtr = Builder.CreateGlobalStringPtr(str, "blockString");
            Builder.CreateCall(printFunc, strPtr);
        }

        void inline insertBeforeRet(string str, Function *printFunc, Function::iterator& bb, IRBuilder<>& Builder) {
            ASSERT(!bb->empty());
            Instruction *terminator = bb->getTerminator();
            ASSERT(isa<ReturnInst>(terminator));
            Builder.SetInsertPoint(terminator);
            Value *strPtr = Builder.CreateGlobalStringPtr(str, "blockString");
            Builder.CreateCall(printFunc, strPtr);
        }

        bool doInitialization(Module &M) override {
            cerr << "loop.dot opening\n";
            looptreefile.open("loops.dot");
            return false; // return false because we did not modify the module
        }

        bool doFinalization(Module &M) override {
            cerr << "loop.dot file created\n";
            looptreefile.close();
            return false; // return false because we did not modify the module
        }
        
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
            bool isEntry = false, isExit = false;
            cfg *graph = new cfg(F.getName().str());

            for(Function::iterator bb = F.begin(), e = F.end(); bb != e; ++bb){
                funcBBnum++;
                if(isEntryBlock(*bb) && isExitBlock(*bb)) {
                    graph->insertNode(F.getName().str() + ":entry:" + to_string(count));

                    bb->setName(F.getName() + ":entry:" + to_string(count++));
                    insertBeginning(bb->getName().str(), printFunc, bb, Builder);
                    string str = (F.getName() + ":exit:" + to_string(count++)).str();
                    insertBeforeRet(str, printFunc, bb, Builder);
                } else if (isEntryBlock(*bb)) {
                    bb->setName(F.getName() + ":entry:" + to_string(count++));
                    insertBeginning(bb->getName().str(), printFunc, bb, Builder);

                    graph->insertNode(bb->getName().str());
                } else if (isExitBlock(*bb)) {
                    string str = (F.getName() + ":neither:" + to_string(count++)).str();
                    insertBeginning(str, printFunc, bb, Builder);

                    graph->insertNode(F.getName().str() + ":exit:" + to_string(count));
                    bb->setName(F.getName() + ":exit:" + to_string(count++));
                    insertBeforeRet(bb->getName().str(), printFunc, bb, Builder);
                } else {
                    bb->setName(F.getName() + ":neither:" + to_string(count++));
                    insertBeginning(bb->getName().str(), printFunc, bb, Builder);

                    graph->insertNode(bb->getName().str());
                }
                /* errs() << "Basic Block name: " << bb->getName() << ", size: " << bb->size() << "\n"; */

            }

            for(auto &bb : F){
                for(auto *succ: successors(&bb)){
                    graph->insertEdge(bb.getName().str(), succ->getName().str());
                }
            }

            auto tok = high_resolution_clock::now();
            __duration += duration_cast<microseconds>(tok - tik);

            map<node *, node *> iloopHeaders{};
            fillIloopHeaders(graph, iloopHeaders);
            loopNode *root = createLoopTree(iloopHeaders, graph);
            /* root->print(loopfile); */
            root->print(looptreefile, F.getName().str());

            delete graph;
            delete root;

            errs() << F.getName() << ": " << __duration.count() << " microseconds\n";
            return true;
        }
    }; // end of struct bbprinter 
} // end anonymous namespace
char bbprinter::ID = 0;
static RegisterPass<bbprinter> X("bbprinter", "Basic Block Printer Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);
