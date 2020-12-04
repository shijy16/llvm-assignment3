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
    Pointer2Set p2set_field;
    PointerInfo() : p2set(),p2set_field() {}
    PointerInfo(const PointerInfo &info) : p2set(info.p2set),p2set_field(info.p2set_field) {}

    bool operator==(const PointerInfo &info) const {
        return p2set == info.p2set && p2set_field == info.p2set_field;
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
    std::map<Function*,std::set<Value*>> ret_p2s;
    std::map<int, std::list<Function *>> result;  // call指令对应到哪些函数
    bool change = false;
    FuncPtrVisitor() :result(),arg_p2s(),ret_p2s() {}
    void merge(PointerInfo *dest, const PointerInfo &src) override {
        for (Pointer2Set::const_iterator ii = src.p2set.begin(),
                ie = src.p2set.end();
                ii != ie; ++ii) {
            std::set<Value*> value_set = ii->second;
            for(std::set<Value*>::iterator vi = value_set.begin(),
                    ve = value_set.end();
                    vi != ve;++vi){
                dest->p2set[ii->first].insert(*vi);
                printf("MERGE!!!!!\n");
            }
        }
        for (Pointer2Set::const_iterator ii = src.p2set_field.begin(),
                ie = src.p2set_field.end();
                ii != ie; ++ii) {
            std::set<Value*> value_set = ii->second;
            for(std::set<Value*>::iterator vi = value_set.begin(),
                    ve = value_set.end();
                    vi != ve;++vi){
                dest->p2set_field[ii->first].insert(*vi);
                printf("MERGE!!!!!\n");
            }
        }
    }


    PointerInfo getArgs(Function* fn){
        errs()<<fn->getName() << "\n";
        return arg_p2s[fn];
    }

    void mergeInputDF(Function* fn,BasicBlock* bb,PointerInfo* bbinval){
        printf("before merge:%ld\n",bbinval->p2set.size());
        PointerInfo in_args_p2s = getArgs(fn);
        printf("GET arg:%ld\n",in_args_p2s.p2set.size());
        merge(bbinval,in_args_p2s);
        printf("after merge:%ld\n",bbinval->p2set.size());
    }

    void compDFVal(Instruction *inst, PointerInfo* dfval) override {
        if (isa<DbgInfoIntrinsic>(inst)) return;
        if (isa<IntrinsicInst>(inst)) return;
        if (CallInst *callInst = dyn_cast<CallInst>(inst)){
            //std::cout<<"CallInst:"<<callInst->getDebugLoc().getLine()<<std::endl;
            handleCallInst(callInst,dfval);
        } else if (PHINode* phyNode = dyn_cast<PHINode>(inst)) {
            //std::cout<<"PHINode:"<<inst->getDebugLoc().getLine()<<std::endl;
            handlePHINode(phyNode,dfval);
        } else if (LoadInst* loadInst = dyn_cast<LoadInst>(inst)){
            //std::cout<<"LoadInst:"<<inst->getDebugLoc().getLine()<<std::endl;
            handleLoadInst(loadInst,dfval);
        } else if (StoreInst* storeInst = dyn_cast<StoreInst>(inst)){
            //std::cout<<"StoreInst:"<<inst->getDebugLoc().getLine()<<std::endl;
            handleStoreInst(storeInst,dfval);
        } else if (GetElementPtrInst* getElementPtrInst = dyn_cast<GetElementPtrInst>(inst)){
            //std::cout<<"GetElementPtrInst:"<<inst->getDebugLoc().getLine()<<std::endl;
            handleGetElementPtrInst(getElementPtrInst,dfval);
        } else if (ReturnInst* returnInst = dyn_cast<ReturnInst>(inst)){
            handleReturnInst(returnInst,dfval);
        }
    }

    void handleReturnInst(ReturnInst* retInst,PointerInfo* dfval){
        Function* func = retInst->getFunction();
        Value* retValue = retInst->getReturnValue();
        if(!retValue || !retValue->getType()->isPointerTy()) return;
        //把所有返回值放进ret_p2s
        ret_p2s[func].insert(dfval->p2set[retValue].begin(),dfval->p2set[retValue].end());
    }

    void handleGetElementPtrInst(GetElementPtrInst* gepInst,PointerInfo* dfval){
        Value* ptr = gepInst->getPointerOperand();
        dfval->p2set[gepInst].clear();
        if(dfval->p2set[ptr].empty()){
            dfval->p2set[gepInst].insert(ptr);
        } else {
            dfval->p2set[gepInst].insert(dfval->p2set[ptr].begin(),dfval->p2set[ptr].end());
        }
    }

    void handleLoadInst(LoadInst* loadInst,PointerInfo* dfval){
        errs()<<"Load:\n";
        loadInst->dump();
        Value* target_value = loadInst->getPointerOperand();
        dfval->p2set[loadInst].clear();
        std::set<Value*> values;
        if(GetElementPtrInst* gepInst = dyn_cast<GetElementPtrInst>(target_value)){
            Value* ptr = gepInst->getPointerOperand();
            values = dfval->p2set[ptr];
            if(dfval->p2set[ptr].empty()){
                values = dfval->p2set_field[ptr];
                dfval->p2set[loadInst].insert(values.begin(),values.end());
            } else {
                values = dfval->p2set[ptr];
                for(auto vi = values.begin(),ve = values.end();vi != ve; vi++){
                    Value* v = *vi;
                    dfval->p2set[loadInst].insert(dfval->p2set_field[v].begin(),dfval->p2set_field[v].end());
                }
            }
        } else {
            values = dfval->p2set[target_value];
            dfval->p2set[loadInst].insert(values.begin(),values.end());
        }

    }


    void handleStoreInst(StoreInst* storeInst,PointerInfo* dfval){
        errs()<<"Store:\n";
        Value* store_value = storeInst->getValueOperand();
        Value* target_value = storeInst->getPointerOperand();
        storeInst->dump();
        //获取要存储的值的values
        std::set<Value*> store_values;
        if(dfval->p2set[store_value].empty()){
            store_values.insert(store_value);
        } else {
            store_values.insert(dfval->p2set[store_value].begin(),dfval->p2set[store_value].end());
        }
        /*
        if(store_value->getType()->isPointerTy()){
            if(Function* func = dyn_cast<Function>(store_value)){
                store_values.insert(store_value);
            } else {
                store_values.insert(dfval->p2set[store_value].begin(),dfval->p2set[store_value].end());
            }
        } else {
            //store_values.insert(store_value);
        }
        */
        //存入目标value的set
        dfval->p2set[target_value].clear();
        //如果目标value是取结构体指针或者数组指针的指令，使用field
        if(GetElementPtrInst* gepInst = dyn_cast<GetElementPtrInst>(target_value)){
            Value* ptr = gepInst->getPointerOperand();
            if(dfval->p2set[ptr].empty()){
                dfval->p2set_field[ptr].clear();
                dfval->p2set_field[ptr].insert(store_values.begin(),store_values.end());
                ptr->dump();
                errs()<<dfval->p2set_field[ptr].size() <<"\n";
            } else {
                std::set<Value*> values = dfval->p2set[ptr];
                for(auto vi = values.begin(),ve = values.end();vi != ve; vi++){
                    Value* v = *vi;
                    dfval->p2set_field[v].clear();
                    dfval->p2set_field[v].insert(store_values.begin(),store_values.end());
                }
            }
        } else { //否则直接存
            dfval->p2set[target_value].clear();
            dfval->p2set[target_value].insert(store_values.begin(),store_values.end());
            errs()<<dfval->p2set[target_value].size()<<"\n";
        }
        errs()<<"store end\n";

    }

    void handlePHINode(PHINode* phyNode,PointerInfo* dfval){
        errs()<<"handle handlePHINode\n";
        dfval->p2set[phyNode].clear();
        for(Value* v : phyNode->incoming_values()){
            if(Function* func = dyn_cast<Function>(v)){
                dfval->p2set[phyNode].insert(func);
                errs()<<"handlePHINode:"<<dfval->p2set[phyNode].size()<<"\n";
            } else if(v->getType()->isPointerTy()){
                dfval->p2set[phyNode].insert(dfval->p2set[v].begin(),dfval->p2set[v].end());
            }
        }
        errs()<<"handle handlePHINode finished"<<"\n";
    }




    void handleCallInst(CallInst* callInst,PointerInfo* dfval){

        std::map<Function*,PointerInfo> old_arg_p2s = arg_p2s;
        int line = callInst->getDebugLoc().getLine();
        result[line].clear();
        errs()<<"Call:\n";

        callInst->dump();
        //得出所有可能调用的函数
        std::set<Function*> callees;
        callees = getFuncByValue(callInst->getCalledOperand(),dfval);
        for(auto ci = callees.begin(),ce = callees.end();ci != ce;ci++){
            errs()<<"push\n";
            Function* func = *ci;
            result[line].push_back(func);
        }

        //计算所有参数的pts
        PointerInfo caller_args;
        for(unsigned i = 0; i < callInst->getNumArgOperands();i++){
            Value* arg = callInst->getArgOperand(i);
            if(arg->getType()->isPointerTy()){
                if(Function* func = dyn_cast<Function>(arg)){
                    caller_args.p2set[arg].insert(func);
                } else {
                    caller_args.p2set[arg].insert(dfval->p2set[arg].begin(),dfval->p2set[arg].end());
                    printf("insert %ld\n",caller_args.p2set[arg].size());
                }
            }
        }
        if(caller_args.p2set.empty()){ //没有指针参数的话就直接返回
            printf("Empty!\n");
            return;
        }
        //放进每个callee对应参数的arg_p2s
        for(auto ci = callees.begin(),ce = callees.end();ci != ce;ci++){
            Function* callee = *ci;
            for(unsigned i = 0; i < callInst->getNumArgOperands();i++){
                Value* caller_arg = callInst->getArgOperand(i);
                if(caller_arg->getType()->isPointerTy()){
                    Value* callee_arg = callee->arg_begin() + i;
                    arg_p2s[callee].p2set[callee_arg].insert(caller_args.p2set[caller_arg].begin(),caller_args.p2set[caller_arg].end());
                    printf("insert args:%ld\n",caller_args.p2set.count(caller_arg));
                }
            }
        }

        // 获取返回值
        for(auto ci = callees.begin(),ce = callees.end();ci != ce;ci++){
            Function* callee = *ci;
            if(!ret_p2s[callee].empty()){
                dfval->p2set[callInst].insert(ret_p2s[callee].begin(),ret_p2s[callee].end());
            }
        }
        if(old_arg_p2s != arg_p2s){
            printf("change\n");
            change = true;
        }
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
        errs()<<"getFuncByValue:\n";
        value->dump();
        errs()<<dfval->p2set.size()<<" "<<dfval->p2set[value].size()<<"\n";
        //沿着控制流找Function
        res = getFuncByValue_work(value,dfval);
        for(auto fi = res.begin(),fe = res.end(); fi != fe;fi++){
            Function* func = *fi;
            res.insert(func);
        }
        if(res.empty()){
            errs()<<"nothing found\n";
        }
        return res;
    }


    void printResult(){
        errs()<<"result\n";
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

