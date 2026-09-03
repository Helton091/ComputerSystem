#include"AST2IR.hpp"
#include<cassert>
namespace IR{

static bool is_float_type(Type* t){ return t == FloatType::get(); }

static bool is_comparison_opcode(Opcode op){
    return op == Opcode::LT  || op == Opcode::GT  || op == Opcode::LE  ||
           op == Opcode::GE  || op == Opcode::EQ  || op == Opcode::NE  ||
           op == Opcode::FLT || op == Opcode::FGT || op == Opcode::FLE ||
           op == Opcode::FGE || op == Opcode::FEQ || op == Opcode::FNE;
}

static void expect_type(Type* expected, Value* actual, const std::string& context){
    if(expected == actual->type) return;
    throw std::runtime_error(
        "[AST2IR] type mismatch " + context +
        ": expected '" + expected->to_string() +
        "', found '" + actual->type->to_string() + "'"
    );
    
}

static Opcode compound_binop_opcode(Tok op, Type* operand_type){
    bool flt = is_float_type(operand_type);
    switch(op){
    case Tok::ADD_ASSIGN:    return flt ? Opcode::FADD : Opcode::ADD;
    case Tok::SUB_ASSIGN:    return flt ? Opcode::FSUB : Opcode::SUB;
    case Tok::STAR_ASSIGN:   return flt ? Opcode::FMUL : Opcode::MUL;
    case Tok::SLASH_ASSIGN:  return flt ? Opcode::FDIV : Opcode::DIV;
    case Tok::PERCENT_ASSIGN:
        if(flt) throw std::runtime_error("[AST2IR] float type does not support modulo operation");
        return Opcode::REM;
    default: throw std::runtime_error("[AST2IR] cannot translate compound binary token " + std::to_string(static_cast<int>(op)) + " to opcode");
    }
}

static Opcode binop_opcode(Tok op, Type* operand_type){
    bool flt = is_float_type(operand_type);
    switch(op){
    case Tok::ADD:    return flt ? Opcode::FADD : Opcode::ADD;
    case Tok::SUB:    return flt ? Opcode::FSUB : Opcode::SUB;
    case Tok::STAR:   return flt ? Opcode::FMUL : Opcode::MUL;
    case Tok::SLASH:  return flt ? Opcode::FDIV : Opcode::DIV;
    case Tok::PERCENT:
        if(flt) throw std::runtime_error("[AST2IR] float type does not support modulo operation");
        return Opcode::REM;
    case Tok::LT:     return flt ? Opcode::FLT : Opcode::LT;
    case Tok::LE:     return flt ? Opcode::FLE : Opcode::LE;
    case Tok::GT:     return flt ? Opcode::FGT : Opcode::GT;
    case Tok::GE:     return flt ? Opcode::FGE : Opcode::GE;
    case Tok::EQ:     return flt ? Opcode::FEQ : Opcode::EQ;
    case Tok::NE:     return flt ? Opcode::FNE : Opcode::NE;
    default: throw std::runtime_error("[AST2IR] cannot translate binary token " + std::to_string(static_cast<int>(op)) + " to opcode");
    }
}

static Opcode unop_opcode(Tok op, Type* operand_type){
    bool flt = is_float_type(operand_type);
    switch(op){
    case Tok::SUB: return flt ? Opcode::FNEG : Opcode::NEG;
    default: throw std::runtime_error("[AST2IR] cannot translate unary token " + std::to_string(static_cast<int>(op)) + " to opcode");
    }
}

Value* AST2IR::gen_expr_as(AST::ExprNode* expr, Type* expected, const std::string& context){
    if(dynamic_cast<AST::NullPointerNode*>(expr)){
        auto pt = dynamic_cast<PointerType*>(expected);
        if(!pt) throw std::runtime_error("[AST2IR] nullptr used in non-pointer context " + context);
        return module_->get_nullptr(pt->element_type);
    }
    return gen_expr(expr);
}

std::pair<Value*,Type*> AST2IR::gen_lvalue(AST::ExprNode* expr){
    Value* addr = nullptr;
    Type* elem_type = nullptr;
    if(auto indent = dynamic_cast<AST::IdentifierNode*>(expr)){
        addr = find_variable(indent->name);
        if(!addr) throw std::runtime_error("[AST2IR] undefined variable " + indent->name);
        elem_type = static_cast<PointerType*>(addr->type)->element_type;
        return {addr,elem_type};
    }
    if(auto un = dynamic_cast<AST::UnaryExpr*>(expr)){
        if(un->op==Tok::STAR){
            addr = gen_expr(un->operand.get());
            auto ptr_type = dynamic_cast<PointerType*>(addr->type);
            if(!ptr_type) throw std::runtime_error("[AST2IR] try to derefenrence a non-pointer type");
            elem_type = ptr_type->element_type;
            return {addr, elem_type};
        }
    }
    if(auto sb = dynamic_cast<AST::IndexExpr*>(expr)){
        Value* subs = gen_expr(sb->index.get());
        auto [addr_1, elem_type_1] = gen_lvalue(sb->base.get());
        auto arr_tp = dynamic_cast<ArrayType*>(elem_type_1);
        if(!arr_tp) throw std::runtime_error("only array type can be indexed");
        addr = make_inst(curr_bb_,Opcode::GETPTR,PointerType::get(arr_tp->element_type),new_temp_name(),{addr_1,subs});
        return {addr, arr_tp->element_type}; 
    }


    throw std::runtime_error("[AST2IR] try to gen_lvalue to a non-lvalue thing");
    return {addr,elem_type};
}

std::unique_ptr<Module> AST2IR::translate(AST::ProgramNode* program){
    module_ = std::make_unique<Module>();
    for(const std::unique_ptr<AST::DeclStmt>& glob_var : program->glob_vars){
        Value* init = nullptr;
        Type* type = to_ir_type(glob_var->var.type.get());
        const std::string& name = glob_var->var.name;

        if(type == IntType::get()){
            if(auto c = dynamic_cast<AST::IntNumberNode*>(glob_var->init.get())){
                init = module_->get_const(c->value);
            } else if(!glob_var->init){
                init = module_->get_const(0);
            } else {
                throw std::runtime_error("[AST2IR] global int variable '" + name + "' must be initialized with an integer literal");
            }
        }
        else if(type == FloatType::get()){
            if(auto c = dynamic_cast<AST::FloatNumberNode*>(glob_var->init.get())){
                init = module_->get_const(c->value);
            } else if(!glob_var->init){
                init = module_->get_const(0.0f);
            } else {
                throw std::runtime_error("[AST2IR] global float variable '" + name + "' must be initialized with a float literal");
            }
        }
        else if(auto pt = dynamic_cast<PointerType*>(type)){
            if(!glob_var->init || dynamic_cast<AST::NullPointerNode*>(glob_var->init.get())){
                init = module_->get_nullptr(pt->element_type);
            } else {
                throw std::runtime_error("[AST2IR] global pointer variable '" + name + "' can only be initialized with nullptr");
            }
        }
        else if(type == VoidType::get()){
            throw std::runtime_error("[AST2IR] global variable '" + name + "' cannot have void type");
        }
        else {
            throw std::runtime_error("[AST2IR] unsupported global variable type '" + type->to_string() + "' for variable '" + name + "'");
        }
        module_->add_global(glob_var->var.name,PointerType::get(type),init);
    }
    for(const std::unique_ptr<AST::FunctionNode>& funcnode : program->functions){
        Function* func = module_->add_function(funcnode->name,to_ir_type(funcnode->return_type.get()));
        for (size_t i = 0; i < funcnode->params.size(); ++i)
            func->add_arg(to_ir_type(funcnode->params[i].type.get()));

    }
    for(const std::unique_ptr<AST::FunctionNode>& funcnode : program->functions){
        gen_function(funcnode.get());
        
    }
    return std::move(module_);
}

void AST2IR::gen_function(AST::FunctionNode* funcnode){
    temp_counter_ = 0;
    break_stack_.clear();
    continue_stack_.clear();
    curr_func_ = module_->find_function(funcnode->name);
    curr_func_->add_block("entry");
    curr_bb_ = curr_func_->add_block("start");
    make_inst(curr_func_->entry,Opcode::JMP,VoidType::get(),"jmp",{curr_bb_});

    enter_scope();
    int temp_cnt = 0;
    for(const AST::Param& arg : funcnode->params){
        Argument* new_arg =  curr_func_->args[temp_cnt].get();
        Instruction* addr = curr_func_->add_alloca(arg.name, PointerType::get(to_ir_type(arg.type.get())));
        make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{new_arg,addr});
        if(scope_stack_.back().find(arg.name) != scope_stack_.back().end()) throw std::runtime_error("[AST2IR] duplicate name '" + arg.name + "' in function '" + curr_func_->name + "' argument list");
        scope_stack_.back()[arg.name] = addr;
        ++temp_cnt;
    }

    gen_stmt(funcnode->body.get());
    if(!curr_bb_->is_terminated()) {
        if(curr_func_->return_type == IntType::get()){
            make_inst(curr_bb_,Opcode::RET,VoidType::get(),"",{module_->get_const(0)});
        } else if(curr_func_->return_type == FloatType::get()){
            make_inst(curr_bb_,Opcode::RET,VoidType::get(),"",{module_->get_const(0.0f)});
        } else if(curr_func_->return_type == VoidType::get()){
            make_inst(curr_bb_,Opcode::RET,VoidType::get(),"",{});
        } else if(auto pt = dynamic_cast<PointerType*>(curr_func_->return_type))
            make_inst(curr_bb_,Opcode::RET,VoidType::get(),"",{module_->get_nullptr(pt->element_type)});
        else {
            throw std::runtime_error("[AST2IR] unknown return type for function " + curr_func_->name);
        }
    }
    exit_scope();
}

void AST2IR::gen_stmt(AST::StatementNode* stmt){
    if(curr_bb_->is_terminated())
        curr_bb_ = curr_func_->add_block(new_block_name("_unreachable_"));


    if(auto bs = dynamic_cast<AST::BlockStatement*>(stmt)){
        enter_scope();
        for(const auto& bbs : bs->statements){
            gen_stmt(bbs.get());
        }
        exit_scope();
    } else if(auto rs = dynamic_cast<AST::ReturnStatement*>(stmt)){
        if(curr_func_->return_type != VoidType::get()){
            if(!rs->expr) throw std::runtime_error("[AST2IR] function '" + curr_func_->name + "' with non-void return type must return a value");
            Value* ret_val = gen_expr_as(rs->expr.get(), curr_func_->return_type,
                        "in return statement of function '" + curr_func_->name + "'");
            expect_type(curr_func_->return_type, ret_val,
                        "in return statement of function '" + curr_func_->name + "'");
            make_inst(curr_bb_, Opcode::RET, VoidType::get(),"",{ret_val});
        } else {
            if(rs->expr) throw std::runtime_error("[AST2IR] void function '" + curr_func_->name + "' cannot return a value");
            make_inst(curr_bb_, Opcode::RET, VoidType::get(),"",{});
        }
    } else if(auto ds = dynamic_cast<AST::DeclStmt*>(stmt)){
        std::unordered_map<std::string,Instruction*>& curr_stack =  scope_stack_.back();
        if(curr_stack.find(ds->var.name) != curr_stack.end()) throw std::runtime_error("[AST2IR] redefined variable '" + ds->var.name + "' in function '" + curr_func_->name + "'");
        Instruction* di = curr_func_->add_alloca(ds->var.name, PointerType::get(to_ir_type(ds->var.type.get())));
        curr_stack[ds->var.name] = di;
        Type* elem_type = static_cast<PointerType*>(di->type)->element_type;
        if(dynamic_cast<ArrayType*>(elem_type)) return;
        //array type don't involve initialization
        if(ds->init){
            Value* stored_value = gen_expr_as(ds->init.get(), elem_type,
                        "in initialization of variable '" + ds->var.name + "'");
            expect_type(elem_type, stored_value,
                        "in initialization of variable '" + ds->var.name + "'");
            make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{stored_value,di});
        } else {
            Value* v = nullptr;
            if(dynamic_cast<AST::IntType*>(ds->var.type.get())){
                v = module_->get_const(0);
            } else if(dynamic_cast<AST::FloatType*>(ds->var.type.get())){
                v = module_->get_const(0.0f);
            } else if(auto pt = dynamic_cast<AST::PointerType*>(ds->var.type.get())){
                v = module_->get_nullptr(to_ir_type(pt->pointee.get()));
            }
            else {
                throw std::runtime_error("[AST2IR] unsupported type '" + ds->var.type->to_string() + "' for default initialization of variable '" + ds->var.name + "' in function '" + curr_func_->name + "'");
            }
            make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{v,di});
        }
    } else if(auto es = dynamic_cast<AST::ExprStmt*>(stmt)){
        gen_expr(es->expr.get());
    } else if(auto is = dynamic_cast<AST::IfStmt*>(stmt)){
        Value* cond_v = gen_expr(is->cond.get());
        expect_type(IntType::get(), cond_v,
                    "in condition of 'if' statement");
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

        
    } else if(auto ws = dynamic_cast<AST::WhileStmt*>(stmt)){
        BasicBlock* cond_bb = curr_func_->add_block(new_block_name("_while_cond_"));
        BasicBlock* body_bb = curr_func_->add_block(new_block_name("_while_body_"));
        BasicBlock* end_bb  = curr_func_->add_block(new_block_name("_while_end_"));
        break_stack_.push_back(end_bb);
        continue_stack_.push_back(cond_bb);
        if(!curr_bb_->is_terminated())
            make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{cond_bb});

        curr_bb_ = cond_bb;
        Value* cond_v = gen_expr(ws->cond.get());
        expect_type(IntType::get(), cond_v,
                    "in condition of 'while' statement");
        make_inst(curr_bb_,Opcode::BR,VoidType::get(),"",{cond_v,body_bb,end_bb});

        curr_bb_ = body_bb;
        gen_stmt(ws->body.get());
        if(!curr_bb_->is_terminated())
            make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{cond_bb});

        curr_bb_ = end_bb;
        break_stack_.pop_back();
        continue_stack_.pop_back();
    } else if(auto fs = dynamic_cast<AST::ForStmt*>(stmt)){
        enter_scope(); //for init
        gen_stmt(fs->init.get());
        BasicBlock* cond_bb = curr_func_->add_block(new_block_name("_for_cond_"));
        BasicBlock* body_bb = curr_func_->add_block(new_block_name("_for_body_"));
        BasicBlock* update_bb = curr_func_->add_block(new_block_name("_for_update_"));
        BasicBlock* end_bb  = curr_func_->add_block(new_block_name("_for_end_"));
        break_stack_.push_back(end_bb);
        continue_stack_.push_back(update_bb);

        if(!curr_bb_->is_terminated())
            make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{cond_bb});
        
        curr_bb_ = cond_bb;
        Value* cond_v = fs->cond ? gen_expr(fs->cond.get()) : module_->get_const(1);
        expect_type(IntType::get(), cond_v,
                    "in condition of 'for' statement");
        make_inst(curr_bb_,Opcode::BR,VoidType::get(),"",{cond_v,body_bb,end_bb});
        
        curr_bb_ = body_bb;
        gen_stmt(fs->body.get());
        if(!curr_bb_->is_terminated())
            make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{update_bb});

        curr_bb_ = update_bb;
        if(fs->update) gen_expr(fs->update.get());
        if(!curr_bb_->is_terminated())
            make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{cond_bb});

        curr_bb_ = end_bb;


        break_stack_.pop_back();
        continue_stack_.pop_back();
        exit_scope(); //for init
    } else if(dynamic_cast<AST::BreakStmt*>(stmt)){
        if(break_stack_.empty()) throw std::runtime_error("[AST2IR] 'break' outside of loop or switch in function '" + curr_func_->name + "'");
        make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{break_stack_.back()});
    } else if(dynamic_cast<AST::ContinueStmt*>(stmt)){
        if(continue_stack_.empty()) throw std::runtime_error("[AST2IR] 'continue' outside of loop in function '" + curr_func_->name + "'");
        make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{continue_stack_.back()});
    }
}

Value* AST2IR::gen_expr(AST::ExprNode* expr){
    if(auto nn = dynamic_cast<AST::IntNumberNode*>(expr)){
        return module_->get_const(nn->value);
    } else if(auto nn = dynamic_cast<AST::FloatNumberNode*>(expr)){
        return module_->get_const(nn->value);
    } else if(auto bn = dynamic_cast<AST::BinaryExpr*>(expr)){
        bool left_null  = dynamic_cast<AST::NullPointerNode*>(bn->left.get());
        bool right_null = dynamic_cast<AST::NullPointerNode*>(bn->right.get());

        Value* o1 = left_null  ? nullptr : gen_expr(bn->left.get());
        Value* o2 = right_null ? nullptr : gen_expr(bn->right.get());

        //nullptr 归一化：一边是 nullptr 时，类型从另一边注入
        if(left_null != right_null){
            Value* other = left_null ? o2 : o1;
            auto pt = dynamic_cast<PointerType*>(other->type);
            if(pt){
                Value* null = module_->get_nullptr(pt->element_type);
                if(left_null) o1 = null; else o2 = null;
            } else {
                throw std::runtime_error("[AST2IR] nullptr used with non-pointer operand in binary expression of function '" + curr_func_->name + "'");
            }
        }
        if(left_null && right_null){
            throw std::runtime_error("[AST2IR] cannot determine pointer type of nullptr in binary expression of function '" + curr_func_->name + "'");
        }

        auto ptr1 = dynamic_cast<PointerType*>(o1->type);
        auto ptr2 = dynamic_cast<PointerType*>(o2->type);
        auto is_scalar = [](Type* t){ return dynamic_cast<IntType*>(t) != nullptr || dynamic_cast<FloatType*>(t) != nullptr; };

        //p ± i / i + p：按 pointee 大小缩放的指针步进（仅限标量 pointee）
        if(ptr1 && o2->type==IntType::get() && (bn->op==Tok::ADD || bn->op == Tok::SUB)){
            if(!is_scalar(ptr1->element_type))
                throw std::runtime_error("[AST2IR] pointer arithmetic is only supported on pointers to scalar types, got '" + o1->type->to_string() + "' in function '" + curr_func_->name + "'");
            if(bn->op == Tok::SUB)
                o2 = make_inst(curr_bb_,Opcode::NEG,o2->type,new_temp_name(),{o2});
            return make_inst(curr_bb_,Opcode::GETPTR,o1->type,new_temp_name(),{o1,o2});
        }
        if(o1->type==IntType::get() && ptr2 && bn->op==Tok::ADD){
            if(!is_scalar(ptr2->element_type))
                throw std::runtime_error("[AST2IR] pointer arithmetic is only supported on pointers to scalar types, got '" + o2->type->to_string() + "' in function '" + curr_func_->name + "'");
            return make_inst(curr_bb_,Opcode::GETPTR,o2->type,new_temp_name(),{o2,o1});
        }
        //指针对：p - q 求元素差；比较降级为 PTRDIFF 后与 0 做整数比较
        //等价性依赖平坦内存且指针差落在有符号 32 位内（MEM ≤ 2GB）
        if(ptr1 && ptr2){
            if(ptr1->element_type != ptr2->element_type)
                throw std::runtime_error("[AST2IR] binary expression operands type mismatch: left is '" + o1->type->to_string() + "', right is '" + o2->type->to_string() + "' in function '" + curr_func_->name + "'");
            if(!is_scalar(ptr1->element_type))
                throw std::runtime_error("[AST2IR] pointer operations are only supported on pointers to scalar types, got '" + o1->type->to_string() + "' in function '" + curr_func_->name + "'");
            if(bn->op == Tok::SUB)
                return make_inst(curr_bb_,Opcode::PTRDIFF,IntType::get(),new_temp_name(),{o1,o2});
            switch(bn->op){
            case Tok::EQ: case Tok::NE:
            case Tok::LT: case Tok::LE:
            case Tok::GT: case Tok::GE:{
                Value* diff = make_inst(curr_bb_,Opcode::PTRDIFF,IntType::get(),new_temp_name(),{o1,o2});
                return make_inst(curr_bb_,binop_opcode(bn->op,IntType::get()),IntType::get(),new_temp_name(),{diff,module_->get_const(0)});
            }
            default:
                throw std::runtime_error("[AST2IR] invalid operation on two pointer operands in function '" + curr_func_->name + "'");
            }
        }
        if(o1->type != o2->type) throw std::runtime_error("[AST2IR] binary expression operands type mismatch: left is '" + o1->type->to_string() + "', right is '" + o2->type->to_string() + "' in function '" + curr_func_->name + "'");
        Opcode op = binop_opcode(bn->op,o1->type);
        Type* result_type = is_comparison_opcode(op) ? IntType::get() : o1->type;
        return make_inst(curr_bb_,op,result_type,new_temp_name(),{o1,o2});
    } else if(auto un = dynamic_cast<AST::UnaryExpr*>(expr)){
        switch(un->op){
        case Tok::AMPERSAND: {
            auto [addr, elem_type] = gen_lvalue(un->operand.get());
            return addr;
        }
        case Tok::STAR: {
            auto [addr,elem_type] = gen_lvalue(un);
            return make_inst(curr_bb_, Opcode::LOAD, elem_type, new_temp_name(), {addr});
        }
        case Tok::SUB: {
            Value* o = gen_expr(un->operand.get());
            return make_inst(curr_bb_, unop_opcode(un->op, o->type), o->type, new_temp_name(), {o});
        }
        default:
            throw std::runtime_error("[AST2IR] unknown unary opcode in function '" + curr_func_->name + "'");
        }

    } else if(auto is = dynamic_cast<AST::IdentifierNode*>(expr)){
        Value* addr = find_variable(is->name);
        if(!addr) throw std::runtime_error("[AST2IR] undefined variable '" + is->name + "' used in function '" + curr_func_->name + "'");
        if(auto p = dynamic_cast<PointerType*>(addr->type)){
            if(dynamic_cast<ArrayType*>(p->element_type)) throw std::runtime_error("[AST2IR] array type cannot be used as a value (" + is->name + ")" );
            return make_inst(curr_bb_, Opcode::LOAD, p->element_type, new_temp_name(), {addr});
        }
        else
            throw std::runtime_error("[AST2IR] variable '" + is->name + "' is not an address");
    } else if(auto ae = dynamic_cast<AST::AssignmentExpr*>(expr)){
        auto [dest_addr,elem_type] = gen_lvalue(ae->lhs.get());

        Value* rhs_inst = gen_expr_as(ae->rhs.get(), elem_type, "in assignment");

        expect_type(elem_type, rhs_inst,
                    "in assignment ");
        make_inst(curr_bb_, Opcode::STORE, VoidType::get(), "", {rhs_inst, dest_addr});
        return rhs_inst;

    } else if(auto ce = dynamic_cast<AST::CallExpr*>(expr)){
        Function* func = module_->find_function(ce->name);
        if(!func) throw std::runtime_error("[AST2IR] undefined function '" + ce->name + "' called in function '" + curr_func_->name + "'");
        if(func->args.size() != ce->args.size()) throw std::runtime_error("[AST2IR] function '" + ce->name + "' expects " + std::to_string(func->args.size()) + " argument(s), but got " + std::to_string(ce->args.size()) + " in function '" + curr_func_->name + "'");
        std::vector<Value*> args;
        args.push_back(func);
        for(size_t i = 0; i < ce->args.size(); ++i){
            Type* param_type = func->args[i]->type;
            AST::ExprNode* en = ce->args[i].get();
            
            
            Value* arg_val = gen_expr_as(en, param_type,
                        "in argument " + std::to_string(i + 1) +
                        " of call to function '" + ce->name + "'");
            expect_type(param_type, arg_val,
                        "in argument " + std::to_string(i + 1) +
                        " of call to function '" + ce->name + "'");
            args.push_back(arg_val);
        }
        return make_inst(curr_bb_,Opcode::CALL,func->return_type,new_temp_name(),args);

    } else if(dynamic_cast<AST::NullPointerNode*>(expr)){
        throw std::runtime_error("[AST2IR] nullptr must be used in a pointer context");
    } else if(auto ie = dynamic_cast<AST::IndexExpr*>(expr)){
        auto [addr, elem_type] = gen_lvalue(ie);
        return make_inst(curr_bb_,Opcode::LOAD,elem_type,new_temp_name(),{addr});
    } else if(auto cae = dynamic_cast<AST::CompoundAssignExpr*>(expr)){
        auto [addr, elem_type] = gen_lvalue(cae->lhs.get());
        Value* old_v = make_inst(curr_bb_,Opcode::LOAD,elem_type,new_temp_name(),{addr});
        Value* right_v = gen_expr(cae->rhs.get());
        expect_type(right_v->type,old_v,"in compound assignment expression");
        Opcode new_op = compound_binop_opcode(cae->op,right_v->type);
        Value* new_v = nullptr;
        new_v = make_inst(curr_bb_,new_op,right_v->type,"",{old_v,right_v});
        make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{new_v,addr});
        return new_v;

    }
    else{
        throw std::runtime_error("[AST2IR] unknown AST expression node type in function '" + curr_func_->name + "'");
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

Value* AST2IR::find_variable(const std::string& name){
    Instruction* local = find_alloc(name);
    if(local) return local;

    GlobalVariable* glob = module_->find_global(name);
    return glob;
}

Type* AST2IR::to_ir_type(AST::Type* ast_type){
    if(dynamic_cast<AST::IntType*>(ast_type)) return IntType::get();
    if(dynamic_cast<AST::FloatType*>(ast_type)) return FloatType::get();
    if(dynamic_cast<AST::VoidType*>(ast_type)) return VoidType::get();
    if(auto p = dynamic_cast<AST::PointerType*>(ast_type)) return PointerType::get(to_ir_type(p->pointee.get()));
    if(auto a = dynamic_cast<AST::ArrayType*>(ast_type)) return ArrayType::get(to_ir_type(a->base_type.get()),a->length);
    throw std::runtime_error("[AST2IR] encounter unknown AST type");
}


}