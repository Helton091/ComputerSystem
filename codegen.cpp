#include "codegen.hpp"

void CodeGen::gen_while(const WhileStmt* node) {
    std::string start_label = new_label(".L_while_start_");
    std::string end_label = new_label(".L_while_end_");

    emit(start_label + ":");
    gen_expr(node->cond.get());
    emit("lw t0, 0(sp)");
    emit("addi sp, sp, 4");
    emit("beq t0, zero, " + end_label);
    gen_statement(node->body.get());
    emit("j " + start_label);
    emit(end_label + ":");
}

void CodeGen::gen_if(const IfStmt* node) {
    gen_expr(node->cond.get());
    emit("lw t0, 0(sp)");
    emit("addi sp, sp, 4");
    if (node->else_branch) {
        std::string else_label = new_label(".L_else_");
        std::string end_label = new_label(".L_end_");
        emit("beq t0, zero, " + else_label);
        gen_statement(node->then_branch.get());
        emit("j " + end_label);
        emit(else_label + ":");
        gen_statement(node->else_branch.get());
        emit(end_label + ":");
    } else {
        std::string end_label = new_label(".L_end_");
        emit("beq t0, zero, " + end_label);
        gen_statement(node->then_branch.get());
        emit(end_label + ":");
    }
}

int CodeGen::lookup_variable(const std::string& name) {
    for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    throw std::runtime_error("undefined variable " + name);
}

int CodeGen::count_decl_with_size(const BlockStatement* block) {
    int count = 0;
    for (const std::unique_ptr<StatementNode>& stat : block->statements) {
        if (dynamic_cast<const DeclStmt*>(stat.get())) {
            count += 4;
        } else if (auto bloc = dynamic_cast<const BlockStatement*>(stat.get())) {
            count += count_decl_with_size(bloc);
        } else if (auto ifs = dynamic_cast<const IfStmt*>(stat.get())) {
            if (auto then_blk = dynamic_cast<const BlockStatement*>(ifs->then_branch.get())) {
                count += count_decl_with_size(then_blk);
            }
            if (ifs->else_branch) {
                if (auto else_blk = dynamic_cast<const BlockStatement*>(ifs->else_branch.get())) {
                    count += count_decl_with_size(else_blk);
                }
            }
        } else if (auto whs = dynamic_cast<const WhileStmt*>(stat.get())) {
            if (auto body_blk = dynamic_cast<const BlockStatement*>(whs->body.get())) {
                count += count_decl_with_size(body_blk);
            }
        }
    }
    return count;
}

void CodeGen::gen_expr(const ExprNode* node) {
    if (auto num = dynamic_cast<const NumberNode*>(node)) {
        emit("addi t0, zero, " + std::to_string(num->value));
        emit("addi sp, sp, -4");
        emit("sw t0, 0(sp)");
    } else if (auto unary = dynamic_cast<const UnaryExpr*>(node)) {
        gen_expr(unary->operand.get());
        emit("lw t0, 0(sp)");
        emit("addi sp, sp, 4");
        switch (unary->op) {
        case Tok::SUB:
            emit("sub t0, zero, t0");
            break;
        default:
            throw std::runtime_error("unknown unary operation");
        }
        emit("addi sp, sp, -4");
        emit("sw t0, 0(sp)");
    } else if (auto binary = dynamic_cast<const BinaryExpr*>(node)) {
        gen_expr(binary->left.get());
        gen_expr(binary->right.get());

        emit("lw t1, 0(sp)"); // right
        emit("addi sp, sp, 4");
        emit("lw t0, 0(sp)"); // left
        emit("addi sp, sp, 4");

        switch (binary->op) {
        case Tok::ADD:    emit("add t0, t0, t1"); break;
        case Tok::SUB:    emit("sub t0, t0, t1"); break;
        case Tok::STAR:   emit("mul t0, t0, t1"); break;
        case Tok::SLASH:  emit("div t0, t0, t1"); break;
        case Tok::PERCENT: emit("rem t0, t0, t1"); break;
        case Tok::LT: emit("slt t0, t0, t1"); break;
        case Tok::GT: emit("slt t0, t1, t0"); break;
        case Tok::LE: emit("slt t0, t1, t0"); emit("xori t0, t0, 1"); break;
        case Tok::GE: emit("slt t0, t0, t1"); emit("xori t0, t0, 1"); break;
        case Tok::EQ: emit("sub t0, t0, t1"); emit("sltiu t0, t0, 1"); break;
        case Tok::NE: emit("sub t0, t0, t1"); emit("sltu t0, zero, t0"); break;
        default: throw std::runtime_error("unknown binary op");
        }

        emit("addi sp, sp, -4");
        emit("sw t0, 0(sp)");
    } else if (auto iden = dynamic_cast<const IdentifierNode*>(node)) {
        gen_identifier(iden);
    } else if (auto assi = dynamic_cast<const AssignmentExpr*>(node)) {
        gen_assignment(assi);
    } else if (const CallExpr* func = dynamic_cast<const CallExpr*>(node)) {
        for (const std::unique_ptr<ExprNode>& arg : func->args) {
            gen_expr(arg.get());
        }
        for (int i = static_cast<int>(func->args.size()) - 1; i >= 0; --i) {
            emit("lw a" + std::to_string(i) + ", 0(sp)");
            emit("addi sp, sp, 4");
        }

        emit("addi sp, sp, -4");
        emit("sw ra, 0(sp)");
        emit("call " + func->name);
        emit("lw ra, 0(sp)");
        emit("addi sp, sp, 4");
        emit("addi sp, sp, -4");
        emit("sw a0, 0(sp)");
    } else {
        throw std::runtime_error("unknown expression node");
    }
}

void CodeGen::gen_return(const ReturnStatement* node) {
    gen_expr(node->expr.get());
    emit("lw a0, 0(sp)");
    emit("addi sp, sp, 4");

    emit("j " + current_epilogue);
}

void CodeGen::gen_statement(const StatementNode* node) {
    if (auto ret = dynamic_cast<const ReturnStatement*>(node)) {
        gen_return(ret);
    } else if (auto dec = dynamic_cast<const DeclStmt*>(node)) {
        gen_decl(dec);
    } else if (auto expr = dynamic_cast<const ExprStmt*>(node)) {
        gen_expr_stmt(expr);
    } else if (auto blk = dynamic_cast<const BlockStatement*>(node)) {
        gen_block(blk);
    } else if (auto ifs = dynamic_cast<const IfStmt*>(node)) {
        gen_if(ifs);
    } else if (auto whs = dynamic_cast<const WhileStmt*>(node)) {
        gen_while(whs);
    } else {
        throw std::runtime_error("unknown statement");
    }
}

void CodeGen::gen_block(const BlockStatement* node) {
    enter_scope();
    for (const std::unique_ptr<StatementNode>& snode : node->statements) {
        gen_statement(snode.get());
    }
    exit_scope();
}

int CodeGen::count_params_with_size(const FunctionNode* funct) {
    return static_cast<int>(funct->params.size()) * 4;
}

void CodeGen::gen_function(const FunctionNode* node) {
    scope_stack.clear();
    next_offset = -12;
    frame_size = calculate_frame_size(node);
    current_epilogue = new_label(".L_epilogue_");
    emit(node->name + ":");
    emit("addi sp, sp, " + std::to_string(-frame_size));
    emit("sw s0, " + std::to_string(frame_size - 4) + "(sp)");
    emit("sw ra, " + std::to_string(frame_size - 8) + "(sp)");
    emit("addi s0, sp, " + std::to_string(frame_size));
    enter_scope();
    for (size_t i = 0; i < node->params.size(); ++i) {
        emit("sw a" + std::to_string(i) + ", " + std::to_string(next_offset) + "(s0)");
        scope_stack.back()[node->params[i].name] = next_offset;
        next_offset -= 4;
    }
    gen_block(node->body.get());
    exit_scope();
    emit(current_epilogue + ":");
    emit("lw ra, " + std::to_string(-8) + "(s0)");
    emit("lw s0, " + std::to_string(-4) + "(s0)");
    emit("addi sp, sp, " + std::to_string(frame_size));
    emit("ret");
}

void CodeGen::gen_program(const ProgramNode* node) {
    emit("_start:");
    emit("call main");
    emit("li a7, 10");
    emit("ecall");
    for (const std::unique_ptr<FunctionNode>& func_ptr : node->functions) {
        gen_function(func_ptr.get());
    }
}

void CodeGen::gen_decl(const DeclStmt* node) {
    int offset = next_offset;
    if (scope_stack.empty()) {
        throw std::runtime_error("decleration outside any scope " + node->name);
    }
    std::unordered_map<std::string, int>& current_scope = scope_stack.back();
    if (current_scope.find(node->name) != current_scope.end()) {
        throw std::runtime_error("redefined variable " + node->name);
    }
    current_scope[node->name] = offset;
    next_offset -= 4;
    if (node->init) {
        gen_expr(node->init.get());
        emit("lw t0, 0(sp)");
        emit("addi sp, sp, 4");
    } else {
        emit("li t0, 0");
    }
    emit("sw t0, " + std::to_string(offset) + "(s0)");
}

void CodeGen::gen_identifier(const IdentifierNode* node) {
    int offset = lookup_variable(node->name);

    emit("lw t0, " + std::to_string(offset) + "(s0)");
    emit("addi sp, sp, -4");
    emit("sw t0, 0(sp)");
}

void CodeGen::gen_assignment(const AssignmentExpr* node) {
    gen_expr(node->rhs.get());

    emit("lw t0, 0(sp)");
    emit("addi sp, sp, 4");

    int offset = lookup_variable(node->lhs->name);
    emit("sw t0, " + std::to_string(offset) + "(s0)");
    emit("addi sp, sp, -4");
    emit("sw t0, 0(sp)");
}

void CodeGen::gen_expr_stmt(const ExprStmt* node) {
    gen_expr(node->expr.get());
    emit("addi sp, sp, 4");
}

void CodeGen::generate(ASTNode* ast, std::ostream& out) {
    const ProgramNode* program = dynamic_cast<const ProgramNode*>(ast);
    if (!program) {
        throw std::runtime_error("root must be ProgramNode");
    }
    instructions.clear();
    gen_program(program);
    for (const std::string& line : instructions) {
        out << line << "\n";
    }
}
