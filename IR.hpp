#pragma once
#include<string>
#include<vector>
#include<memory>
#include<unordered_map>
#include<ostream>
#include<utility>
#include<iostream>
#include<stdexcept>
namespace IR{
class Type{
public:
    virtual ~Type() = default;
    virtual int size() const = 0;
    virtual std::string to_string() const = 0;
};

class IntType : public Type{
public:
    static IntType* get(){static IntType i; return &i;}
    int size() const override{return 4;}
    std::string to_string() const override{return "i32";}
private:
    IntType() = default;
};

class VoidType : public Type{
public:
    static VoidType* get(){static VoidType v; return &v;}
    int size() const override{return 0;}
    std::string to_string() const override{return "void";}
private:
    VoidType() = default;
};

enum class Opcode{
    ALLOCA, LOAD, STORE,
    ADD, SUB, MUL, DIV, REM, NEG,
    LT, GT, LE, GE, EQ, NE,
    BR, JMP, RET,
    CALL,
    PHI
};

struct User;
struct BasicBlock;
struct Function;

struct Value{
    Type* type;
    std::string name;
    std::vector<User*> users;
    Value(Type* t, std::string n = "") : type(t), name(std::move(n)) {}
    virtual ~Value() = default;

    void add_use(User* u) { users.push_back(u); }
    void remove_use(User* u); //remove ONCE
    void replace_all_uses_with(Value* v);
};

struct User : Value{
    std::vector<Value*> operands;
    User(Type* t, std::string n = "") : Value(t, std::move(n)) {}
    ~User() override { drop_operands(); }

    void add_operand(Value* v);
    void drop_operands();  //remove all
};

struct ConstantInt : Value {
    int i_val;
    explicit ConstantInt(int v) : Value(IntType::get()), i_val(v) {}
};

struct Argument : Value{
    int arg_no;
    explicit Argument(int no)
        : Value(IntType::get(), "arg" + std::to_string(no)), arg_no(no) {}
};

struct Instruction : User{
    Opcode op;
    BasicBlock* parent = nullptr;
    Instruction(Opcode o, Type* t, std::string n = "")
        : User(t, std::move(n)), op(o) {}
    void erase_from_parent();
};

struct BasicBlock : Value{
    std::vector<std::unique_ptr<Instruction>> insts;
    Function* parent = nullptr;
    explicit BasicBlock(std::string n)
        : Value(VoidType::get(), std::move(n)) {}
    Instruction* add_inst(std::unique_ptr<Instruction> inst);
    bool is_terminated() const;
};

struct Function : Value{
    std::vector<std::unique_ptr<Argument>> args;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
    BasicBlock* entry = nullptr;
    explicit Function(std::string n)
        : Value(VoidType::get(), std::move(n)) {}
    ~Function(){
        for (auto& block : blocks)
            for (auto& inst : block->insts)
                inst->drop_operands();
    }
    Argument* add_arg();                       
    BasicBlock* add_block(const std::string& name);
    Instruction* add_alloca(const std::string& var_name);
};

struct Module {
    
    Function* add_function(const std::string& name);
    Function* find_function(const std::string& name) const;
    ConstantInt* get_const(int v);

    void dump(std::ostream& out) const;

private:
    std::vector<std::unique_ptr<ConstantInt>> const_pool_; 
    std::unordered_map<int, ConstantInt*> const_map_;
    std::vector<std::unique_ptr<Function>> functions;        
};

Instruction* make_inst(BasicBlock* bb, Opcode op, Type* type,
                       const std::string& name,
                       const std::vector<Value*>& operands = {});


} //namespace IR