#pragma once
#include "token.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

class Compiler {
public:
    std::string source_path;
    std::string output_path;
    std::string source_code;
    bool has_error = false;
    std::vector<Token> tokens;

    Compiler(const std::string& src_path, const std::string& out_path)
        : source_path(src_path), output_path(out_path) {}
    void compile();

private:
    void read_source_file();
};
