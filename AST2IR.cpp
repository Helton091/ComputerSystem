#include"AST2IR.hpp"
namespace IR{

Opcode T2O(Tok tok,ExprNode* type){
    if(dynamic_cast<BinaryExpr*>(type)){
        switch(tok){
        case Tok::ADD: return Opcode::ADD;
        case Tok::SUB: return Opcode::SUB;
        case Tok::STAR: return Opcode::MUL;
        case Tok::SLASH: return Opcode::DIV;
        case Tok::PERCENT: return Opcode::REM;
        case Tok::LT: return Opcode::LT;
        case Tok::LE: return Opcode::LE;
        case Tok::GT: return Opcode::GT;
        case Tok::GE: return Opcode::GE;
        case Tok::EQ: return Opcode::EQ;
        case Tok::NE: return Opcode::NE;
        default: throw std::runtime_error("cannot translate tok to opcode");
        }
    } else if(dynamic_cast<UnaryExpr*>(type)){
        switch(tok){
        case Tok::SUB: return Opcode::NEG;
        default: throw std::runtime_error("cannot translate tok to opcode");
        }
    } else {
        switch(tok){
        default: throw std::runtime_error("cannot translate tok to opcode");
        }
    }
    
}

std::unique_ptr<Module> AST2IR::translate(ProgramNode* program){
    module_ = std::make_unique<Module>();
    for(const std::unique_ptr<FunctionNode>& funcnode : program->functions){
        Function* func = module_->add_function(funcnode->name);
        for (size_t i = 0; i < funcnode->params.size(); ++i)
            func->add_arg();

    }
    for(const std::unique_ptr<FunctionNode>& funcnode : program->functions){
        gen_function(funcnode.get());
        
    }
    return std::move(module_);
}

void AST2IR::gen_function(FunctionNode* funcnode){
    temp_counter_ = 0;
    curr_func_ = module_->find_function(funcnode->name);
    curr_func_->add_block("entry");
    curr_bb_ = curr_func_->add_block("start");
    make_inst(curr_func_->entry,Opcode::JMP,VoidType::get(),"jmp",{curr_bb_});

    enter_scope();
    int temp_cnt = 0;
    for(const Param& arg : funcnode->params){
        Argument* new_arg =  curr_func_->args[temp_cnt].get();
        Instruction* addr = curr_func_->add_alloca(arg.name);
        make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{new_arg,addr});
        if(scope_stack_.back().find(arg.name) != scope_stack_.back().end()) throw std::runtime_error("same name " + arg.name + " in function " + curr_func_->name + "'s arguments list");
        scope_stack_.back()[arg.name] = addr;
        ++temp_cnt;
    }

    gen_stmt(funcnode->body.get());
    if(!curr_bb_->is_terminated()) make_inst(curr_bb_,Opcode::RET,VoidType::get(),"",{module_->get_const(0)});
    exit_scope();
}

void AST2IR::gen_stmt(StatementNode* stmt){
    if(auto bs = dynamic_cast<BlockStatement*>(stmt)){
        enter_scope();
        for(const auto& bbs : bs->statements){
            gen_stmt(bbs.get());
        }
        exit_scope();
    } else if(auto rs = dynamic_cast<ReturnStatement*>(stmt)){
        Value* ret_val = gen_expr(rs->expr.get());
        make_inst(curr_bb_, Opcode::RET, VoidType::get(),"",{ret_val});
    } else if(auto ds = dynamic_cast<DeclStmt*>(stmt)){
        std::unordered_map<std::string,Instruction*>& curr_stack =  scope_stack_.back();
        if(curr_stack.find(ds->name) != curr_stack.end()) throw std::runtime_error("redefined variable " + ds->name);
        Instruction* di = curr_func_->add_alloca(ds->name);
        curr_stack[ds->name] = di;
        if(ds->init){
            make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{gen_expr(ds->init.get()),di});
        } else {
            make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{module_->get_const(0),di});
        }
    } else if(auto es = dynamic_cast<ExprStmt*>(stmt)){
        gen_expr(es->expr.get());
    } else if(auto is = dynamic_cast<IfStmt*>(stmt)){
        Value* cond_v = gen_expr(is->cond.get());
        std::string bb1_name = new_block_name("_if_then_");
        BasicBlock* bb1 = curr_func_->add_block(bb1_name);
        if(is->else_branch){
            std::string bb2_name = new_block_name("_if_else_");
            BasicBlock* bb2 = curr_func_->add_block(bb2_name);
            std::string bbm_name = new_block_name("_if_end_");
            BasicBlock* bbm = curr_func_->add_block(bbm_name);
            if(!curr_bb_->is_terminated()) make_inst(curr_bb_,Opcode::BR,VoidType::get(),"",{cond_v,bb1,bb2});
            
            curr_bb_ = bb1;
            gen_stmt(is->then_branch.get());
            if(!curr_bb_->is_terminated()) make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{bbm});
            
            curr_bb_ = bb2;
            gen_stmt(is->else_branch.get());
            if(!curr_bb_->is_terminated()) make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{bbm});
            
            curr_bb_ = bbm;
        } else {
            std::string bbm_name = new_block_name("_if_end_");
            BasicBlock* bbm = curr_func_->add_block(bbm_name);
            if(!curr_bb_->is_terminated()) make_inst(curr_bb_,Opcode::BR,VoidType::get(),"",{cond_v,bb1,bbm});
            curr_bb_ = bb1;
            gen_stmt(is->then_branch.get());
            if(!curr_bb_->is_terminated()) make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{bbm});
            curr_bb_ = bbm;
        }

        
    } else if(auto ws = dynamic_cast<WhileStmt*>(stmt)){
        BasicBlock* cond_bb = curr_func_->add_block(new_block_name("_while_cond_"));
        BasicBlock* body_bb = curr_func_->add_block(new_block_name("_while_body_"));
        BasicBlock* end_bb  = curr_func_->add_block(new_block_name("_while_end_"));

        if(!curr_bb_->is_terminated())
            make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{cond_bb});

        curr_bb_ = cond_bb;
        Value* cond_v = gen_expr(ws->cond.get());
        make_inst(curr_bb_,Opcode::BR,VoidType::get(),"",{cond_v,body_bb,end_bb});

        curr_bb_ = body_bb;
        gen_stmt(ws->body.get());
        if(!curr_bb_->is_terminated())
            make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{cond_bb});

        curr_bb_ = end_bb;
    }
}

Value* AST2IR::gen_expr(ExprNode* expr){
    if(auto nn = dynamic_cast<NumberNode*>(expr)){
        return module_->get_const(nn->value);
    } else if(auto bn = dynamic_cast<BinaryExpr*>(expr)){
        Value* o1 = gen_expr(bn->left.get());
        Value* o2 = gen_expr(bn->right.get());
        return make_inst(curr_bb_,T2O(bn->op,bn),IntType::get(),new_temp_name(),{o1,o2});
    } else if(auto un = dynamic_cast<UnaryExpr*>(expr)){
        Value* o = gen_expr(un->operand.get());
        return make_inst(curr_bb_,T2O(un->op,un),IntType::get(),new_temp_name(),{o});
    } else if(auto is = dynamic_cast<IdentifierNode*>(expr)){
        Instruction* alloca = find_alloc(is->name);
        if(!alloca) throw std::runtime_error("undefined yet used variable " + is->name);
        return make_inst(curr_bb_, Opcode::LOAD, IntType::get(), new_temp_name(), {alloca});
    } else if(auto ae = dynamic_cast<AssignmentExpr*>(expr)){
        Value* rhs_inst = gen_expr(ae->rhs.get());
        Instruction* alloca_left = find_alloc(ae->lhs->name);
        if(!alloca_left) throw std::runtime_error("undefined yet used variable " + ae->lhs->name);
        make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{rhs_inst,alloca_left});
        return rhs_inst;
    } else if(auto ce = dynamic_cast<CallExpr*>(expr)){
        Function* func = module_->find_function(ce->name);
        if(!func) throw std::runtime_error("undefined function " + ce->name);
        if(func->args.size() != ce->args.size()) throw std::runtime_error("function " + ce->name + "should have " + std::to_string(func->args.size()) + "argument, but got " + std::to_string(ce->args.size()) + " arguments");
        std::vector<Value*> args;
        args.push_back(func);
        for(const auto& arg : ce->args){
            args.push_back(gen_expr(arg.get()));
        }
        return make_inst(curr_bb_,Opcode::CALL,IntType::get(),new_temp_name(),args);

    }
    else{
        throw std::runtime_error("unknown node type");
    }

}

Instruction* AST2IR::find_alloc(const std::string& name){
    for(auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it){
        if(it->find(name) != it->end()){
            return (*it)[name];
        }
    }
    return nullptr;
}


}