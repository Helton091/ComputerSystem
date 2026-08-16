#pragma once
#include "token.hpp"
#include <string>
#include <vector>

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();
    bool has_error() const { return has_error_; }

private:
    const std::string& source_;
    bool has_error_ = false;
};
