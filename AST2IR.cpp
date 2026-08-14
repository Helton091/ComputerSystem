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
        gen_function(funcnode.get());
    }
    return std::move(module_);
}

void AST2IR::gen_function(FunctionNode* funcnode){
    temp_counter_ = 0;
    curr_func_ = module_->add_function(funcnode->name);
    curr_func_->add_block("entry");
    curr_bb_ = curr_func_->add_block("start");
    make_inst(curr_func_->entry,Opcode::JMP,VoidType::get(),"jmp",{curr_bb_});
    gen_stmt(funcnode->body.get());


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
    }
    else{
        throw std::runtime_error("unknown node type");
    }

}


}