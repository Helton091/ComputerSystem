enum class Fmt { R, I, S, B, U, J };

struct InstDef {
    std::string name;
    uint8_t opcode;
    uint8_t funct3;
    uint8_t funct7;
    Fmt fmt;
    uint8_t rs2 = 0;   // 对两操作数 R-type 指令（如 fsqrt.s/fcvt/fmv）提供固定 rs2
};

static const InstDef INST_TABLE[] = {
    // R-type
    {"add",  0x33, 0x0, 0x00, Fmt::R},
    {"sub",  0x33, 0x0, 0x20, Fmt::R},
    {"sll",  0x33, 0x1, 0x00, Fmt::R},
    {"slt",  0x33, 0x2, 0x00, Fmt::R},
    {"sltu", 0x33, 0x3, 0x00, Fmt::R},
    {"xor",  0x33, 0x4, 0x00, Fmt::R},
    {"srl",  0x33, 0x5, 0x00, Fmt::R},
    {"sra",  0x33, 0x5, 0x20, Fmt::R},
    {"or",   0x33, 0x6, 0x00, Fmt::R},
    {"and",  0x33, 0x7, 0x00, Fmt::R},
    
    // M expansion
    {"mul",  0x33, 0x0, 0x01, Fmt::R},
    {"div",  0x33, 0x4, 0x01, Fmt::R},
    {"rem",  0x33, 0x6, 0x01, Fmt::R},
    
    // I-type arithmetic
    {"addi", 0x13, 0x0, 0x00, Fmt::I},
    {"xori", 0x13, 0x4, 0x00, Fmt::I},
    {"ori",  0x13, 0x6, 0x00, Fmt::I},
    {"andi", 0x13, 0x7, 0x00, Fmt::I},
    {"slli", 0x13, 0x1, 0x00, Fmt::I},
    {"srli", 0x13, 0x5, 0x00, Fmt::I},
    {"srai", 0x13, 0x5, 0x20, Fmt::I},
    {"slti", 0x13, 0x2, 0x00, Fmt::I},
    {"sltiu",0x13, 0x3, 0x00, Fmt::I},
    
    // Load
    {"lb",   0x03, 0x0, 0x00, Fmt::I},
    {"lh",   0x03, 0x1, 0x00, Fmt::I},
    {"lw",   0x03, 0x2, 0x00, Fmt::I},
    {"lbu",  0x03, 0x4, 0x00, Fmt::I},
    {"lhu",  0x03, 0x5, 0x00, Fmt::I},
    
    // Store
    {"sb",   0x23, 0x0, 0x00, Fmt::S},
    {"sh",   0x23, 0x1, 0x00, Fmt::S},
    {"sw",   0x23, 0x2, 0x00, Fmt::S},
    
    // Branch
    {"beq",  0x63, 0x0, 0x00, Fmt::B},
    {"bne",  0x63, 0x1, 0x00, Fmt::B},
    {"blt",  0x63, 0x4, 0x00, Fmt::B},
    {"bge",  0x63, 0x5, 0x00, Fmt::B},
    {"bltu", 0x63, 0x6, 0x00, Fmt::B},
    {"bgeu", 0x63, 0x7, 0x00, Fmt::B},
    
    // Jump
    {"jal",  0x6F, 0x0, 0x00, Fmt::J},
    {"jalr", 0x67, 0x0, 0x00, Fmt::I},
    
    // U-type
    {"lui",   0x37, 0x0, 0x00, Fmt::U},
    {"auipc", 0x17, 0x0, 0x00, Fmt::U},
    
    // System
    {"ecall",  0x73, 0x0, 0x00, Fmt::I},
    {"ebreak", 0x73, 0x0, 0x00, Fmt::I},

    // RV32F floating-point load/store
    {"flw", 0x07, 0x2, 0x00, Fmt::I},
    {"fsw", 0x27, 0x2, 0x00, Fmt::S},

    // RV32F computational (R-type, 3 operands)
    {"fadd.s",  0x53, 0x0, 0x00, Fmt::R},
    {"fsub.s",  0x53, 0x0, 0x04, Fmt::R},
    {"fmul.s",  0x53, 0x0, 0x08, Fmt::R},
    {"fdiv.s",  0x53, 0x0, 0x0C, Fmt::R},

    // RV32F sign-injection
    {"fsgnj.s",  0x53, 0x0, 0x10, Fmt::R},
    {"fsgnjn.s", 0x53, 0x1, 0x10, Fmt::R},
    {"fsgnjx.s", 0x53, 0x2, 0x10, Fmt::R},

    // RV32F min/max
    {"fmin.s", 0x53, 0x0, 0x14, Fmt::R},
    {"fmax.s", 0x53, 0x1, 0x14, Fmt::R},

    // RV32F compare (result in integer register)
    {"fle.s", 0x53, 0x0, 0x50, Fmt::R},
    {"flt.s", 0x53, 0x1, 0x50, Fmt::R},
    {"feq.s", 0x53, 0x2, 0x50, Fmt::R},

    // RV32F conversion (R-type, 2 operands, fixed rs2)
    {"fcvt.w.s",  0x53, 0x0, 0x60, Fmt::R, 0x00},
    {"fcvt.wu.s", 0x53, 0x0, 0x60, Fmt::R, 0x01},
    {"fcvt.s.w",  0x53, 0x0, 0x68, Fmt::R, 0x00},
    {"fcvt.s.wu", 0x53, 0x0, 0x68, Fmt::R, 0x01},

    // RV32F move/class (R-type, 2 operands, fixed rs2)
    {"fmv.x.w", 0x53, 0x0, 0x70, Fmt::R, 0x00},
    {"fclass.s",0x53, 0x1, 0x70, Fmt::R, 0x00},
    {"fmv.w.x", 0x53, 0x0, 0x78, Fmt::R, 0x00},

    // RV32F sqrt (R-type, 2 operands, fixed rs2=0)
    {"fsqrt.s", 0x53, 0x0, 0x2C, Fmt::R, 0x00},

    
};

inline const InstDef* find_inst(const std::string& name) {
    for(const auto& inst : INST_TABLE) {
        if(name == inst.name) return &inst;
    }
    return nullptr;
}

struct RegDef {
    std::string name;
    std::string alias;
    uint32_t index;
};

static const RegDef REG_TABLE[] = {
    // Integer registers
    {"x0","zero", 0}, {"x1","ra", 1}, {"x2","sp", 2}, {"x3","gp", 3},
    {"x4","tp", 4}, {"x5","t0", 5}, {"x6","t1", 6}, {"x7","t2", 7},
    {"x8","s0", 8}, {"x9","s1", 9}, {"x10","a0",10},{"x11","a1",11},
    {"x12","a2",12},{"x13","a3",13},{"x14","a4",14},{"x15","a5",15},
    {"x16","a6",16},{"x17","a7",17},{"x18","s2",18},{"x19","s3",19},
    {"x20","s4",20},{"x21","s5",21},{"x22","s6",22},{"x23","s7",23},
    {"x24","s8",24},{"x25","s9",25},{"x26","s10",26},{"x27","s11",27},
    {"x28","t3",28},{"x29","t4",29},{"x30","t5",30},{"x31","t6",31},

    // Floating-point registers
    {"f0","ft0", 0},  {"f1","ft1", 1},  {"f2","ft2", 2},  {"f3","ft3", 3},
    {"f4","ft4", 4},  {"f5","ft5", 5},  {"f6","ft6", 6},  {"f7","ft7", 7},
    {"f8","fs0", 8},  {"f9","fs1", 9},
    {"f10","fa0",10}, {"f11","fa1",11}, {"f12","fa2",12}, {"f13","fa3",13},
    {"f14","fa4",14}, {"f15","fa5",15}, {"f16","fa6",16}, {"f17","fa7",17},
    {"f18","fs2",18}, {"f19","fs3",19}, {"f20","fs4",20}, {"f21","fs5",21},
    {"f22","fs6",22}, {"f23","fs7",23}, {"f24","fs8",24}, {"f25","fs9",25},
    {"f26","fs10",26},{"f27","fs11",27},
    {"f28","ft8",28}, {"f29","ft9",29}, {"f30","ft10",30},{"f31","ft11",31}
};

inline uint32_t get_reg_index(const std::string& name) {
    for(const auto& reg : REG_TABLE) {
        if(name == reg.name || name == reg.alias) return reg.index;
    }
    return 0xFFFFFFFF; // Invalid index
}

