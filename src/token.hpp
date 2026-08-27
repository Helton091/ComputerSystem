#pragma once
#include <string>
#include <unordered_map>

enum class Tok {
    IDENT, INT_NUMBER, FLOAT_NUMBER,EOF_TOK,

    KW_INT, KW_RETURN, KW_FLOAT, KW_VOID, KW_NULLPTR,

    LPAREN, RPAREN, LCURLY, RCURLY, LBRACKET, RBRACKET, 
    SEMICOLON, COMMA,

    ADD, SUB, STAR, SLASH, PERCENT, AMPERSAND /* & */,
    AND_AND,  
    ASSIGN,
    EQ, NE, LT, GT, LE, GE,
    KW_IF, KW_ELSE, KW_WHILE, KW_FOR, 
};

inline std::unordered_map<std::string, Tok> keywords = {
    {"int", Tok::KW_INT},
    {"return", Tok::KW_RETURN},
    {"if", Tok::KW_IF},
    {"else", Tok::KW_ELSE},
    {"while", Tok::KW_WHILE},
    {"float", Tok::KW_FLOAT},
    {"void", Tok::KW_VOID},
    {"nullptr", Tok::KW_NULLPTR},
    {"for", Tok::KW_FOR},
};

struct Token {
    Tok type;
    std::string text;
    int line_no;
    int col_no;
};
