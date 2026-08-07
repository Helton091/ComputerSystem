#include"compiler.hpp"
int Parser::left_binding_power(Tok type) {
    switch(type) {
        case Tok::ADD: case Tok::SUB: return 10;
        case Tok::STAR: case Tok::SLASH: case Tok::PERCENT: return 20;
        default: return -1;  // 非运算符必须返回负数，否则 0 < 0 为 false 会误入循环
    }
}

int Parser::right_binding_power(Tok type) {
    switch(type) {
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
        return std::make_unique<BinaryExpr>(token.type,std::move(left),std::move(right));
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
    default:
        throw std::runtime_error("unknown nud token");
    }
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
    }
    throw std::runtime_error("Unknown statement");
}

std::unique_ptr<ReturnStatement> Parser::parse_return_statement(){
    std::unique_ptr<ExprNode> expr = parse_expression();
    expect(Tok::SEMICOLON, "Expected ';' after return statement");
    return std::make_unique<ReturnStatement>(std::move(expr));
}

std::unique_ptr<BlockStatement> Parser::parse_block(){
    std::unique_ptr<BlockStatement> block = std::make_unique<BlockStatement>();
    while(pos < tokens.size() && tokens[pos].type != Tok::RCURLY){
        std::unique_ptr<StatementNode> stmt = parse_statement();
        block->statements.push_back(std::move(stmt));
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

        emit("lw t1, 0(sp)");
        emit("addi sp, sp, 4");
        emit("lw t0, 0(sp)");
        emit("addi sp, sp, 4");

        switch (binary->op) {
        case Tok::ADD:    emit("add t0, t0, t1"); break;   
        case Tok::SUB:    emit("sub t0, t0, t1"); break;   
        case Tok::STAR:   emit("mul t0, t0, t1"); break;   
        case Tok::SLASH:  emit("div t0, t0, t1"); break;   
        case Tok::PERCENT: emit("rem t0, t0, t1"); break; 
        default: throw std::runtime_error("unknown binary op");
        }
    
        emit("addi sp, sp, -4");
        emit("sw t0, 0(sp)");
    } else {
        throw std::runtime_error("unknown expression node");
    }
}

void CodeGen::gen_return(const ReturnStatement* node){
    gen_expr(node->expr.get());
    emit("lw a0, 0(sp)");
    emit("addi sp, sp, 4");
    emit("li a7, 10");
    emit("ecall");
}

void CodeGen::gen_statement(const StatementNode* node){
    if(auto ret = dynamic_cast<const ReturnStatement*>(node)){
        gen_return(ret);
    } else {
        throw std::runtime_error("unknown statement");
    }
}

void CodeGen::gen_block(const BlockStatement* node){
    for(const std::unique_ptr<StatementNode>& snode : node->statements){
        gen_statement(snode.get());
    }
}

void CodeGen::gen_function(const FunctionNode* node){
    //emit(".globl " + node->name);
    emit(node->name + ":");
    gen_block(node->body.get());
}

void CodeGen::gen_program(const ProgramNode* node) {
    //emit(".text");
    for (const std::unique_ptr<FunctionNode>& func_ptr : node->functions) {
        gen_function(func_ptr.get());
    }
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
        case '=': tokens.push_back({Tok::ASSIGN, "=", line_no, col_no}); ++i; ++col_no; break;
        case '(': tokens.push_back({Tok::LPAREN, "(", line_no, col_no}); ++i; ++col_no; break;
        case ')': tokens.push_back({Tok::RPAREN, ")", line_no, col_no}); ++i; ++col_no; break;
        case '{': tokens.push_back({Tok::LCURLY, "{", line_no, col_no}); ++i; ++col_no; break;
        case '}': tokens.push_back({Tok::RCURLY, "}", line_no, col_no}); ++i; ++col_no; break;
        case ';': tokens.push_back({Tok::SEMICOLON, ";", line_no, col_no}); ++i; ++col_no; break;
        case ',': tokens.push_back({Tok::COMMA, ",", line_no, col_no}); ++i; ++col_no; break;
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