#include <llvm/IR/Function.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Pass.h>
#include <llvm/Support/raw_ostream.h>
#include <list>
#include <iostream>

#include "Dataflow.h"

using namespace llvm;


typedef std::map<Value*,std::set<Value*>> Pointer2Set;

struct PointerInfo {
    Pointer2Set p2set;
    PointerInfo() : p2set() {}
    PointerInfo(const PointerInfo &info) : p2set(info.p2set) {}

    bool operator==(const PointerInfo &info) const {
        return p2set == info.p2set;
    }
};
/*
inline raw_ostream &operator<<(raw_ostream &out, const PointerInfo &info) {
    for (std::pair<Value *,std::set<Value*>>::iterator ii = info.p2set.begin(),
                                           ie = info.p2set.end();
         ii != ie; ++ii) {

    }
    return out;
}
*/


class FuncPtrVisitor : public DataflowVisitor<struct PointerInfo> {
public:
    std::map<Function*,PointerInfo> arg_p2s;
    std::map<int, std::list<Function *>> result;  // call指令对应到哪些函数
    FuncPtrVisitor() :result(),arg_p2s() {}
    void merge(PointerInfo *dest, const PointerInfo &src) override {
        for (Pointer2Set::const_iterator ii = src.p2set.begin(),
                ie = src.p2set.end();
                ii != ie; ++ii) {
            std::set<Value*> value_set = ii->second;
            for(std::set<Value*>::iterator vi = value_set.begin(),
                    ve = value_set.end();
                    vi != ve;++vi){
                dest->p2set[ii->first].insert(*vi);
            }
        }
    }


    PointerInfo getArgs(Function* fn){
        return arg_p2s[fn];
    }

    void mergeInputDF(Function* fn,BasicBlock* bb,PointerInfo* bbinval){
        //是函数第一个block，合并所有参数的pts
        merge(bbinval,getArgs(fn));
    }

    void compDFVal(Instruction *inst, PointerInfo* dfval) override {
        if (isa<DbgInfoIntrinsic>(inst)) return;
        if (CallInst *callInst = dyn_cast<CallInst>(inst)){
            std::cout<<"CallInst:"<<callInst->getDebugLoc().getLine()<<std::endl;
            callInst->dump();
            handleCallInst(callInst,dfval);
        }

    }

    void handleCallInst(CallInst* callInst,PointerInfo* dfval){
        int line = callInst->getDebugLoc().getLine();
        //得出所有可能调用的函数
        if (Function *func = callInst->getCalledFunction()) {  // 最简单的直接函数调用
            result[line].push_back(func);
        } else if (Value *value = callInst->getCalledOperand()) {
            
        } else {
            errs() << "ERROR\n";
        }
        //计算所有参数的pts，放进arg_p2s
    }

    void printResult(){
        for (std::map<int, std::list<Function *>>::iterator it = result.begin();
             it != result.end(); ++it) {
            errs() << it->first << " : ";
            it->second.sort();
            it->second.unique();
            std::list<Function *>::iterator funcEnd = it->second.end();
            --funcEnd;
            for (std::list<Function *>::iterator func = it->second.begin();
                 func != funcEnd; ++func) {
                errs() << (*func)->getName() << ',';
            }
            errs() << (*funcEnd)->getName() << '\n';
        }
    }
};

