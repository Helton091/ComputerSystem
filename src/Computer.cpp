#include "Computer.hpp"
#include<cstdio>
#include<climits>
#include<vector>
#include<fstream>
#include<iostream>

void Computer::execute_step() {
    // 停机状态：直接返回，不再取指执行
    if(halted) return;
    // PC 越界兜底：超出内存范围视为程序异常结束
    if(PC + 4 > MEM_SIZE){ halted = true; return; }

    // Fetch instruction
    uint32_t instruction = ReadWord(PC);
    if(on_step) on_step(*this);  // 调试钩子：观察当前状态
    uint32_t next_PC = PC + 4;
    uint8_t opcode = instruction & 0b1111111;
    uint8_t funct3 = (instruction >> 12) & 0b111;
    uint8_t rs2 = (instruction >> 20) & 0b11111;
    uint8_t rs1 = (instruction >> 15) & 0b11111;
    uint8_t rd = (instruction >> 7) & 0b11111;
    uint8_t funct7 = (instruction >> 25) & 0x7F;

    // ---- 立即数提取（全部做符号扩展）----
    int32_t immI = static_cast<int32_t>(instruction) >> 20;
    int32_t immS = (static_cast<int32_t>(instruction) >> 25 << 5)
                   | ((instruction >> 7) & 0x1F);
    int32_t immB = (((instruction >> 31) & 0x1)   << 12)
                   | (((instruction >> 7)  & 0x1)  << 11)
                   | (((instruction >> 25) & 0x3F) << 5)
                   | (((instruction >> 8)  & 0xF)  << 1);
    immB = (immB << 19) >> 19; // 符号扩展 13→32
    int32_t immJ = (((instruction >> 31) & 0x1)   << 20)
                   | (((instruction >> 12) & 0xFF)  << 12)
                   | (((instruction >> 20) & 0x1)   << 11)
                   | (((instruction >> 21) & 0x3FF) << 1);
    immJ = (immJ << 11) >> 11; // 符号扩展 21→32
    uint32_t immU = instruction & 0xFFFFF000;

    switch(opcode) {
        case 0b0110011: { // R-type
            uint32_t a = RegFile[rs1];
            uint32_t b = RegFile[rs2];
            uint32_t res = 0;
            bool ok = true;

            if(funct7 == 0x01) {
                // ===== M 扩展：整数乘除法 =====
                switch(funct3){
                    case 0b000: { // MUL
                        int64_t prod = static_cast<int64_t>(static_cast<int32_t>(a))
                                     * static_cast<int64_t>(static_cast<int32_t>(b));
                        res = static_cast<uint32_t>(prod & 0xFFFFFFFFu);
                        break;
                    }
                    case 0b001: { // MULH
                        int64_t prod = static_cast<int64_t>(static_cast<int32_t>(a))
                                     * static_cast<int64_t>(static_cast<int32_t>(b));
                        res = static_cast<uint32_t>((static_cast<uint64_t>(prod) >> 32) & 0xFFFFFFFFu);
                        break;
                    }
                    case 0b010: { // MULHSU
                        int64_t prod = static_cast<int64_t>(static_cast<int32_t>(a))
                                     * static_cast<int64_t>(static_cast<uint64_t>(b));
                        res = static_cast<uint32_t>((static_cast<uint64_t>(prod) >> 32) & 0xFFFFFFFFu);
                        break;
                    }
                    case 0b011: { // MULHU
                        uint64_t prod = static_cast<uint64_t>(a)
                                      * static_cast<uint64_t>(b);
                        res = static_cast<uint32_t>((prod >> 32) & 0xFFFFFFFFu);
                        break;
                    }
                    case 0b100: { // DIV
                        int32_t sa = static_cast<int32_t>(a);
                        int32_t sb = static_cast<int32_t>(b);
                        if(sb == 0){
                            res = 0xFFFFFFFFu;
                        } else if(sa == INT32_MIN && sb == -1){
                            res = static_cast<uint32_t>(INT32_MIN);
                        } else {
                            res = static_cast<uint32_t>(sa / sb);
                        }
                        break;
                    }
                    case 0b101: { // DIVU
                        if(b == 0){
                            res = 0xFFFFFFFFu;
                        } else {
                            res = a / b;
                        }
                        break;
                    }
                    case 0b110: { // REM
                        int32_t sa = static_cast<int32_t>(a);
                        int32_t sb = static_cast<int32_t>(b);
                        if(sb == 0){
                            res = a;
                        } else if(sa == INT32_MIN && sb == -1){
                            res = 0;
                        } else {
                            res = static_cast<uint32_t>(sa % sb);
                        }
                        break;
                    }
                    case 0b111: { // REMU
                        if(b == 0){
                            res = a;
                        } else {
                            res = a % b;
                        }
                        break;
                    }
                    default:
                        ok = false;
                        break;
                }
            } else {
                // ===== 基础 RV32I R-type 运算 =====
                switch(funct3){
                    case 0b000: // ADD or SUB
                        res = (funct7 & 0x20) ? (a - b) : (a + b);
                        break;
                    case 0b001: // SLL
                        res = a << (b & 0x1F);
                        break;
                    case 0b010: // SLT
                        res = (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? 1 : 0;
                        break;
                    case 0b011: // SLTU
                        res = (a < b) ? 1 : 0;
                        break;
                    case 0b100: // XOR
                        res = a ^ b;
                        break;
                    case 0b101: // SRL or SRA
                        if(funct7 & 0x20) {
                            res = static_cast<uint32_t>(static_cast<int32_t>(a) >> (b & 0x1F));
                        } else {
                            res = a >> (b & 0x1F);
                        }
                        break;
                    case 0b110: // OR
                        res = a | b;
                        break;
                    case 0b111: // AND
                        res = a & b;
                        break;
                    default:
                        ok = false;
                        break;
                }
            }
            if(ok) WriteReg(rd, res);
            break;
        }
        case 0b0010011: { // I-type arithmetic
            uint32_t a = RegFile[rs1];
            uint32_t res = 0;
            bool ok = true;
            switch(funct3){
                case 0b000: // ADDI
                    res = a + static_cast<uint32_t>(immI);
                    break;
                case 0b010: // SLTI
                    res = (static_cast<int32_t>(a) < immI) ? 1 : 0;
                    break;
                case 0b011: // SLTIU
                    res = (a < static_cast<uint32_t>(immI)) ? 1 : 0;
                    break;
                case 0b100: // XORI
                    res = a ^ static_cast<uint32_t>(immI);
                    break;
                case 0b110: // ORI
                    res = a | static_cast<uint32_t>(immI);
                    break;
                case 0b111: // ANDI
                    res = a & static_cast<uint32_t>(immI);
                    break;
                case 0b001: // SLLI
                    if((funct7 & 0xFE) == 0x00) {
                        res = a << (immI & 0x1F);
                    } else {
                        ok = false;
                    }
                    break;
                case 0b101: // SRLI / SRAI
                    if((funct7 & 0xFE) == 0x00) {
                        res = a >> (immI & 0x1F);
                    } else if((funct7 & 0xFE) == 0x20) {
                        res = static_cast<uint32_t>(static_cast<int32_t>(a) >> (immI & 0x1F));
                    } else {
                        ok = false;
                    }
                    break;
                default:
                    ok = false;
                    break;
            }
            if(ok) WriteReg(rd, res);
            break;
        }
        case 0b0000011: { // Load
            uint32_t addr = RegFile[rs1] + static_cast<uint32_t>(immI);
            uint32_t res = 0;
            bool ok = true;
            switch(funct3){
                case 0b000: // LB
                    res = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(ReadByte(addr))));
                    break;
                case 0b001: // LH
                    res = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(ReadHalf(addr))));
                    break;
                case 0b010: // LW
                    res = ReadWord(addr);
                    break;
                case 0b100: // LBU
                    res = ReadByte(addr);
                    break;
                case 0b101: // LHU
                    res = ReadHalf(addr);
                    break;
                default:
                    ok = false;
                    break;
            }
            if(ok) WriteReg(rd, res);
            break;
        }
        case 0b0100011: { // Store
            uint32_t addr = RegFile[rs1] + static_cast<uint32_t>(immS);
            switch(funct3){
                case 0b000: // SB
                    WriteByte(addr, static_cast<uint8_t>(RegFile[rs2] & 0xFF));
                    break;
                case 0b001: // SH
                    WriteHalf(addr, static_cast<uint16_t>(RegFile[rs2] & 0xFFFF));
                    break;
                case 0b010: // SW
                    WriteWord(addr, RegFile[rs2]);
                    break;
                default:
                    break;
            }
            break;
        }
        case 0b1100011: { // Branch
            uint32_t a = RegFile[rs1];
            uint32_t b = RegFile[rs2];
            bool take = false;
            switch(funct3){
                case 0b000: take = (a == b); break; // BEQ
                case 0b001: take = (a != b); break; // BNE
                case 0b100: take = (static_cast<int32_t>(a) < static_cast<int32_t>(b)); break; // BLT
                case 0b101: take = (static_cast<int32_t>(a) >= static_cast<int32_t>(b)); break; // BGE
                case 0b110: take = (a < b); break; // BLTU
                case 0b111: take = (a >= b); break; // BGEU
                default: break;
            }
            if(take) next_PC = PC + static_cast<uint32_t>(immB);
            break;
        }
        case 0b1101111: { // JAL
            WriteReg(rd, PC + 4);
            next_PC = PC + static_cast<uint32_t>(immJ);
            break;
        }
        case 0b1100111: { // JALR
            uint32_t target = (RegFile[rs1] + static_cast<uint32_t>(immI)) & ~0x1u;
            WriteReg(rd, PC + 4);
            next_PC = target;
            break;
        }
        case 0b0110111: { // LUI
            WriteReg(rd, immU);
            break;
        }
        case 0b0010111: { // AUIPC
            WriteReg(rd, PC + immU);
            break;
        }
        case 0b1110011: { // SYSTEM（ecall / ebreak）
            uint32_t imm12 = (instruction >> 20) & 0xFFF;
            if(funct3 == 0b000) {
                if(imm12 == 0x000) {
                    // ecall
                    uint32_t syscall_no = RegFile[17]; // a7
                    uint32_t a0 = RegFile[10];
                    switch(syscall_no){
                        case 1: // print_int
                            std::printf("%d", static_cast<int32_t>(a0));
                            break;
                        case 4: { // print_string
                            uint32_t addr = a0;
                            while(addr < MEM_SIZE){
                                uint8_t c = MEM[addr++];
                                if(c == 0) break;
                                std::putchar(c);
                            }
                            break;
                        }
                        case 11: // print_char
                            std::putchar(static_cast<int>(a0 & 0xFF));
                            break;
                        case 10: // exit
                            halted = true;
                            break;
                        default:
                            break;
                    }
                } else if(imm12 == 0x001) {
                    // ebreak
                    halted = true;
                }
            }
            break;
        }
        default:
            break;
    }
    PC = next_PC;
}

// ===== 模拟器入口 =====
static std::vector<uint8_t> load_bin(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
}

int main(int argc, char* argv[]){
    if(argc < 2){
        std::fprintf(stderr, "Usage: %s <program.bin> [--trace]\n", argv[0]);
        return 1;
    }

    const char* bin_path = argv[1];
    bool trace = false;
    for(int i = 2; i < argc; ++i){
        if(std::string(argv[i]) == "--trace") trace = true;
    }

    Computer cpu;
    std::vector<uint8_t> code = load_bin(bin_path);
    if(code.empty()){
        std::cerr << "Failed to load: " << bin_path << "\n";
        return 1;
    }
    cpu.LoadProgram(code.data(), code.size(), 0x1000);

    if(trace){
        cpu.set_trace([](const Computer& c){
            uint32_t pc = c.get_pc();
            uint32_t inst = c.ReadWord(pc);
            std::fprintf(stderr, "PC=0x%08X inst=0x%08X | a0=%d a7=%d sp=0x%08X\n",
                         pc, inst,
                         static_cast<int32_t>(c.get_reg(10)),
                         static_cast<int32_t>(c.get_reg(17)),
                         c.get_reg(2));
        });
    }

    cpu.execute();
    std::fflush(stdout);
    return 0;
}
