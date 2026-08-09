#include"compiler.hpp"
void CodeGen::gen_while(const WhileStmt* node) {
    std::string start_label = new_label(".L_while_start_");
    std::string end_label   = new_label(".L_while_end_");

    emit(start_label + ":");
    gen_expr(node->cond.get());
    emit("lw t0, 0(sp)");
    emit("addi sp, sp, 4");
    emit("beq t0, zero, " + end_label);
    gen_statement(node->body.get());
    emit("j " + start_label);
    emit(end_label + ":");
}
void CodeGen::gen_if(const IfStmt* node){
    gen_expr(node->cond.get());
    emit("lw t0, 0(sp)");
    emit("addi sp, sp, 4");
    if(node->else_branch){
        std::string else_label = new_label(".L_else_");
        std::string end_label  = new_label(".L_end_");
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

int CodeGen::lookup_variable(const std::string& name){
    for(auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it){
        auto found = it->find(name);
        if(found != it->end()){
            return found->second;
        }
    }
    throw std::runtime_error("undefined variable " + name);
}
int CodeGen::count_decl_with_size(const BlockStatement* block){
    int count = 0;
    for(const std::unique_ptr<StatementNode>& stat : block->statements){
        if(dynamic_cast<const DeclStmt*>(stat.get())){
            count += 4;
        } else if(auto bloc = dynamic_cast<const BlockStatement*>(stat.get())){
            count += count_decl_with_size(bloc);
        } else if(auto ifs = dynamic_cast<const IfStmt*>(stat.get())){
            if(auto then_blk = dynamic_cast<const BlockStatement*>(ifs->then_branch.get())){
                count += count_decl_with_size(then_blk);
            }
            if(ifs->else_branch){
                if(auto else_blk = dynamic_cast<const BlockStatement*>(ifs->else_branch.get())){
                    count += count_decl_with_size(else_blk);
                }
            }
        } else if(auto whs = dynamic_cast<const WhileStmt*>(stat.get())){
            if(auto body_blk = dynamic_cast<const BlockStatement*>(whs->body.get())){
                count += count_decl_with_size(body_blk);
            }
        }
    }
    return count;
}
int Parser::left_binding_power(Tok type) {
    switch(type) {
        case Tok::ASSIGN: return 1;
        case Tok::EQ: case Tok::NE: return 4;
        case Tok::LT: case Tok::LE: case Tok::GE: case Tok::GT: return 5;
        case Tok::ADD: case Tok::SUB: return 10;
        case Tok::STAR: case Tok::SLASH: case Tok::PERCENT: return 20;
        default: return -1;  // 非运算符必须返回负数，否则 0 < 0 为 false 会误入循环
    }
}

int Parser::right_binding_power(Tok type) {
    switch(type) {
        case Tok::ASSIGN: return 1;
        case Tok::EQ: case Tok::NE: return 5;
        case Tok::LT: case Tok::LE: case Tok::GE: case Tok::GT: return 6;
        case Tok::ADD: case Tok::SUB: return 11;
        case Tok::STAR: case Tok::SLASH: case Tok::PERCENT: return 21;
        default: return -1;
    }
}

std::unique_ptr<ExprNode> Parser::led(const Token& token, std::unique_ptr<ExprNode> left, std::unique_ptr<ExprNode> right){
    switch(token.type){
    case Tok::ADD:
    case Tok::SUB:
    case Tok::STAR:
    case Tok::SLASH:
    case Tok::PERCENT:
    case Tok::EQ: case Tok::NE:
    case Tok::LT: case Tok::LE: case Tok::GE: case Tok::GT:
        return std::make_unique<BinaryExpr>(token.type,std::move(left),std::move(right));
    case Tok::ASSIGN:{
        auto ident = dynamic_cast<IdentifierNode*>(left.get());
        if(!ident){
            throw std::runtime_error("left side of assignment must be a variable");
        }
        auto ident_ptr = std::unique_ptr<IdentifierNode>(
            static_cast<IdentifierNode*>(left.release())
        );
        return std::make_unique<AssignmentExpr>(std::move(ident_ptr), std::move(right));
    }
    default:
        throw std::runtime_error("unknown led token");
    }
}

std::unique_ptr<ExprNode> Parser::nud(const Token& token){
    switch(token.type){
    case Tok::NUMBER:
        return std::make_unique<NumberNode>(std::stoi(token.text));
    case Tok::LPAREN:{
        std::unique_ptr<ExprNode> result = parse_expression(0);
        expect(Tok::RPAREN,"expect corressponding rparen");
        return result;
    }
    case Tok::SUB:{
        std::unique_ptr<ExprNode> oper = parse_expression(21);
        return std::make_unique<UnaryExpr>(Tok::SUB,std::move(oper)); 
    }
    case Tok::IDENT:{
        return std::make_unique<IdentifierNode>(token.text);
    }
    default:
        throw std::runtime_error("unknown nud token");
    }
}

std::unique_ptr<StatementNode> Parser::parse_declaration_statement(){
    // int 已经被 parse_statement 的 match 吃掉了
    const Token& token = expect(Tok::IDENT, "expect variable name");
    std::string name = token.text;

    std::unique_ptr<ExprNode> init = nullptr;
    if (match(Tok::ASSIGN)) {
        init = parse_expression();
    }

    expect(Tok::SEMICOLON, "Expected ';'");
    return std::make_unique<DeclStmt>(name, std::move(init));
}

std::unique_ptr<ExprNode> Parser::parse_expression(int min_bp) {
    Token token = advance();
    std::unique_ptr<ExprNode> left = nud(token);
    while (true) {
        const Token& next = peek();
        int lbp = left_binding_power(next.type);
        if (lbp < min_bp) break;
        advance(); 
        int rbp = right_binding_power(next.type);
        std::unique_ptr<ExprNode> right = parse_expression(rbp);

        left = led(next, std::move(left), std::move(right));
    }
    return left;
}

std::unique_ptr<StatementNode> Parser::parse_statement(){
    if(match(Tok::KW_RETURN)){
        return parse_return_statement();
    } else if(match(Tok::KW_INT)){
        return parse_declaration_statement();
    } else if(match(Tok::KW_IF)){
        expect(Tok::LPAREN, "expect lparen after if");
        std::unique_ptr<ExprNode> cond = parse_expression();
        expect(Tok::RPAREN, "expect rparen after if");
        std::unique_ptr<StatementNode> then_block = parse_statement();
        std::unique_ptr<StatementNode> else_block = nullptr;
        if(match(Tok::KW_ELSE)){
            else_block = parse_statement();
        }
        return std::make_unique<IfStmt>(std::move(cond), std::move(then_block), std::move(else_block));
    } else if(match(Tok::LCURLY)){
        std::unique_ptr<BlockStatement> block = parse_block();
        expect(Tok::RCURLY, "Expected '}'");
        return block;
    } else if(match(Tok::KW_WHILE)){
        expect(Tok::LPAREN, "expect lparen after while");
        std::unique_ptr<ExprNode> cond = parse_expression();
        expect(Tok::RPAREN, "expect rparen after while");
        std::unique_ptr<StatementNode> body = parse_statement();
        return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
    }
    // 表达式语句
    std::unique_ptr<ExprNode> expr = parse_expression();
    expect(Tok::SEMICOLON, "Expected ';'");
    return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<ReturnStatement> Parser::parse_return_statement(){
    std::unique_ptr<ExprNode> expr = parse_expression();
    expect(Tok::SEMICOLON, "Expected ';' after return statement");
    return std::make_unique<ReturnStatement>(std::move(expr));
}

std::unique_ptr<BlockStatement> Parser::parse_block(){
    std::unique_ptr<BlockStatement> block = std::make_unique<BlockStatement>();
    while(pos < tokens.size() && tokens[pos].type != Tok::RCURLY){
        if(match(Tok::LCURLY)){
            std::unique_ptr<BlockStatement> nested = parse_block();
            expect(Tok::RCURLY,"expected '}' in nested block");
            block->statements.push_back(std::move(nested));
        } else {
            std::unique_ptr<StatementNode> stmt = parse_statement();
            block->statements.push_back(std::move(stmt));
        }
    }
    return block;
}

std::vector<Param> Parser::parse_parameters(){
    std::vector<Param> params;
    if(peek().type == Tok::RPAREN){
        return params; // No parameters
    }
    do {
        expect(Tok::KW_INT, "Expected 'int' in parameter declaration");
        const Token& name_token = expect(Tok::IDENT, "Expected parameter name");
        params.push_back({name_token.text, TypeKind::INT});
    } while(match(Tok::COMMA));
    return params;
}

std::unique_ptr<FunctionNode> Parser::parse_function(){
    expect(Tok::KW_INT, "Expected 'int' at the beginning of function declaration");
    const Token& name_token = expect(Tok::IDENT, "Expected function name");
    std::string func_name = name_token.text;
    expect(Tok::LPAREN, "Expected '(' after function name");
    std::vector<Param> params = parse_parameters();
    expect(Tok::RPAREN, "Expected ')' after parameters");
    expect(Tok::LCURLY, "Expected '{' at the beginning of function body");
    std::unique_ptr<BlockStatement> body = parse_block();
    expect(Tok::RCURLY, "Expected '}' at the end of function body");
    return std::make_unique<FunctionNode>(func_name, TypeKind::INT, std::move(params), std::move(body));
}

std::unique_ptr<ProgramNode> Parser::parse(){
    return parse_program();
}

std::unique_ptr<ProgramNode> Parser::parse_program(){
    std::unique_ptr<ProgramNode> program = std::make_unique<ProgramNode>();
    while(pos < tokens.size() && tokens[pos].type != Tok::EOF_TOK){
        std::unique_ptr<FunctionNode> func = parse_function();
        program->functions.push_back(std::move(func));
    }
    return program;
}

void CodeGen::gen_expr(const ExprNode* node){
    if(auto num = dynamic_cast<const NumberNode*>(node)){
        emit("addi t0, zero, " + std::to_string(num->value));
        emit("addi sp, sp, -4");
        emit("sw t0, 0(sp)");
    } else if(auto unary = dynamic_cast<const UnaryExpr*>(node)){
        gen_expr(unary->operand.get());
        emit("lw t0, 0(sp)");
        emit("addi sp, sp, 4");
        switch(unary->op){
        case Tok::SUB:
            emit("sub t0, zero, t0");
            break;
        default:
            throw std::runtime_error("unknown unary operation");
        }
        emit("addi sp, sp, -4");
        emit("sw t0, 0(sp)");
    } else if(auto binary = dynamic_cast<const BinaryExpr*>(node)){
        gen_expr(binary->left.get());
        gen_expr(binary->right.get());

        emit("lw t1, 0(sp)"); //right
        emit("addi sp, sp, 4");
        emit("lw t0, 0(sp)"); //left
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
    } else if(auto iden = dynamic_cast<const IdentifierNode*>(node)){
        gen_identifier(iden);
    } else if(auto assi = dynamic_cast<const AssignmentExpr*>(node)){
        gen_assignment(assi);
    }
    else {
        throw std::runtime_error("unknown expression node");
    }
}

void CodeGen::gen_return(const ReturnStatement* node){
    gen_expr(node->expr.get());
    emit("lw a0, 0(sp)");
    emit("addi sp, sp, 4");

    emit("lw s0, -4(s0)");
    emit("addi sp, sp, " + std::to_string(frame_size));

    emit("li a7, 10");
    emit("ecall");
}

void CodeGen::gen_statement(const StatementNode* node){
    if(auto ret = dynamic_cast<const ReturnStatement*>(node)){
        gen_return(ret);
    } else if(auto dec = dynamic_cast<const DeclStmt*>(node)){
        gen_decl(dec);
    } else if(auto expr = dynamic_cast<const ExprStmt*>(node)){
        gen_expr_stmt(expr);
    } else if(auto blk = dynamic_cast<const BlockStatement*>(node)){
        gen_block(blk);
    } else if(auto ifs = dynamic_cast<const IfStmt*>(node)){
        gen_if(ifs);
    } else if(auto whs = dynamic_cast<const WhileStmt*>(node)){
        gen_while(whs);
    }
    else {
        throw std::runtime_error("unknown statement");
    }
}

void CodeGen::gen_block(const BlockStatement* node){
    enter_scope();
    for(const std::unique_ptr<StatementNode>& snode : node->statements){
        gen_statement(snode.get());
    }
    exit_scope();
}

void CodeGen::gen_function(const FunctionNode* node){
    //emit(".globl " + node->name);
    scope_stack.clear();
    next_offset = -8;
    frame_size = calculate_frame_size(node);
    emit(node->name + ":");
    emit("addi sp, sp, " + std::to_string(-frame_size));
    emit("sw s0, " + std::to_string(frame_size - 4) + "(sp)");
    emit("addi s0, sp, " + std::to_string(frame_size));
    enter_scope();
    gen_block(node->body.get());
    exit_scope();
}

void CodeGen::gen_program(const ProgramNode* node) {
    //emit(".text");
    for (const std::unique_ptr<FunctionNode>& func_ptr : node->functions) {
        gen_function(func_ptr.get());
    }
}

void CodeGen::gen_decl(const DeclStmt* node){
    int offset = next_offset;
    if(scope_stack.empty()){
        throw std::runtime_error("decleration outside any scope " + node->name);
    }
    std::unordered_map<std::string,int>& current_scope = scope_stack.back();
    if(current_scope.find(node->name) != current_scope.end()){
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

void CodeGen::gen_identifier(const IdentifierNode* node){
    int offset = lookup_variable(node->name);

    emit("lw t0, " + std::to_string(offset) + "(s0)");
    emit("addi sp, sp, -4");
    emit("sw t0, 0(sp)");
}

void CodeGen::gen_assignment(const AssignmentExpr* node){
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

void CodeGen::generate(ASTNode* ast, std::ostream& out){
    const ProgramNode* program = dynamic_cast<const ProgramNode*>(ast);
    if(!program){
        throw std::runtime_error("root must be ProgramNode");
    }
    instructions.clear();
    gen_program(program);
    for(const std::string& line : instructions){
        out << line << "\n";
    }
}

void Compiler::read_source_file() {
    std::ifstream file(source_path);
    if(!file) {
        std::cerr << "Error: cannot open source file '" << source_path << "'\n";
        has_error = true;
        return;
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    source_code = oss.str();
}

void Compiler::Lexer() {
    int line_no = 1, col_no = 1;
    for(size_t i = 0; i < source_code.size();){
        unsigned char c = static_cast<unsigned char>(source_code[i]);
        if(c == '\n'){
            ++line_no; col_no = 1; ++i; continue;
        }
        if(std::isspace(c)){
            ++i; ++col_no; continue;
        }
        switch(c){
        case '+': tokens.push_back({Tok::ADD, "+", line_no, col_no}); ++i; ++col_no; break;
        case '-': tokens.push_back({Tok::SUB, "-", line_no, col_no}); ++i; ++col_no; break;
        case '*': tokens.push_back({Tok::STAR, "*", line_no, col_no}); ++i; ++col_no; break;
        case '%': tokens.push_back({Tok::PERCENT, "%", line_no, col_no}); ++i; ++col_no; break;
        case '(': tokens.push_back({Tok::LPAREN, "(", line_no, col_no}); ++i; ++col_no; break;
        case ')': tokens.push_back({Tok::RPAREN, ")", line_no, col_no}); ++i; ++col_no; break;
        case '{': tokens.push_back({Tok::LCURLY, "{", line_no, col_no}); ++i; ++col_no; break;
        case '}': tokens.push_back({Tok::RCURLY, "}", line_no, col_no}); ++i; ++col_no; break;
        case ';': tokens.push_back({Tok::SEMICOLON, ";", line_no, col_no}); ++i; ++col_no; break;
        case ',': tokens.push_back({Tok::COMMA, ",", line_no, col_no}); ++i; ++col_no; break;
        case '=': 
            if(i + 1 < source_code.size() && source_code[i + 1] == '='){
                tokens.push_back({Tok::EQ, "==", line_no, col_no}); i+=2; col_no+=2; break;
            } else {
                tokens.push_back({Tok::ASSIGN, "=", line_no, col_no}); ++i; ++col_no; break;
            }
        case '/':
            //  //
            if(i + 1 < source_code.size() && source_code[i + 1] == '/'){
                while(i < source_code.size() && source_code[i] != '\n') ++i;
                break;
            }
            //  /* */
            if(i + 1 < source_code.size() && source_code[i+1] == '*'){
                i += 2;
                bool closed = false;
                while(i + 1 < source_code.size()){
                    if(source_code[i] == '*' && source_code[i+1] == '/'){ closed = true; break; }
                    if(source_code[i] == '\n'){ ++line_no; col_no = 1; }
                    ++i;
                }
                if(!closed){
                    std::cerr << "Error: unterminated block comment at line " << line_no << "\n";
                    has_error = true;
                    break;
                }
                i += 2;  // 跳过 */
                break;
            }
            tokens.push_back({Tok::SLASH, "/", line_no, col_no});
            ++i; ++col_no;
            break;
        case '<':
            if(i + 1 < source_code.size() && source_code[i + 1] == '='){
                tokens.push_back({Tok::LE, "<=", line_no, col_no});
                i += 2; col_no += 2;
            } else {
                tokens.push_back({Tok::LT, "<", line_no, col_no});
                ++i; ++col_no;
            }
            break;
        case '>':
            if(i + 1 < source_code.size() && source_code[i + 1] == '='){
                tokens.push_back({Tok::GE, ">=", line_no, col_no});
                i += 2; col_no += 2;
            } else {
                tokens.push_back({Tok::GT, ">", line_no, col_no});
                ++i; ++col_no;
            }
            break;
        case '!':
            if(i + 1 < source_code.size() && source_code[i + 1] == '='){
                tokens.push_back({Tok::NE, "!=", line_no, col_no});
                i += 2; col_no += 2;
            } else {
                std::cerr << "Error: unexpected character '!' at line " << line_no << " col " << col_no << "\n";
                has_error = true;
                ++i; ++col_no;
            }
            break;
        default:
            if(std::isalpha(c) || c == '_'){
                size_t start = i;
                while(i < source_code.size() && (std::isalnum(static_cast<unsigned char>(source_code[i])) || source_code[i] == '_')) ++i;
                std::string ident = source_code.substr(start, i - start);
                auto it = keywords.find(ident);
                if(it != keywords.end()){
                    tokens.push_back({it->second, ident, line_no, col_no});
                } else {
                    tokens.push_back({Tok::IDENT, ident, line_no, col_no});
                }
                col_no += static_cast<int>(i - start);
            } else if(std::isdigit(c)){
                size_t start = i;
                while(i < source_code.size() && std::isdigit(static_cast<unsigned char>(source_code[i]))) ++i;
                tokens.push_back({Tok::NUMBER, source_code.substr(start, i - start), line_no, col_no});
                col_no += static_cast<int>(i - start);
            } else {
                std::cerr << "Error: unexpected character '" << c << "' at line " << line_no << " col " << col_no << "\n";
                has_error = true;
                ++i; ++col_no;
            }
            break;  
        }

    }
    tokens.push_back({Tok::EOF_TOK, "", line_no, col_no});
}

void Compiler::compile() {
    read_source_file();
    if(has_error) return;

    Lexer();
    if(has_error) return;

    Parser parser(tokens);
    std::unique_ptr<ProgramNode> ast = parser.parse();

    std::ofstream out(output_path);
    if(!out) {
        std::cerr << "Error: cannot open output file '" << output_path << "'\n";
        has_error = true;
        return;
    }

    CodeGen codegen;
    codegen.generate(ast.get(), out);
}
