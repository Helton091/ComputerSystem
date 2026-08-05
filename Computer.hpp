#ifndef COMPUTER_HPP
#define COMPUTER_HPP
#include<cstdint>
#include<cstddef>
#include<vector>

inline constexpr uint32_t MEM_SIZE = 0x400000; // 4MB

class Computer{
private:
    std::vector<uint8_t> MEM;
    uint32_t RegFile[32] = {0};
    uint32_t PC = 0;
    bool halted = false;

    // 写寄存器，统一拦截 x0
    void WriteReg(uint8_t idx, uint32_t val){
        if(idx != 0) RegFile[idx] = val;
    }
public:
    Computer() : MEM(MEM_SIZE, 0) {
        RegFile[2] = MEM_SIZE - 4; // sp 初始化到栈顶
    }

    bool is_halted() const { return halted; }
    void reset_halt() { halted = false; }
    uint32_t get_pc() const { return PC; }
    uint32_t get_reg(uint8_t idx) const { return RegFile[idx]; }

    uint8_t ReadByte(uint32_t addr){return MEM[addr];}
    uint16_t ReadHalf(uint32_t addr){
        return static_cast<uint16_t>(MEM[addr]) | static_cast<uint16_t>(MEM[addr+1]) << 8;
    }
    uint32_t ReadWord(uint32_t addr){
        return static_cast<uint32_t>(MEM[addr]) | static_cast<uint32_t>(MEM[addr+1]) << 8 | static_cast<uint32_t>(MEM[addr+2]) << 16 | static_cast<uint32_t>(MEM[addr+3]) << 24;
    }
    void WriteByte(uint32_t addr, uint8_t data){MEM[addr] = data;}
    void WriteHalf(uint32_t addr, uint16_t data){
        MEM[addr] = static_cast<uint8_t>(data & 0xFF);
        MEM[addr+1] = static_cast<uint8_t>((data >> 8) & 0xFF);
    }
    void WriteWord(uint32_t addr, uint32_t data){
        MEM[addr] = static_cast<uint8_t>(data & 0xFF);
        MEM[addr+1] = static_cast<uint8_t>((data >> 8) & 0xFF);
        MEM[addr+2] = static_cast<uint8_t>((data >> 16) & 0xFF);
        MEM[addr+3] = static_cast<uint8_t>((data >> 24) & 0xFF);
    }
    void LoadProgram(const uint8_t* code, size_t size, uint32_t base_addr){
        for(size_t i = 0; i < size; ++i){
            MEM[base_addr + i] = code[i];
        }
        PC = base_addr;
    }

    void execute_step();
    void execute(){
        while(!halted){
            execute_step();
        }
    }
};

#endif // COMPUTER_HPP
