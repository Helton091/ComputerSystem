enum class Fmt { R, I, S, B, U, J };

struct InstDef {
    std::string name;
    uint8_t opcode;
    uint8_t funct3;
    uint8_t funct7;
    Fmt fmt;
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
    {"x0","zero", 0}, {"x1","ra", 1}, {"x2","sp", 2}, {"x3","gp", 3},
    {"x4","tp", 4}, {"x5","t0", 5}, {"x6","t1", 6}, {"x7","t2", 7},
    {"x8","s0", 8}, {"x9","s1", 9}, {"x10","a0",10},{"x11","a1",11},
    {"x12","a2",12},{"x13","a3",13},{"x14","a4",14},{"x15","a5",15},
    {"x16","a6",16},{"x17","a7",17},{"x18","s2",18},{"x19","s3",19},
    {"x20","s4",20},{"x21","s5",21},{"x22","s6",22},{"x23","s7",23},
    {"x24","s8",24},{"x25","s9",25},{"x26","s10",26},{"x27","s11",27},
    {"x28","t3",28},{"x29","t4",29},{"x30","t5",30},{"x31","t6",31}
};

inline uint32_t get_reg_index(const std::string& name) {
    for(const auto& reg : REG_TABLE) {
        if(name == reg.name || name == reg.alias) return reg.index;
    }
    return 0xFFFFFFFF; // Invalid index
}

