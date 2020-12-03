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
        merge(bbinval,getArgs(fn));
    }

    void compDFVal(Instruction *inst, PointerInfo* dfval) override {
        if (isa<DbgInfoIntrinsic>(inst)) return;
        inst->dump();
        if (CallInst *callInst = dyn_cast<CallInst>(inst)){
            std::cout<<"CallInst:"<<callInst->getDebugLoc().getLine()<<std::endl;
            handleCallInst(callInst,dfval);
        } else if (true) {
        }

    }

    void handleCallInst(CallInst* callInst,PointerInfo* dfval){
        int line = callInst->getDebugLoc().getLine();

        //得出所有可能调用的函数
        std::set<Function*> callees;
        callees = getFuncByValue(callInst->getCalledOperand(),dfval);
        for(auto ci = callees.begin(),ce = callees.end();ci != ce;ci++){
            Function* func = *ci;
            result[line].push_back(func);
        }

        //计算所有参数的pts
        PointerInfo caller_args;
        for(unsigned i = 0; i < callInst->getNumArgOperands();i++){
            Value* arg = callInst->getArgOperand(i);
            if(arg->getType()->isPointerTy()){
                caller_args.p2set[arg].insert(dfval->p2set[arg].begin(),dfval->p2set[arg].end());
            }
        }
        if(caller_args.p2set.empty()){ //没有指针参数的话就直接返回
            printf("Empty!\n");
            return;
        }
        //放进每个callee对应参数的arg_p2s



    }


    std::set<Function*> getFuncByValue_work(Value* value,PointerInfo* dfval){
        std::set<Function*> res;
        if(Function* func = dyn_cast<Function>(value)){
            res.insert(func);
            return res;
        }
        for(auto vi = dfval->p2set[value].begin(),ve = dfval->p2set[value].end();
                vi != ve; vi++){
            std::set<Function*> r = getFuncByValue_work(*vi,dfval);
            res.insert(r.begin(),r.end());
        }
        return res;
    }

    std::set<Function*> getFuncByValue(Value* value,PointerInfo* dfval){
        std::set<Function*> res;
        //沿着控制流找Function
        res = getFuncByValue_work(value,dfval);
        for(auto fi = res.begin(),fe = res.end(); fi != fe;fi++){
            Function* func = *fi;
            res.insert(func);
        }
        return res;
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

