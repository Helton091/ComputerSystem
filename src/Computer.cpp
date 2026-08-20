#include "Computer.hpp"
#include "Computer_info.hpp"
#include<cstdio>
#include<climits>
#include<vector>
#include<fstream>
#include<iostream>
#include<cmath>
#include<cstring>

static float bits_to_float(uint32_t bits){
    float f;
    static_assert(sizeof(f) == sizeof(bits));
    std::memcpy(&f,&bits,sizeof(f));
    return f;
}

static uint32_t float_to_bits(float f){
    uint32_t bits;
    static_assert(sizeof(f) == sizeof(bits));
    std::memcpy(&bits,&f,sizeof(f));
    return bits;
}

// ---- 浮点辅助函数 ----

static uint32_t canonical_nan(){ return 0x7FC00000u; }

static bool fp_is_nan(uint32_t bits){
    uint32_t exp = (bits >> 23) & 0xFF;
    uint32_t mant = bits & 0x7FFFFF;
    return exp == 0xFF && mant != 0;
}

static uint32_t fclass_result(uint32_t bits){
    uint32_t sign = bits >> 31;
    uint32_t exp = (bits >> 23) & 0xFF;
    uint32_t mant = bits & 0x7FFFFF;
    if(exp == 0xFF && mant != 0){
        return ((mant >> 22) & 1) ? (1u << 9) : (1u << 8); // qNaN / sNaN
    }
    if(exp == 0xFF && mant == 0){
        return sign ? (1u << 0) : (1u << 7); // -inf / +inf
    }
    if(exp == 0 && mant == 0){
        return sign ? (1u << 3) : (1u << 4); // -0 / +0
    }
    if(exp == 0 && mant != 0){
        return sign ? (1u << 2) : (1u << 5); // negative / positive subnormal
    }
    return sign ? (1u << 1) : (1u << 6); // negative / positive normal
}

static uint32_t fmin(uint32_t a_bits, uint32_t b_bits){
    float a = bits_to_float(a_bits);
    float b = bits_to_float(b_bits);
    if(fp_is_nan(a_bits) && fp_is_nan(b_bits)) return canonical_nan();
    if(fp_is_nan(a_bits)) return b_bits;
    if(fp_is_nan(b_bits)) return a_bits;
    if(a == 0.0f && b == 0.0f){
        return (a_bits & 0x80000000u) ? a_bits : b_bits;
    }
    return (a < b) ? a_bits : b_bits;
}

static uint32_t fmax(uint32_t a_bits, uint32_t b_bits){
    float a = bits_to_float(a_bits);
    float b = bits_to_float(b_bits);
    if(fp_is_nan(a_bits) && fp_is_nan(b_bits)) return canonical_nan();
    if(fp_is_nan(a_bits)) return b_bits;
    if(fp_is_nan(b_bits)) return a_bits;
    if(a == 0.0f && b == 0.0f){
        return (a_bits & 0x80000000u) ? b_bits : a_bits;
    }
    return (a > b) ? a_bits : b_bits;
}

static int32_t float_to_int_rne(float f){
    if(std::isnan(f) || std::isinf(f)) return INT32_MAX;
    return static_cast<int32_t>(std::lrintf(f));
}

static uint32_t float_to_uint_rne(float f){
    if(std::isnan(f) || f < 0.0f) return 0;
    if(std::isinf(f) || f > static_cast<float>(UINT32_MAX)) return UINT32_MAX;
    return static_cast<uint32_t>(std::llrintf(f));
}

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
        case 0b0000111: { // LOAD-FP (flw)
            if(funct3 == 0b010){
                uint32_t addr = RegFile[rs1] + static_cast<uint32_t>(immI);
                FRegFile[rd] = ReadWord(addr);
            }
            break;
        }
        case 0b0100111: { // STORE-FP (fsw)
            if(funct3 == 0b010){
                uint32_t addr = RegFile[rs1] + static_cast<uint32_t>(immS);
                WriteWord(addr, FRegFile[rs2]);
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
                        case 2: // print_float (fa0)
                            std::printf("%f", static_cast<double>(bits_to_float(FRegFile[10])));
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
        case 0b1010011: { // OP-FP
            uint8_t funct7_fp = (instruction >> 25) & 0x7F;
            uint8_t funct5_fp = funct7_fp >> 2;
            uint8_t fmt = funct7_fp & 0x3;
            if(fmt != 0) break; // 只支持 single precision

            switch(funct5_fp){
                case 0b00000: { // fadd.s
                    float a = bits_to_float(FRegFile[rs1]);
                    float b = bits_to_float(FRegFile[rs2]);
                    FRegFile[rd] = float_to_bits(a + b);
                    break;
                }
                case 0b00001: { // fsub.s
                    float a = bits_to_float(FRegFile[rs1]);
                    float b = bits_to_float(FRegFile[rs2]);
                    FRegFile[rd] = float_to_bits(a - b);
                    break;
                }
                case 0b00010: { // fmul.s
                    float a = bits_to_float(FRegFile[rs1]);
                    float b = bits_to_float(FRegFile[rs2]);
                    FRegFile[rd] = float_to_bits(a * b);
                    break;
                }
                case 0b00011: { // fdiv.s
                    float a = bits_to_float(FRegFile[rs1]);
                    float b = bits_to_float(FRegFile[rs2]);
                    FRegFile[rd] = float_to_bits(a / b);
                    break;
                }
                case 0b01011: { // fsqrt.s
                    float a = bits_to_float(FRegFile[rs1]);
                    FRegFile[rd] = float_to_bits(std::sqrt(a));
                    break;
                }
                case 0b00100: { // fsgnj / fsgnjn / fsgnjx
                    uint32_t a_bits = FRegFile[rs1];
                    uint32_t b_bits = FRegFile[rs2];
                    uint32_t sign = 0;
                    if(funct3 == 0b000){
                        sign = b_bits & 0x80000000u;
                    } else if(funct3 == 0b001){
                        sign = (~b_bits) & 0x80000000u;
                    } else if(funct3 == 0b010){
                        sign = (a_bits ^ b_bits) & 0x80000000u;
                    }
                    FRegFile[rd] = (a_bits & 0x7FFFFFFFu) | sign;
                    break;
                }
                case 0b00101: { // fmin / fmax
                    if(funct3 == 0b000){
                        FRegFile[rd] = fmin(FRegFile[rs1], FRegFile[rs2]);
                    } else if(funct3 == 0b001){
                        FRegFile[rd] = fmax(FRegFile[rs1], FRegFile[rs2]);
                    }
                    break;
                }
                case 0b10100: { // feq / flt / fle
                    float a = bits_to_float(FRegFile[rs1]);
                    float b = bits_to_float(FRegFile[rs2]);
                    bool cmp = false;
                    if(!std::isnan(a) && !std::isnan(b)){
                        if(funct3 == 0b000) cmp = (a <= b);
                        else if(funct3 == 0b001) cmp = (a < b);
                        else if(funct3 == 0b010) cmp = (a == b);
                    }
                    WriteReg(rd, cmp ? 1u : 0u);
                    break;
                }
                case 0b11000: { // fcvt.w.s / fcvt.wu.s
                    float a = bits_to_float(FRegFile[rs1]);
                    uint32_t ires = 0;
                    if(funct3 == 0b000){
                        if(rs2 == 0b00000){
                            ires = static_cast<uint32_t>(float_to_int_rne(a));
                        } else if(rs2 == 0b00001){
                            ires = float_to_uint_rne(a);
                        }
                    }
                    WriteReg(rd, ires);
                    break;
                }
                case 0b11010: { // fcvt.s.w / fcvt.s.wu
                    float fres = 0.0f;
                    if(rs2 == 0b00000){
                        fres = static_cast<float>(static_cast<int32_t>(RegFile[rs1]));
                    } else if(rs2 == 0b00001){
                        fres = static_cast<float>(RegFile[rs1]);
                    }
                    FRegFile[rd] = float_to_bits(fres);
                    break;
                }
                case 0b11100: { // fmv.x.w / fclass.s
                    uint32_t a_bits = FRegFile[rs1];
                    if(funct3 == 0b000){ // fmv.x.w
                        WriteReg(rd, a_bits);
                    } else if(funct3 == 0b001){ // fclass.s
                        WriteReg(rd, fclass_result(a_bits));
                    }
                    break;
                }
                case 0b11110: { // fmv.w.x
                    FRegFile[rd] = RegFile[rs1];
                    break;
                }
                default:
                    break;
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

static uint32_t read_u32(std::ifstream& file){
    uint8_t bytes[4];
    file.read(reinterpret_cast<char*>(bytes), 4);
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

void Computer::LoadProgram(const std::string& path){
    std::ifstream file(path, std::ios::binary);
    if(!file){
        throw std::runtime_error("[Simulator] failed to open binary: '" + path + "'");
    }

    // 1. 读 header
    uint32_t magic = read_u32(file);
    if(magic != 0x434D4D00){
        throw std::runtime_error("[Simulator] invalid CMM binary: bad magic " + std::to_string(magic) + " (expected 0x434D4D00) in '" + path + "'");
    }
    uint32_t text_size = read_u32(file);
    uint32_t data_base = read_u32(file);
    std::vector<uint8_t> text(text_size);
    file.read(reinterpret_cast<char*>(text.data()), text_size);
    std::vector<uint8_t> static_data{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };
    LoadSegment(text.data(), text.size(), TEXT_BASE);  // 0x1000
    LoadSegment(static_data.data(), static_data.size(), data_base);  // 0x200000
    PC = TEXT_BASE;
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
    cpu.LoadProgram(std::string(bin_path));

    if(trace){
        cpu.set_trace([](const Computer& c){
            uint32_t pc = c.get_pc();
            uint32_t inst = c.ReadWord(pc);
            std::fprintf(stderr, "PC=0x%08X inst=0x%08X | a0=%d fa0=%f a7=%d sp=0x%08X\n",
                         pc, inst,
                         static_cast<int32_t>(c.get_reg(10)),
                         static_cast<double>(bits_to_float(c.get_freg(10))),
                         static_cast<int32_t>(c.get_reg(17)),
                         c.get_reg(2));
        });
    }

    cpu.execute();
    std::fflush(stdout);
    return 0;
}
