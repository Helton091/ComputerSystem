#include "assembler.hpp"

// 辅助函数（file-local）
static int32_t parse_imm(const std::string& s) {
    if(s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return static_cast<int32_t>(std::stoul(s, nullptr, 16));
    }
    return std::stoi(s);
}

static int64_t parse_imm64(const std::string& s) {
    if(s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return static_cast<int64_t>(std::stoull(s, nullptr, 16));
    }
    return std::stoll(s);
}

static uint32_t get_offset(const std::unordered_map<std::string, uint32_t>& labels,
                           const std::string& name, uint32_t current_addr, int line) {
    auto it = labels.find(name);
    if(it == labels.end()) {
        std::cerr << "Error: undefined label '" << name << "' at line " << line << "\n";
        return 0;
    }
    return it->second - current_addr;
}

void Assembler::Emit(){
    std::ofstream ofs(output_path, std::ios::binary);
    if(!ofs) {
        std::cerr << "Error: cannot open output file '" << output_path << "'\n";
        return;
    }
    for(uint32_t code : machine_codes) {
        ofs.put(static_cast<char>(code & 0xFF));
        ofs.put(static_cast<char>((code >> 8) & 0xFF));
        ofs.put(static_cast<char>((code >> 16) & 0xFF));
        ofs.put(static_cast<char>((code >> 24) & 0xFF));
    }
}

void Assembler::Pass2(){
    for(const RawLine& raw_line : raw_lines){
        if(raw_line.mnemonic.empty()) continue;
        uint32_t current_addr = machine_codes.size() * 4;

        if(raw_line.mnemonic == "nop"){
            machine_codes.push_back(0x00000013);
            continue;
        }
        if(raw_line.mnemonic == "li"){
            if(raw_line.operands.size() != 2){
                std::cerr << "Error: 'li' expects 2 operands at line " << raw_line.line << "\n";
                continue;
            }
            uint32_t rd = get_reg_index(raw_line.operands[0]);
            if(rd == 0xFFFFFFFF){
                std::cerr << "Error: Invalid register in 'li' at line " << raw_line.line << "\n";
                continue;
            }
            int64_t imm = parse_imm64(raw_line.operands[1]);
            if(imm >= -2048 && imm <= 2047){
                machine_codes.push_back(
                    (static_cast<uint32_t>(imm & 0xFFF) << 20) | (rd << 7) | 0x13
                );
            } else {
                int32_t upper = static_cast<int32_t>((imm + 0x800) >> 12);
                int32_t lower = static_cast<int32_t>(imm - (static_cast<int64_t>(upper) << 12));
                machine_codes.push_back(
                    (static_cast<uint32_t>(upper & 0xFFFFF) << 12) | (rd << 7) | 0x37
                );
                machine_codes.push_back(
                    (static_cast<uint32_t>(lower & 0xFFF) << 20) | (rd << 15) | (0 << 12) | (rd << 7) | 0x13
                );
            }
            continue;
        }
        if(raw_line.mnemonic == "mv"){
            if(raw_line.operands.size() != 2){
                std::cerr << "Error: 'mv' expects 2 operands at line " << raw_line.line << "\n";
                continue;
            }
            uint32_t rd = get_reg_index(raw_line.operands[0]);
            uint32_t rs1 = get_reg_index(raw_line.operands[1]);
            if(rd == 0xFFFFFFFF || rs1 == 0xFFFFFFFF){
                std::cerr << "Error: Invalid register in 'mv' at line " << raw_line.line << "\n";
                continue;
            }
            machine_codes.push_back(
                (rs1 << 15) | (rd << 7) | 0x13
            );
            continue;
        }
        if(raw_line.mnemonic == "j"){
            if(raw_line.operands.size() != 1){
                std::cerr << "Error: 'j' expects 1 operand at line " << raw_line.line << "\n";
                continue;
            }
            uint32_t imm = get_offset(label_addresses, raw_line.operands[0], current_addr, raw_line.line);
            machine_codes.push_back(
                (((imm >> 20) & 0x1) << 31) |
                (((imm >> 1) & 0x3FF) << 21) |
                (((imm >> 11) & 0x1) << 20) |
                (((imm >> 12) & 0xFF) << 12) |
                0x6F
            );
            continue;
        }
        if(raw_line.mnemonic == "call"){
            if(raw_line.operands.size() != 1){
                std::cerr << "Error: 'call' expects 1 operand at line " << raw_line.line << "\n";
                continue;
            }
            uint32_t imm = get_offset(label_addresses, raw_line.operands[0], current_addr, raw_line.line);
            machine_codes.push_back(
                (((imm >> 20) & 0x1) << 31) |
                (((imm >> 1) & 0x3FF) << 21) |
                (((imm >> 11) & 0x1) << 20) |
                (((imm >> 12) & 0xFF) << 12) |
                (1 << 7) | 0x6F
            );
            continue;
        }
        if(raw_line.mnemonic == "ret"){
            if(!raw_line.operands.empty()){
                std::cerr << "Error: 'ret' expects no operands at line " << raw_line.line << "\n";
                continue;
            }
            machine_codes.push_back(
                (1 << 15) | (0 << 12) | (0 << 7) | 0x67
            );
            continue;
        }

        const InstDef* inst_def = find_inst(raw_line.mnemonic);
        if(inst_def == nullptr){
            std::cerr << "Error: Unknown instruction '" << raw_line.mnemonic
                      << "' at line " << raw_line.line << "\n";
            continue;
        }

        switch(inst_def->fmt){
            case Fmt::R: {
                if(raw_line.operands.size() != 3){
                    std::cerr << "Error: R-type instruction '" << raw_line.mnemonic
                              << "' expects 3 operands at line " << raw_line.line << "\n";
                    continue;
                }
                uint32_t rd = get_reg_index(raw_line.operands[0]);
                uint32_t rs1 = get_reg_index(raw_line.operands[1]);
                uint32_t rs2 = get_reg_index(raw_line.operands[2]);
                if(rd == 0xFFFFFFFF || rs1 == 0xFFFFFFFF || rs2 == 0xFFFFFFFF){
                    std::cerr << "Error: Invalid register in instruction '" << raw_line.mnemonic
                              << "' at line " << raw_line.line << "\n";
                    continue;
                }
                machine_codes.push_back(
                    (inst_def->funct7 << 25) | (rs2 << 20) | (rs1 << 15) |
                    (inst_def->funct3 << 12) | (rd << 7) | inst_def->opcode
                );
                break;
            }
            case Fmt::I: {
                if(raw_line.mnemonic == "ecall"){
                    machine_codes.push_back(0x00000073);
                    break;
                }
                if(raw_line.mnemonic == "ebreak"){
                    machine_codes.push_back(0x00100073);
                    break;
                }

                // load / jalr: op rd, imm(rs1)
                if(inst_def->opcode == 0x03 || raw_line.mnemonic == "jalr"){
                    if(raw_line.operands.size() != 5 ||
                       raw_line.operands[2] != "(" || raw_line.operands[4] != ")"){
                        std::cerr << "Error: invalid syntax for '" << raw_line.mnemonic
                                  << "' at line " << raw_line.line
                                  << " (expected: op rd, imm(rs1))\n";
                        continue;
                    }
                    uint32_t rd = get_reg_index(raw_line.operands[0]);
                    uint32_t rs1 = get_reg_index(raw_line.operands[3]);
                    if(rd == 0xFFFFFFFF || rs1 == 0xFFFFFFFF){
                        std::cerr << "Error: Invalid register in instruction '" << raw_line.mnemonic
                                  << "' at line " << raw_line.line << "\n";
                        continue;
                    }
                    int32_t imm = parse_imm(raw_line.operands[1]);
                    machine_codes.push_back(
                        (static_cast<uint32_t>(imm & 0xFFF) << 20) | (rs1 << 15) |
                        (inst_def->funct3 << 12) | (rd << 7) | inst_def->opcode
                    );
                    break;
                }

                // arithmetic I-type: addi rd, rs1, imm
                if(raw_line.operands.size() != 3){
                    std::cerr << "Error: I-type instruction '" << raw_line.mnemonic
                              << "' expects 3 operands at line " << raw_line.line << "\n";
                    continue;
                }
                uint32_t rd = get_reg_index(raw_line.operands[0]);
                uint32_t rs1 = get_reg_index(raw_line.operands[1]);
                if(rd == 0xFFFFFFFF || rs1 == 0xFFFFFFFF){
                    std::cerr << "Error: Invalid register in instruction '" << raw_line.mnemonic
                              << "' at line " << raw_line.line << "\n";
                    continue;
                }
                int32_t imm = parse_imm(raw_line.operands[2]);
                machine_codes.push_back(
                    (static_cast<uint32_t>(imm & 0xFFF) << 20) | (rs1 << 15) |
                    (inst_def->funct3 << 12) | (rd << 7) | inst_def->opcode
                );
                break;
            }
            case Fmt::S: {
                if(raw_line.operands.size() != 5 ||
                   raw_line.operands[2] != "(" || raw_line.operands[4] != ")"){
                    std::cerr << "Error: invalid syntax for '" << raw_line.mnemonic
                              << "' at line " << raw_line.line
                              << " (expected: op rs2, imm(rs1))\n";
                    continue;
                }
                uint32_t rs2 = get_reg_index(raw_line.operands[0]);
                uint32_t rs1 = get_reg_index(raw_line.operands[3]);
                if(rs1 == 0xFFFFFFFF || rs2 == 0xFFFFFFFF){
                    std::cerr << "Error: Invalid register in instruction '" << raw_line.mnemonic
                              << "' at line " << raw_line.line << "\n";
                    continue;
                }
                int32_t imm = parse_imm(raw_line.operands[1]);
                machine_codes.push_back(
                    (((static_cast<uint32_t>(imm) >> 5) & 0x7F) << 25) | (rs2 << 20) |
                    (rs1 << 15) | (inst_def->funct3 << 12) |
                    ((static_cast<uint32_t>(imm) & 0x1F) << 7) | inst_def->opcode
                );
                break;
            }
            case Fmt::B: {
                if(raw_line.operands.size() != 3){
                    std::cerr << "Error: B-type instruction '" << raw_line.mnemonic
                              << "' expects 3 operands at line " << raw_line.line << "\n";
                    continue;
                }
                uint32_t rs1 = get_reg_index(raw_line.operands[0]);
                uint32_t rs2 = get_reg_index(raw_line.operands[1]);
                if(rs1 == 0xFFFFFFFF || rs2 == 0xFFFFFFFF){
                    std::cerr << "Error: Invalid register in instruction '" << raw_line.mnemonic
                              << "' at line " << raw_line.line << "\n";
                    continue;
                }
                uint32_t imm = 0;
                if(label_addresses.find(raw_line.operands[2]) != label_addresses.end()){
                    imm = label_addresses.at(raw_line.operands[2]) - current_addr;
                } else {
                    imm = parse_imm(raw_line.operands[2]);
                }
                machine_codes.push_back(
                    (((imm >> 12) & 0x1) << 31) |
                    (((imm >> 5) & 0x3F) << 25) |
                    (rs2 << 20) | (rs1 << 15) |
                    (inst_def->funct3 << 12) |
                    (((imm >> 1) & 0xF) << 8) |
                    (((imm >> 11) & 0x1) << 7) |
                    inst_def->opcode
                );
                break;
            }
            case Fmt::U: {
                if(raw_line.operands.size() != 2){
                    std::cerr << "Error: U-type instruction '" << raw_line.mnemonic
                              << "' expects 2 operands at line " << raw_line.line << "\n";
                    continue;
                }
                uint32_t rd = get_reg_index(raw_line.operands[0]);
                if(rd == 0xFFFFFFFF){
                    std::cerr << "Error: Invalid register in instruction '" << raw_line.mnemonic
                              << "' at line " << raw_line.line << "\n";
                    continue;
                }
                int32_t imm = parse_imm(raw_line.operands[1]);
                machine_codes.push_back(
                    (static_cast<uint32_t>(imm) << 12) | (rd << 7) | inst_def->opcode
                );
                break;
            }
            case Fmt::J: {
                if(raw_line.operands.size() != 2){
                    std::cerr << "Error: J-type instruction '" << raw_line.mnemonic
                              << "' expects 2 operands at line " << raw_line.line << "\n";
                    continue;
                }
                uint32_t rd = get_reg_index(raw_line.operands[0]);
                if(rd == 0xFFFFFFFF){
                    std::cerr << "Error: Invalid register in instruction '" << raw_line.mnemonic
                              << "' at line " << raw_line.line << "\n";
                    continue;
                }
                uint32_t imm = 0;
                if(label_addresses.find(raw_line.operands[1]) != label_addresses.end()){
                    imm = label_addresses.at(raw_line.operands[1]) - current_addr;
                } else {
                    imm = parse_imm(raw_line.operands[1]);
                }
                machine_codes.push_back(
                    (((imm >> 20) & 0x1) << 31) |
                    (((imm >> 1) & 0x3FF) << 21) |
                    (((imm >> 11) & 0x1) << 20) |
                    (((imm >> 12) & 0xFF) << 12) |
                    (rd << 7) | inst_def->opcode
                );
                break;
            }
            default: {
                std::cerr << "Error: Unsupported instruction format for '"
                          << raw_line.mnemonic << "' at line " << raw_line.line << "\n";
                break;
            }
        }
    }
}

void Assembler::Pass1() {
    uint32_t current_address = 0;
    for(const RawLine& raw_line : raw_lines){
        if(!raw_line.labels.empty()){
            for(const std::string& label : raw_line.labels){
                if(label_addresses.find(label) != label_addresses.end()){
                    std::cerr << "Error: Duplicate label '" << label
                              << "' at line " << raw_line.line << "\n";
                } else {
                    label_addresses[label] = current_address;
                }
            }
        }
        if(!raw_line.mnemonic.empty()){
            current_address += get_instruction_size(raw_line);
        }
    }
}

void Assembler::Parser(){
    for(size_t line_no = 0; line_no < tokens.size(); ++line_no){
        const std::vector<Token>& line_tokens = tokens[line_no];
        RawLine raw_line;
        raw_line.line = line_no + 1;

        size_t i = 0;
        while(i + 1 < line_tokens.size()
              && line_tokens[i].type == Tok::IDENT
              && line_tokens[i + 1].type == Tok::COLON){
            raw_line.labels.push_back(line_tokens[i].text);
            i += 2;
        }

        if(i < line_tokens.size() && line_tokens[i].type == Tok::IDENT){
            raw_line.mnemonic = line_tokens[i].text;
            ++i;

            for(; i < line_tokens.size(); ++i){
                const Token& op_tok = line_tokens[i];
                if(op_tok.type == Tok::COMMA){
                    continue;
                } else if(op_tok.type == Tok::IDENT
                       || op_tok.type == Tok::NUMBER
                       || op_tok.type == Tok::LPAREN
                       || op_tok.type == Tok::RPAREN){
                    raw_line.operands.push_back(op_tok.text);
                } else {
                    std::cerr << "Error at line " << raw_line.line
                              << ": unexpected token '" << op_tok.text << "'\n";
                }
            }
        } else if(i < line_tokens.size()){
            std::cerr << "Error at line " << raw_line.line
                      << ": expected mnemonic, got '" << line_tokens[i].text << "'\n";
        }
        raw_lines.push_back(raw_line);
    }
    tokens.clear();
}

void Assembler::Lexer() {
    for(size_t line_no = 0; line_no < lines.size(); ++line_no){
        const std::string& line = lines[line_no];
        std::vector<Token> line_tokens;
        for(size_t i = 0; i < line.size();){
            unsigned char c = static_cast<unsigned char>(line[i]);
            if(std::isspace(c)){
                ++i; continue;
            }
            if(c == '#') break;
            if(std::isalpha(c) || c == '_' || c=='.'){
                size_t start = i;
                while(i < line.size()){
                    unsigned char ch = static_cast<unsigned char>(line[i]);
                    if(std::isalnum(ch) || ch == '_' || ch == '.') ++i;
                    else break;
                }
                line_tokens.emplace_back(Tok::IDENT, line.substr(start, i - start), line_no + 1);
            }

            else if(std::isdigit(c) || c == '-'){
                size_t start = i;
                if(c == '-'){
                    if(i + 1 >= line.size() || !std::isdigit(static_cast<unsigned char>(line[i+1]))){
                        std::cerr << "Error at line " << line_no + 1
                                  << ": stray '-'\n";
                        ++i;
                        continue;
                    }
                    ++i;
                }


                if(i + 1 < line.size() && line[i] == '0' && line[i+1] == 'x'){
                    i += 2;
                    while(i < line.size() && std::isxdigit(static_cast<unsigned char>(line[i]))) ++i;
                } else {
                    while(i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) ++i;
                }

                line_tokens.emplace_back(Tok::NUMBER, line.substr(start, i - start), line_no + 1);
            }
            else {
                switch(c){
                    case ',': line_tokens.emplace_back(Tok::COMMA, ",", line_no + 1); break;
                    case '(': line_tokens.emplace_back(Tok::LPAREN, "(", line_no + 1); break;
                    case ')': line_tokens.emplace_back(Tok::RPAREN, ")", line_no + 1); break;
                    case ':': line_tokens.emplace_back(Tok::COLON, ":", line_no + 1); break;
                    default:
                        std::cerr << "Error at line " << line_no + 1
                                  << ": unknown character '" << c << "'\n";
                        break;
                }
                ++i;
            }
        }
        tokens.push_back(std::move(line_tokens));
    }
    lines.clear();
}

void Assembler::read_file_lines(){
        std::ifstream file(path);
        if(!file.is_open()){
            std::cerr << "Failed to open file: " << path << std::endl;
            return;
        }
        std::string line;
        while(std::getline(file, line)){
            lines.push_back(line);
        }
        file.close();
}

// ===== 汇编器入口 =====
int main(int argc, char** argv) {
    if(argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.s> [-o output.bin]\n";
        return 1;
    }
    std::string input_path = argv[1];
    std::string output_path = "a.bin";
    for(int i = 2; i < argc; ++i) {
        if(std::string(argv[i]) == "-o" && i + 1 < argc) {
            output_path = argv[++i];
        }
    }
    Assembler assembler(input_path, output_path);
    assembler.assemble();
    return 0;
}
