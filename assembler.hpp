#ifndef ASSEMBLER_HPP
#define ASSEMBLER_HPP
#include<string>
#include<vector>
#include<fstream>
#include<iostream>
#include<unordered_map>
#include<cstdint>
#include<cstdlib>

enum class Tok{IDENT, NUMBER, COMMA, LPAREN, RPAREN, COLON };

struct Token{
    Tok type;
    std::string text;
    int line_no;
    Token(Tok t, const std::string& v, int line) : type(t), text(v), line_no(line) {}
};

struct RawLine {
    std::vector<std::string> labels;
    std::string mnemonic;
    std::vector<std::string> operands;
    int line;
};

inline uint32_t get_instruction_size(const RawLine& raw_line){
    if(raw_line.mnemonic.empty()) return 0;
    return 4;
}

#include "assembler_macro.hpp"

class Assembler {
private:
    std::vector<std::string> lines;
    std::string path;
    std::string output_path;
    std::vector<uint32_t> machine_codes;
    std::vector<std::vector<Token>> tokens;
    std::vector<RawLine> raw_lines;
    std::unordered_map<std::string, uint32_t> label_addresses;
    void read_file_lines();
    void Lexer();
    void Parser();
    void Pass1();
    void Pass2();
    void Emit();
public:
    Assembler(const std::string& file_path, const std::string& out_path)
        : path(file_path), output_path(out_path) {}
    void assemble() {
        read_file_lines();
        Lexer();
        Parser();
        Pass1();
        Pass2();
        Emit();
    }
    const std::vector<uint32_t>& get_machine_codes() const { return machine_codes; }
};

#endif // ASSEMBLER_HPP
