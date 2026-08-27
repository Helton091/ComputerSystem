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

class FloatType : public Type{
public:
    static FloatType* get(){static FloatType f; return &f;}
    int size() const override{return 4;}
    std::string to_string() const override{return "float";}
private:
    FloatType() = default;
};

class PointerType : public Type{
public:
    Type* element_type;
    static PointerType* get(Type* elem){
        static std::unordered_map<Type*,PointerType*> map;
        auto it = map.find(elem);
        if(it != map.end()) return it->second;
        auto p = new PointerType(elem);
        //pointertype live as long as the program, so we don't need delete. OS will do it
        map[elem] = p;
        return p;
    }
    int size() const override{return 4;}
    std::string to_string() const override{return element_type->to_string() + "*";}
private:
    explicit PointerType(Type* elem) : element_type(elem){}
};

struct ArrayHashKey{
    size_t operator()(const std::pair<Type*,int>& p) const{
        return std::hash<Type*>{}(p.first) ^ (std::hash<int>{}(p.second) << 1);
    }
};

class ArrayType : public Type{
public:
    Type* element_type;
    int length;
    static ArrayType* get(Type* elem, int len){
        static std::unordered_map<std::pair<Type*,int>,ArrayType*,ArrayHashKey> map;
        auto it = map.find({elem,len});
        if(it != map.end()) return it->second;
        auto p = new ArrayType(elem,len);
        map[{elem,len}] = p;
        return p;
    }
    int size() const override{return element_type->size() * length;}
    std::string to_string() const override{return "[" + std::to_string(length) + " * " + element_type->to_string() + "]";}
private:
    explicit ArrayType(Type* elem_type,int len) : element_type(elem_type), length(len){}
};

enum class Opcode{
    ALLOCA, LOAD, STORE,
    ADD, SUB, MUL, DIV, REM, NEG,
    FADD, FSUB, FMUL, FDIV, FNEG,
    LT, GT, LE, GE, EQ, NE,
    FLT,FGT,FLE,FGE,FEQ,FNE,
    BR, JMP, RET,
    CALL, GETPTR, 
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

struct GlobalVariable : Value{
    Value* init_value;
    GlobalVariable(Type* t, const std::string& n, Value* init = nullptr)
        : Value(t, n), init_value(init) {}
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

struct NULLPointer : Value{
    explicit NULLPointer(Type* pointee_type) : Value(PointerType::get(pointee_type)){}
};

struct ConstantFloat : Value{
    float f_val;
    explicit ConstantFloat(float f) : Value(FloatType::get()), f_val(f) {}
};

struct Argument : Value{
    int arg_no;
    explicit Argument(int no, Type* typ)
        : Value(typ, "arg" + std::to_string(no)), arg_no(no) {}
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
    Type* return_type;
    std::vector<std::unique_ptr<Argument>> args;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
    BasicBlock* entry = nullptr;
    explicit Function(std::string n, Type* ret_typ)
        : Value(VoidType::get(), std::move(n)), return_type(ret_typ) {}
    ~Function(){
        for (auto& block : blocks)
            for (auto& inst : block->insts)
                inst->drop_operands();
    }
    Argument* add_arg(Type* typ);                       
    BasicBlock* add_block(const std::string& name);
    Instruction* add_alloca(const std::string& var_name, Type* typ);
};

struct Module {
    ~Module(){
        // Drop all operands before destroying Functions so that CALL
        // instructions do not touch already-destroyed callee Functions.
        for(const auto& func : functions){
            for(const auto& block : func->blocks){
                for(const auto& inst : block->insts){
                    inst->drop_operands();
                }
            }
        }
    }
    
    Function* add_function(const std::string& name, Type* typ);
    Function* find_function(const std::string& name) const;
    ConstantInt* get_const(int v);
    ConstantFloat* get_const(float f);
    NULLPointer* get_nullptr(Type* pointee_type);
    const std::vector<std::unique_ptr<Function>>& get_functions() const{return functions;}
    const std::vector<std::unique_ptr<GlobalVariable>>& get_globals() const{return globals;}
    GlobalVariable* add_global(const std::string& name, Type* type, Value* init = nullptr);
    GlobalVariable* find_global(const std::string& name) const;
    void dump(std::ostream& out) const;

private:
    std::vector<std::unique_ptr<NULLPointer>> NULLPointer_pool_;
    std::vector<std::unique_ptr<ConstantInt>> int_const_pool_; 
    std::vector<std::unique_ptr<ConstantFloat>> float_const_pool_;
    std::unordered_map<int, ConstantInt*> int_const_map_;
    std::unordered_map<float , ConstantFloat*> float_const_map_;
    std::unordered_map<Type*, NULLPointer*> NULLPointer_map_;
    std::vector<std::unique_ptr<GlobalVariable>> globals;
    std::vector<std::unique_ptr<Function>> functions;  
    std::unordered_map<std::string, GlobalVariable*> global_map_;
    std::unordered_map<std::string, Function*> function_map_;
    
};

Instruction* make_inst(BasicBlock* bb, Opcode op, Type* type,
                       const std::string& name,
                       const std::vector<Value*>& operands = {});


} //namespace IR