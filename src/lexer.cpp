#include "lexer.hpp"
#include <cctype>
#include <iostream>
#include <stdexcept>

Lexer::Lexer(const std::string& source) : source_(source) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    int line_no = 1, col_no = 1;
    for (size_t i = 0; i < source_.size();) {
        unsigned char c = static_cast<unsigned char>(source_[i]);
        if (c == '\n') {
            ++line_no; col_no = 1; ++i; continue;
        }
        if (std::isspace(c)) {
            ++i; ++col_no; continue;
        }
        switch (c) {
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
            if (i + 1 < source_.size() && source_[i + 1] == '=') {
                tokens.push_back({Tok::EQ, "==", line_no, col_no}); i += 2; col_no += 2; break;
            } else {
                tokens.push_back({Tok::ASSIGN, "=", line_no, col_no}); ++i; ++col_no; break;
            }
        case '/':
            //  //
            if (i + 1 < source_.size() && source_[i + 1] == '/') {
                while (i < source_.size() && source_[i] != '\n') ++i;
                break;
            }
            //  /* */
            if (i + 1 < source_.size() && source_[i + 1] == '*') {
                i += 2;
                bool closed = false;
                while (i + 1 < source_.size()) {
                    if (source_[i] == '*' && source_[i + 1] == '/') { closed = true; break; }
                    if (source_[i] == '\n') { ++line_no; col_no = 1; }
                    ++i;
                }
                if (!closed) {
                    std::cerr << "Error: unterminated block comment at line " << line_no << "\n";
                    has_error_ = true;
                    break;
                }
                i += 2;  // 跳过 */
                break;
            }
            tokens.push_back({Tok::SLASH, "/", line_no, col_no});
            ++i; ++col_no;
            break;
        case '<':
            if (i + 1 < source_.size() && source_[i + 1] == '=') {
                tokens.push_back({Tok::LE, "<=", line_no, col_no});
                i += 2; col_no += 2;
            } else {
                tokens.push_back({Tok::LT, "<", line_no, col_no});
                ++i; ++col_no;
            }
            break;
        case '>':
            if (i + 1 < source_.size() && source_[i + 1] == '=') {
                tokens.push_back({Tok::GE, ">=", line_no, col_no});
                i += 2; col_no += 2;
            } else {
                tokens.push_back({Tok::GT, ">", line_no, col_no});
                ++i; ++col_no;
            }
            break;
        case '!':
            if (i + 1 < source_.size() && source_[i + 1] == '=') {
                tokens.push_back({Tok::NE, "!=", line_no, col_no});
                i += 2; col_no += 2;
            } else {
                std::cerr << "Error: unexpected character '!' at line " << line_no << " col " << col_no << "\n";
                has_error_ = true;
                ++i; ++col_no;
            }
            break;
        default:
            if (std::isalpha(c) || c == '_') {
                size_t start = i;
                while (i < source_.size() && (std::isalnum(static_cast<unsigned char>(source_[i])) || source_[i] == '_')) ++i;
                std::string ident = source_.substr(start, i - start);
                auto it = keywords.find(ident);
                if (it != keywords.end()) {
                    tokens.push_back({it->second, ident, line_no, col_no});
                } else {
                    tokens.push_back({Tok::IDENT, ident, line_no, col_no});
                }
                col_no += static_cast<int>(i - start);
            } else if (std::isdigit(c) || (c == '.' && i + 1 < source_.size() && std::isdigit(static_cast<unsigned char>(source_[i + 1])))) {
                size_t start = i;
                bool is_float = false;
                while (i < source_.size() && std::isdigit(static_cast<unsigned char>(source_[i]))) {
                    ++i;
                }
                if (i < source_.size() && source_[i] == '.') {
                    is_float = true;
                    ++i;
                    while (i < source_.size() && std::isdigit(static_cast<unsigned char>(source_[i]))) {
                        ++i;
                    }
                }

                std::string text = source_.substr(start, i - start);
                if (is_float) {
                    tokens.push_back({Tok::FLOAT_NUMBER, text, line_no, col_no});
                } else {
                    tokens.push_back({Tok::INT_NUMBER, text, line_no, col_no});
                }
                col_no += static_cast<int>(i - start);
            } else {
                std::cerr << "Error: unexpected character '" << c << "' at line " << line_no << " col " << col_no << "\n";
                has_error_ = true;
                ++i; ++col_no;
            }
            break;
        }
    }
    tokens.push_back({Tok::EOF_TOK, "", line_no, col_no});
    return tokens;
}
