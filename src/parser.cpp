#include "parser.hpp"

using namespace AST;

std::unique_ptr<AST::Type> Parser::parse_type_tok(const std::string& err_msg){
    const Token& token = advance();
    std::unique_ptr<AST::Type> base = nullptr;
    switch(token.type){
    case Tok::KW_INT:
        base = std::make_unique<AST::IntType>();
        break;
    case Tok::KW_FLOAT:
        base = std::make_unique<AST::FloatType>();
        break;
    case Tok::KW_VOID:
        base = std::make_unique<AST::VoidType>();
        break;
    default:
        throw std::runtime_error(
            "[Parser] " + err_msg + " at line " + std::to_string(token.line_no) +
            ", col " + std::to_string(token.col_no)
        );
    } 
    while(match(Tok::STAR)) base = std::make_unique<PointerType>(std::move(base));
    return base;
}

int Parser::left_binding_power(Tok type) {
    switch (type) {
        case Tok::ASSIGN: return BP_ASSIGN;
        case Tok::EQ: case Tok::NE: return BP_EQ;
        case Tok::LT: case Tok::LE: case Tok::GE: case Tok::GT: return BP_CMP;
        case Tok::ADD: case Tok::SUB: return BP_ADD;
        case Tok::STAR: case Tok::SLASH: case Tok::PERCENT: return BP_MUL;
        case Tok::LBRACKET: return BP_PREFIX+1;
        default: return -1;
    }
}

int Parser::right_binding_power(Tok type) {
    switch (type) {
        case Tok::ASSIGN: return BP_ASSIGN;     
        case Tok::EQ: case Tok::NE: return BP_EQ + 1;
        case Tok::LT: case Tok::LE: case Tok::GE: case Tok::GT: return BP_CMP + 1;
        case Tok::ADD: case Tok::SUB: return BP_ADD + 1;
        case Tok::STAR: case Tok::SLASH: case Tok::PERCENT: return BP_MUL + 1;
        case Tok::LBRACKET: return 0;
        default: return -1;
    }
}

std::unique_ptr<ExprNode> Parser::led(const Token& token, std::unique_ptr<ExprNode> left, std::unique_ptr<ExprNode> right) {
    switch (token.type) {
    case Tok::ADD:
    case Tok::SUB:
    case Tok::STAR:
    case Tok::SLASH:
    case Tok::PERCENT:
    case Tok::EQ: case Tok::NE:
    case Tok::LT: case Tok::LE: case Tok::GE: case Tok::GT:
        return std::make_unique<BinaryExpr>(token.type, std::move(left), std::move(right));
    case Tok::LBRACKET:{
        std::unique_ptr<IndexExpr> result = std::make_unique<IndexExpr>(std::move(left),std::move(right));
        expect(Tok::RBRACKET,"[parser] expect corressponding ] in index expression");
        return result;
    }
        
    case Tok::ASSIGN: {
        bool is_lvalue = dynamic_cast<IdentifierNode*>(left.get()) != nullptr ||
                        (dynamic_cast<UnaryExpr*>(left.get()) != nullptr &&
                        dynamic_cast<UnaryExpr*>(left.get())->op == Tok::STAR) || 
                        dynamic_cast<IndexExpr*>(left.get()) != nullptr;
        if (!is_lvalue) {
            throw std::runtime_error(
                "[Parser] left side of assignment must be a variable, dereference or array element at line " +
                std::to_string(token.line_no) + ", col " + std::to_string(token.col_no)
            );
        }
        return std::make_unique<AssignmentExpr>(std::move(left), std::move(right));
    }
    default:
        throw std::runtime_error("[Parser] unknown led token '" + token.text + "' at line " + std::to_string(token.line_no) + ", col " + std::to_string(token.col_no));
    }
}

std::unique_ptr<ExprNode> Parser::nud(const Token& token) {
    switch (token.type) {
    case Tok::INT_NUMBER:
        return std::make_unique<IntNumberNode>(std::stoi(token.text));
    case Tok::FLOAT_NUMBER:
        return std::make_unique<FloatNumberNode>(std::stof(token.text));
    case Tok::LPAREN: {
        std::unique_ptr<ExprNode> result = parse_expression(0);
        expect(Tok::RPAREN, "expect corressponding rparen");
        return result;
    }
    case Tok::SUB: {
        std::unique_ptr<ExprNode> oper = parse_expression(BP_PREFIX);
        return std::make_unique<UnaryExpr>(Tok::SUB, std::move(oper));
    }
    case Tok::AMPERSAND:
        return std::make_unique<UnaryExpr>(Tok::AMPERSAND,parse_expression(BP_PREFIX));
    case Tok::STAR:
        return std::make_unique<UnaryExpr>(Tok::STAR,parse_expression(BP_PREFIX));
    case Tok::IDENT: {
        if (peek().type == Tok::LPAREN) {
            advance();
            if (match(Tok::RPAREN)) return std::make_unique<CallExpr>(token.text, std::vector<std::unique_ptr<ExprNode>>{});

            std::vector<std::unique_ptr<ExprNode>> args;
            do {
                args.push_back(parse_expression());
            } while (match(Tok::COMMA));
            expect(Tok::RPAREN, "expect rparen in parameters list");
            return std::make_unique<CallExpr>(token.text, std::move(args));
        }
        return std::make_unique<IdentifierNode>(token.text);
    }
    case Tok::KW_NULLPTR:
        return std::make_unique<NullPointerNode>();
    default:
        throw std::runtime_error("[Parser] unknown nud token '" + token.text + "' at line " + std::to_string(token.line_no) + ", col " + std::to_string(token.col_no));
    }
}

std::unique_ptr<DeclStmt> Parser::parse_declaration_statement() {
    std::unique_ptr<Type> ty = parse_type_tok();
    if(dynamic_cast<VoidType*>(ty.get())) throw std::runtime_error("[parser] declaration of variable should not be void type" );
    const Token& token = expect(Tok::IDENT, "expect variable name");
    std::string name = token.text;

    bool is_array = false;
    if(match(Tok::LBRACKET)){
        const Token& tok = expect(Tok::INT_NUMBER,"expect array length");
        int arr_len = std::stoi(tok.text);
        if(arr_len <= 0) throw std::runtime_error("[parser] array length should be a positive number, but got " + std::to_string(arr_len) + " at line " + std::to_string(tok.line_no) + ", col " + std::to_string(tok.col_no));
        expect(Tok::RBRACKET,"[parser] array declaration should have a pair of [], missing ]");
        ty = std::make_unique<ArrayType>(std::move(ty),arr_len);
        is_array = true;
    }

    if(match(Tok::LBRACKET)) throw std::runtime_error("[parser] unsupported language trait: multi-dimension array at line " + std::to_string(previous().line_no) + ", col " + std::to_string(previous().col_no));

    std::unique_ptr<ExprNode> init = nullptr;
    if (match(Tok::ASSIGN)) {
        if(!is_array) init = parse_expression();
        else throw std::runtime_error("[parser] don't support array initialization yet at line " + std::to_string(token.line_no) + ", col " + std::to_string(token.col_no));
    }

    expect(Tok::SEMICOLON, "Expected ';'");
    return std::make_unique<DeclStmt>(TypedName(std::move(name),std::move(ty)), std::move(init));
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

std::unique_ptr<StatementNode> Parser::parse_statement() {
    if (match(Tok::KW_RETURN)) {
        return parse_return_statement();
    } else if (peek().type == Tok::KW_INT || peek().type == Tok::KW_FLOAT || peek().type == Tok::KW_VOID) {
        return parse_declaration_statement();
    } else if (match(Tok::KW_IF)) {
        expect(Tok::LPAREN, "expect lparen after if");
        std::unique_ptr<ExprNode> cond = parse_expression();
        expect(Tok::RPAREN, "expect rparen after if");
        std::unique_ptr<StatementNode> then_block = parse_statement();
        std::unique_ptr<StatementNode> else_block = nullptr;
        if (match(Tok::KW_ELSE)) {
            else_block = parse_statement();
        }
        return std::make_unique<IfStmt>(std::move(cond), std::move(then_block), std::move(else_block));
    } else if (match(Tok::LCURLY)) {
        std::unique_ptr<BlockStatement> block = parse_block();
        expect(Tok::RCURLY, "Expected '}'");
        return block;
    } else if (match(Tok::KW_WHILE)) {
        expect(Tok::LPAREN, "expect lparen after while");
        std::unique_ptr<ExprNode> cond = parse_expression();
        expect(Tok::RPAREN, "expect rparen after while");
        std::unique_ptr<StatementNode> body = parse_statement();
        return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
    } else if (match(Tok::KW_FOR)){
        expect(Tok::LPAREN, "expect lparen after for");
        std::unique_ptr<StatementNode> init = nullptr;
        if(!match(Tok::SEMICOLON)){
            init = parse_statement();
        }
        std::unique_ptr<ExprNode> cond = nullptr;
        if(!match(Tok::SEMICOLON)){
            cond = parse_expression();
            expect(Tok::SEMICOLON,"expect the second semicolon in for");
        }
        std::unique_ptr<ExprNode> update = nullptr;
        if(!match(Tok::RPAREN)){
            update = parse_expression();
            expect(Tok::RPAREN,"expect right rparen in for");
        }
        std::unique_ptr<StatementNode> body = parse_statement();
        return std::make_unique<ForStmt>(std::move(init),std::move(cond),std::move(update),std::move(body));
    }
    // 表达式语句
    std::unique_ptr<ExprNode> expr = parse_expression();
    expect(Tok::SEMICOLON, "Expected ';'");
    return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<ReturnStatement> Parser::parse_return_statement() {
    std::unique_ptr<ExprNode> expr = nullptr;
    if(tokens.at(pos).type != Tok::SEMICOLON) expr = parse_expression();
    expect(Tok::SEMICOLON, "Expected ';' after return statement");
    return std::make_unique<ReturnStatement>(std::move(expr));
}

std::unique_ptr<BlockStatement> Parser::parse_block() {
    std::unique_ptr<BlockStatement> block = std::make_unique<BlockStatement>();
    while (pos < tokens.size() && tokens[pos].type != Tok::RCURLY) {
        if (match(Tok::LCURLY)) {
            std::unique_ptr<BlockStatement> nested = parse_block();
            expect(Tok::RCURLY, "expected '}' in nested block");
            block->statements.push_back(std::move(nested));
        } else {
            std::unique_ptr<StatementNode> stmt = parse_statement();
            block->statements.push_back(std::move(stmt));
        }
    }
    return block;
}

std::vector<Param> Parser::parse_parameters() {
    std::vector<Param> params;
    std::unordered_set<std::string> seen;
    if (peek().type == Tok::RPAREN) {
        return params; // No parameters
    }
    do {
        std::unique_ptr<Type> parsed_type = parse_type_tok();
        if(dynamic_cast<VoidType*>(parsed_type.get())) throw std::runtime_error("parameters' type should not be void type");
        const Token& name_token = expect(Tok::IDENT, "Expected parameter name");
        if (seen.count(name_token.text)) {
            throw std::runtime_error(
                "[Parser] duplicate parameter '" + name_token.text +
                "' at line " + std::to_string(name_token.line_no) +
                ", col " + std::to_string(name_token.col_no)
            );
        }
        seen.insert(name_token.text);
        params.push_back({name_token.text, std::move(parsed_type)});
    } while (match(Tok::COMMA));
    return params;
}

std::unique_ptr<FunctionNode> Parser::parse_function() {
    std::unique_ptr<Type> ret_type = parse_type_tok();
    const Token& name_token = expect(Tok::IDENT, "Expected function name");
    std::string func_name = name_token.text;
    expect(Tok::LPAREN, "Expected '(' after function name");
    std::vector<Param> params = parse_parameters();
    expect(Tok::RPAREN, "Expected ')' after parameters");
    expect(Tok::LCURLY, "Expected '{' at the beginning of function body");
    std::unique_ptr<BlockStatement> body = parse_block();
    expect(Tok::RCURLY, "Expected '}' at the end of function body");
    return std::make_unique<FunctionNode>(func_name, std::move(ret_type), std::move(params), std::move(body));
}

std::unique_ptr<ProgramNode> Parser::parse() {
    return parse_program();
}

std::unique_ptr<ProgramNode> Parser::parse_program() {
    std::unique_ptr<ProgramNode> program = std::make_unique<ProgramNode>();
    while (pos < tokens.size() && tokens[pos].type != Tok::EOF_TOK) {

        if (tokens[pos].type != Tok::KW_INT && tokens[pos].type != Tok::KW_FLOAT && tokens[pos].type != Tok::KW_VOID) {
            throw std::runtime_error(
                "[Parser] only declarations and functions are allowed at top level, but got '" +
                tokens[pos].text + "' at line " + std::to_string(tokens[pos].line_no) +
                ", col " + std::to_string(tokens[pos].col_no)
            );
        }

        // skip pointer stars to find the identifier (e.g. int *p; int **p;)
        size_t ident_pos = pos + 1;
        while (ident_pos < tokens.size() && tokens[ident_pos].type == Tok::STAR) ++ident_pos;

        if (ident_pos >= tokens.size() || tokens[ident_pos].type != Tok::IDENT) {
            throw std::runtime_error(
                "[Parser] expected identifier after type at line " +
                std::to_string(tokens[pos].line_no) + ", col " + std::to_string(tokens[pos].col_no)
            );
        }

        if(ident_pos + 1 < tokens.size() && tokens[ident_pos + 1].type == Tok::LPAREN){
            std::unique_ptr<FunctionNode> func = parse_function();
            program->functions.push_back(std::move(func));
        }
        else{
            std::unique_ptr<DeclStmt> decl = parse_declaration_statement();
            program->glob_vars.push_back(std::move(decl));
        }
    }
    return program;
}
