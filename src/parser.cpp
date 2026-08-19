#include "parser.hpp"

using namespace AST;

int Parser::left_binding_power(Tok type) {
    switch (type) {
        case Tok::ASSIGN: return 1;
        case Tok::EQ: case Tok::NE: return 4;
        case Tok::LT: case Tok::LE: case Tok::GE: case Tok::GT: return 5;
        case Tok::ADD: case Tok::SUB: return 10;
        case Tok::STAR: case Tok::SLASH: case Tok::PERCENT: return 20;
        default: return -1;
    }
}

int Parser::right_binding_power(Tok type) {
    switch (type) {
        case Tok::ASSIGN: return 1;
        case Tok::EQ: case Tok::NE: return 5;
        case Tok::LT: case Tok::LE: case Tok::GE: case Tok::GT: return 6;
        case Tok::ADD: case Tok::SUB: return 11;
        case Tok::STAR: case Tok::SLASH: case Tok::PERCENT: return 21;
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
    case Tok::ASSIGN: {
        auto ident = dynamic_cast<IdentifierNode*>(left.get());
        if (!ident) {
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
        std::unique_ptr<ExprNode> oper = parse_expression(21);
        return std::make_unique<UnaryExpr>(Tok::SUB, std::move(oper));
    }
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
    default:
        throw std::runtime_error("unknown nud token");
    }
}

std::unique_ptr<StatementNode> Parser::parse_declaration_statement() {
    std::unique_ptr<Type> ty = parse_type_tok();
    const Token& token = expect(Tok::IDENT, "expect variable name");
    std::string name = token.text;

    std::unique_ptr<ExprNode> init = nullptr;
    if (match(Tok::ASSIGN)) {
        init = parse_expression();
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
    } else if (peek().type == Tok::KW_INT || peek().type == Tok::KW_FLOAT) {
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
    }
    // 表达式语句
    std::unique_ptr<ExprNode> expr = parse_expression();
    expect(Tok::SEMICOLON, "Expected ';'");
    return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<ReturnStatement> Parser::parse_return_statement() {
    std::unique_ptr<ExprNode> expr = parse_expression();
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
        const Token& name_token = expect(Tok::IDENT, "Expected parameter name");
        if (seen.count(name_token.text)) {
            throw std::runtime_error(
                "duplicate parameter '" + name_token.text +
                "' at line " + std::to_string(name_token.line_no)
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
        std::unique_ptr<FunctionNode> func = parse_function();
        program->functions.push_back(std::move(func));
    }
    return program;
}
