#include "IR.hpp"

namespace IR {

namespace detail {

std::string operand_to_string(const Operand& op) {
    switch (op.kind) {
        case Operand::IMM:   return std::to_string(op.i_val);
        case Operand::VAR:   return op.name;
        case Operand::LABEL: return op.name;
    }
    return "?";
}

std::string opcode_to_string(Opcode op) {
    switch (op) {
        case Opcode::ADD:    return "add";
        case Opcode::SUB:    return "sub";
        case Opcode::MUL:    return "mul";
        case Opcode::DIV:    return "div";
        case Opcode::REM:    return "rem";
        case Opcode::LT:     return "lt";
        case Opcode::GT:     return "gt";
        case Opcode::LE:     return "le";
        case Opcode::GE:     return "ge";
        case Opcode::EQ:     return "eq";
        case Opcode::NE:     return "ne";
        case Opcode::NEG:    return "neg";
        case Opcode::ASSIGN: return "mov";
        case Opcode::LOAD:   return "load";
        case Opcode::STORE:  return "store";
        case Opcode::PARAM:  return "param";
        case Opcode::CALL:   return "call";
        case Opcode::RET:    return "ret";
        case Opcode::LABEL:  return "label";
        case Opcode::JMP:    return "jmp";
        case Opcode::JZ:     return "jz";
        case Opcode::JNZ:    return "jnz";
        case Opcode::PHI:    return "phi";
    }
    return "?";
}

} // namespace detail

// ---------- Operand ----------

Operand Operand::imm(int v, IRType t) {
    Operand o;
    o.kind = IMM;
    o.type = t;
    o.i_val = v;
    return o;
}

Operand Operand::var(const std::string& n, IRType t) {
    Operand o;
    o.kind = VAR;
    o.type = t;
    o.name = n;
    return o;
}

Operand Operand::label(const std::string& n) {
    Operand o;
    o.kind = LABEL;
    o.name = n;
    return o;
}

bool Operand::is_imm() const { return kind == IMM; }
bool Operand::is_var() const { return kind == VAR; }
bool Operand::is_label() const { return kind == LABEL; }

// ---------- Instruction ----------

Instruction::Instruction(Opcode o) : op(o) {}

// ---------- BasicBlock ----------

void BasicBlock::dump(std::ostream& out, int indent) const {
    out << std::string(indent, ' ') << label << ":\n";
    for (const auto& inst : insts) {
        out << std::string(indent + 2, ' ');

        switch (inst.op) {
            case Opcode::ADD:
            case Opcode::SUB:
            case Opcode::MUL:
            case Opcode::DIV:
            case Opcode::REM:
            case Opcode::LT:
            case Opcode::GT:
            case Opcode::LE:
            case Opcode::GE:
            case Opcode::EQ:
            case Opcode::NE:
                out << detail::operand_to_string(inst.result)
                    << " = " << detail::opcode_to_string(inst.op)
                    << " " << detail::operand_to_string(inst.lhs)
                    << ", " << detail::operand_to_string(inst.rhs);
                break;

            case Opcode::NEG:
                out << detail::operand_to_string(inst.result)
                    << " = neg " << detail::operand_to_string(inst.lhs);
                break;

            case Opcode::ASSIGN:
                out << detail::operand_to_string(inst.result)
                    << " = " << detail::operand_to_string(inst.lhs);
                break;

            case Opcode::LOAD:
                out << detail::operand_to_string(inst.result)
                    << " = load " << detail::operand_to_string(inst.lhs);
                break;

            case Opcode::STORE:
                out << "store " << detail::operand_to_string(inst.lhs)
                    << ", " << detail::operand_to_string(inst.rhs);
                break;

            case Opcode::PARAM:
                out << "param " << detail::operand_to_string(inst.lhs);
                break;

            case Opcode::CALL:
                if (!inst.result.name.empty()) {
                    out << detail::operand_to_string(inst.result) << " = ";
                }
                out << "call " << detail::operand_to_string(inst.lhs);
                if (inst.rhs.is_imm() && inst.rhs.i_val > 0) {
                    out << ", " << inst.rhs.i_val;
                }
                break;

            case Opcode::RET:
                out << "ret " << detail::operand_to_string(inst.lhs);
                break;

            case Opcode::LABEL:
                out << "." << detail::operand_to_string(inst.lhs);
                break;

            case Opcode::JMP:
                out << "jmp " << inst.jump_label;
                break;

            case Opcode::JZ:
                out << "jz " << detail::operand_to_string(inst.lhs)
                    << ", " << inst.jump_label;
                break;

            case Opcode::JNZ:
                out << "jnz " << detail::operand_to_string(inst.lhs)
                    << ", " << inst.jump_label;
                break;

            case Opcode::PHI:
                out << detail::operand_to_string(inst.result) << " = phi(...)";
                break;
        }

        if (!inst.comment.empty()) {
            out << "  # " << inst.comment;
        }
        out << "\n";
    }
}

// ---------- IRFunction ----------

void IRFunction::build_label_map() {
    label_to_idx.clear();
    for (size_t i = 0; i < blocks.size(); ++i) {
        label_to_idx[blocks[i]->label] = i;
    }
}

BasicBlock* IRFunction::find_block(const std::string& label) {
    auto it = label_to_idx.find(label);
    if (it != label_to_idx.end()) {
        return blocks[it->second].get();
    }
    return nullptr;
}

const BasicBlock* IRFunction::find_block(const std::string& label) const {
    auto it = label_to_idx.find(label);
    if (it != label_to_idx.end()) {
        return blocks[it->second].get();
    }
    return nullptr;
}

void IRFunction::dump(std::ostream& out) const {
    out << "func " << name << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) out << ", ";
        out << params[i].name;
    }
    out << "):\n";

    for (const auto& bb : blocks) {
        bb->dump(out, 2);
    }
}

// ---------- IRProgram ----------

void IRProgram::dump(std::ostream& out) const {
    for (const auto& func : functions) {
        func->dump(out);
        out << "\n";
    }
}

std::unique_ptr<IRProgram> AST2IR::translate(ProgramNode* node) {
    program_ = std::make_unique<IRProgram>();
    gen_program(node);
    return std::move(program_);
}

void AST2IR::gen_program(const ProgramNode* node) {
    for (const auto& func_node : node->functions) {
        program_->functions.push_back(std::move(gen_function(func_node.get())));
    }
}

std::unique_ptr<IRFunction> AST2IR::gen_function(const FunctionNode* node) {
    auto ir_func = std::make_unique<IRFunction>();
    ir_func->name = node->name;

    // 每个函数独立的计数器
    tmp_counter_ = 0;
    label_counter_ = 0;

    // 参数进入当前作用域
    enter_scope();
    for (const auto& param : node->params) {
        ir_func->params.push_back({param.name, IRType::INT});
        scope_stack_.back()[param.name] = param.name;
    }

    // 创建入口基本块
    auto entry = std::make_unique<BasicBlock>();
    entry->label = node->name + ".entry";
    BasicBlock* entry_ptr = entry.get();
    ir_func->blocks.push_back(std::move(entry));
    ir_func->entry_idx = 0;

    // 设置当前翻译上下文
    current_func_ = ir_func.get();
    current_block_ = entry_ptr;

    // 翻译函数体
    gen_block(node->body.get());

    // 兜底：如果最后一条指令不是 terminator，补一个 ret 0
    if (current_block_->insts.empty() ||
        (current_block_->insts.back().op != Opcode::RET &&
         current_block_->insts.back().op != Opcode::JMP &&
         current_block_->insts.back().op != Opcode::JZ &&
         current_block_->insts.back().op != Opcode::JNZ)) {
        Instruction ret_inst(Opcode::RET);
        ret_inst.lhs = Operand::imm(0);
        emit(ret_inst);
    }

    // 构建 label -> index 映射，方便后续查找
    ir_func->build_label_map();

    // 退出参数作用域
    exit_scope();

    return ir_func;
}

Operand AST2IR::gen_expr(const ExprNode* node) {
    if (auto num = dynamic_cast<const NumberNode*>(node)) {
        return gen_number(num);
    }
    if (auto id = dynamic_cast<const IdentifierNode*>(node)) {
        return gen_identifier(id);
    }
    if (auto bin = dynamic_cast<const BinaryExpr*>(node)) {
        return gen_binary(bin);
    }
    if (auto unary = dynamic_cast<const UnaryExpr*>(node)) {
        return gen_unary(unary);
    }
    if (auto call = dynamic_cast<const CallExpr*>(node)) {
        return gen_call(call);
    }
    if (auto assign = dynamic_cast<const AssignmentExpr*>(node)) {
        return gen_assignment(assign);
    }
    throw std::runtime_error("unknown expression node");
}


Operand AST2IR::gen_number(const NumberNode* node){
    return Operand::imm(node->value);
}

Operand AST2IR::gen_identifier(const IdentifierNode* node) {
    return Operand::var(lookup_var(node->name));
}

Operand AST2IR::gen_binary(const BinaryExpr* node) {
    Operand left = gen_expr(node->left.get());
    Operand right = gen_expr(node->right.get());

    Operand result = Operand::var(new_temp());

    Instruction inst(map_op(node->op));
    inst.type = IRType::INT;
    inst.result = result;
    inst.lhs = left;
    inst.rhs = right;
    emit(inst);

    return result;
}

Operand AST2IR::gen_unary(const UnaryExpr* node) {
    Operand operand = gen_expr(node->operand.get());

    switch (node->op) {
        case Tok::SUB: {
            Operand result = Operand::var(new_temp());
            Instruction inst(Opcode::NEG);
            inst.result = result;
            inst.lhs = operand;
            emit(inst);
            return result;
        }
        default:
            throw std::runtime_error("unsupported unary operator");
    }
}

Operand AST2IR::gen_assignment(const AssignmentExpr* node){
    Operand rhs = gen_expr(node->rhs.get());
    Operand lhs = Operand::var(lookup_var(node->lhs->name));
    Instruction inst(Opcode::ASSIGN);
    inst.result = lhs;
    inst.lhs = rhs;
    emit(inst);

    return lhs;
}

Operand AST2IR::gen_call(const CallExpr* node) {
    // 1. 翻译所有参数
    std::vector<Operand> args;
    for (const auto& arg : node->args) {
        args.push_back(gen_expr(arg.get()));
    }

    // 2. 按顺序 emit param
    for (const auto& arg : args) {
        Instruction param_inst(Opcode::PARAM);
        param_inst.lhs = arg;
        emit(param_inst);
    }

    // 3. 创建结果临时变量
    Operand result = Operand::var(new_temp());

    // 4. emit call
    Instruction call_inst(Opcode::CALL);
    call_inst.result = result;
    call_inst.lhs = Operand::label(node->name);
    call_inst.rhs = Operand::imm(static_cast<int>(args.size()));
    emit(call_inst);

    return result;
}

void AST2IR::gen_statement(const StatementNode* node) {
    if (auto ret = dynamic_cast<const ReturnStatement*>(node)) {
        gen_return(ret);
    } else if (auto decl = dynamic_cast<const DeclStmt*>(node)) {
        gen_decl(decl);
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

void AST2IR::gen_block(const BlockStatement* node) {
    enter_scope();
    for (const auto& stmt : node->statements) {
        gen_statement(stmt.get());
    }
    exit_scope();
}

void AST2IR::gen_decl(const DeclStmt* node) {
    std::string ir_name = declare_var(node->name);

    Instruction inst(Opcode::ASSIGN);
    inst.result = Operand::var(ir_name);

    if (node->init) {
        inst.lhs = gen_expr(node->init.get());
    } else {
        inst.lhs = Operand::imm(0);
    }
    emit(inst);
}

void AST2IR::gen_expr_stmt(const ExprStmt* node) {
    gen_expr(node->expr.get());
}

void AST2IR::gen_return(const ReturnStatement* node) {
    Operand val = gen_expr(node->expr.get());
    Instruction inst(Opcode::RET);
    inst.lhs = val;
    emit(inst);
}

void AST2IR::gen_if(const IfStmt* node) {
    // 翻译条件表达式，结果在当前 block
    Operand cond = gen_expr(node->cond.get());

    // 创建三个新 block
    BasicBlock* then_bb = new_block(".L_then");
    BasicBlock* else_bb = new_block(".L_else");
    BasicBlock* end_bb  = new_block(".L_end");

    // 当前 block 加条件跳转
    Instruction jz(Opcode::JZ);
    jz.lhs = cond;
    jz.jump_label = else_bb->label;
    emit(jz);

    Instruction jmp_then(Opcode::JMP);
    jmp_then.jump_label = then_bb->label;
    emit(jmp_then);

    // 翻译 then 分支
    current_block_ = then_bb;
    gen_statement(node->then_branch.get());
    Instruction jmp_end1(Opcode::JMP);
    jmp_end1.jump_label = end_bb->label;
    emit(jmp_end1);

    // 翻译 else 分支
    current_block_ = else_bb;
    if (node->else_branch) {
        gen_statement(node->else_branch.get());
    }
    Instruction jmp_end2(Opcode::JMP);
    jmp_end2.jump_label = end_bb->label;
    emit(jmp_end2);

    // 后续语句在 end_block
    current_block_ = end_bb;
}

void AST2IR::gen_while(const WhileStmt* node) {
    BasicBlock* cond_bb = new_block(".L_while_cond");
    BasicBlock* body_bb = new_block(".L_while_body");
    BasicBlock* end_bb  = new_block(".L_while_end");

    // 当前 block 先跳转到 cond
    Instruction jmp_cond(Opcode::JMP);
    jmp_cond.jump_label = cond_bb->label;
    emit(jmp_cond);

    // 翻译 cond
    current_block_= cond_bb;
    Operand cond = gen_expr(node->cond.get());
    Instruction jz(Opcode::JZ);
    jz.lhs = cond;
    jz.jump_label = end_bb->label;
    emit(jz);

    Instruction jmp_body(Opcode::JMP);
    jmp_body.jump_label = body_bb->label;
    emit(jmp_body);

    // 翻译 body
    current_block_ = body_bb;
    gen_statement(node->body.get());
    Instruction jmp_back(Opcode::JMP);
    jmp_back.jump_label = cond_bb->label;
    emit(jmp_back);

    // 后续语句
    current_block_ = end_bb;
}



// ---------- 辅助函数 ----------

void AST2IR::enter_scope() {
    scope_stack_.push_back({});
}

void AST2IR::exit_scope() {
    if (!scope_stack_.empty()) {
        scope_stack_.pop_back();
    }
}

std::string AST2IR::lookup_var(const std::string& name) const {
    for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    throw std::runtime_error("undefined variable: " + name);
}

std::string AST2IR::declare_var(const std::string& name) {
    if (scope_stack_.empty()) {
        throw std::runtime_error("declaration outside any scope: " + name);
    }
    auto& current_scope = scope_stack_.back();
    if (current_scope.find(name) != current_scope.end()) {
        throw std::runtime_error("redefined variable: " + name);
    }
    current_scope[name] = name;
    return name;
}

std::string AST2IR::new_temp() {
    return "__t" + std::to_string(tmp_counter_++);
}

std::string AST2IR::new_label(const std::string& prefix) {
    return prefix + "_" + std::to_string(label_counter_++);
}

BasicBlock* AST2IR::new_block(const std::string& prefix) {
    auto block = std::make_unique<BasicBlock>();
    block->label = new_label(prefix);
    BasicBlock* ptr = block.get();
    current_func_->blocks.push_back(std::move(block));
    return ptr;
}

void AST2IR::emit(const Instruction& inst) {
    current_block_->insts.push_back(inst);
}

Opcode AST2IR::map_op(Tok op) const {
    switch (op) {
        case Tok::ADD: return Opcode::ADD;
        case Tok::SUB: return Opcode::SUB;
        case Tok::STAR: return Opcode::MUL;
        case Tok::SLASH: return Opcode::DIV;
        case Tok::PERCENT: return Opcode::REM;
        case Tok::LT: return Opcode::LT;
        case Tok::GT: return Opcode::GT;
        case Tok::LE: return Opcode::LE;
        case Tok::GE: return Opcode::GE;
        case Tok::EQ: return Opcode::EQ;
        case Tok::NE: return Opcode::NE;
        default: throw std::runtime_error("unsupported operator");
    }
}

} // namespace IR
